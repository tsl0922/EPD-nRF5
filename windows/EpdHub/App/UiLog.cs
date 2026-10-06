using System.Collections.Concurrent;
using Microsoft.Extensions.Logging;

namespace EpdHub.App;

/// <summary>
/// Logger provider that keeps the last few hundred lines in memory (for the GUI), appends them to a
/// file in the data directory and raises an event for live views.
/// </summary>
public sealed class UiLog : ILoggerProvider
{
    public static readonly UiLog Instance = new();
    private const int MaxLines = 500;

    private readonly ConcurrentQueue<string> _lines = new();
    private readonly object _fileGate = new();
    private string? _file;

    public event Action<string>? LineAdded;

    public string[] Lines => _lines.ToArray();

    public void SetFile(string path)
    {
        lock (_fileGate)
        {
            _file = path;
            try
            {
                if (File.Exists(path) && new FileInfo(path).Length > 2_000_000)
                    File.Move(path, Path.ChangeExtension(path, ".old.log"), overwrite: true);
            }
            catch { /* best effort */ }
        }
    }

    public ILogger CreateLogger(string categoryName) => new Logger(this, categoryName);

    public void Dispose() { }

    internal void Add(LogLevel level, string category, string message, Exception? ex)
    {
        string cat = category.Contains('.') ? category[(category.LastIndexOf('.') + 1)..] : category;
        string line = $"{DateTime.Now:HH:mm:ss} [{Short(level)}] {cat}: {message}";
        if (ex is not null) line += " | " + ex.GetType().Name + ": " + ex.Message;

        _lines.Enqueue(line);
        while (_lines.Count > MaxLines && _lines.TryDequeue(out _)) { }

        lock (_fileGate)
        {
            if (_file is not null)
            {
                try { File.AppendAllText(_file, line + Environment.NewLine); } catch { }
            }
        }
        try { LineAdded?.Invoke(line); } catch { }
    }

    private static string Short(LogLevel l) => l switch
    {
        LogLevel.Trace => "trc", LogLevel.Debug => "dbg", LogLevel.Information => "inf",
        LogLevel.Warning => "WRN", LogLevel.Error => "ERR", LogLevel.Critical => "CRIT", _ => "?",
    };

    private sealed class Logger : ILogger
    {
        private readonly UiLog _owner;
        private readonly string _category;
        public Logger(UiLog owner, string category) { _owner = owner; _category = category; }

        public IDisposable BeginScope<TState>(TState state) where TState : notnull => NullScope.Instance;
        public bool IsEnabled(LogLevel logLevel) => logLevel >= LogLevel.Information;

        public void Log<TState>(LogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
        {
            if (!IsEnabled(logLevel)) return;
            // The MCP SDK logs every request at Information level; keep those out of the UI.
            if (_category.StartsWith("ModelContextProtocol", StringComparison.Ordinal) && logLevel < LogLevel.Warning) return;
            if (_category.StartsWith("Microsoft.", StringComparison.Ordinal) && logLevel < LogLevel.Warning) return;
            _owner.Add(logLevel, _category, formatter(state, exception), exception);
        }

        private sealed class NullScope : IDisposable
        {
            public static readonly NullScope Instance = new();
            public void Dispose() { }
        }
    }
}
