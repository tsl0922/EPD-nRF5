using System.Text.Json;
using System.Text.Json.Serialization;
using EpdHub.Render;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpdHub.Service;

/// <summary>Persists the current DisplayContent as JSON in the data directory.</summary>
public sealed class ContentStore
{
    private static readonly JsonSerializerOptions Json = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    private readonly object _gate = new();
    private readonly ILogger<ContentStore> _log;
    private DisplayContent? _cached;

    public string DataDir { get; }
    public string ContentPath => Path.Combine(DataDir, "content.json");
    public string PreviewPath => Path.Combine(DataDir, "preview.png");
    public string StatePath => Path.Combine(DataDir, "state.json");
    public string ImagePath => Path.Combine(DataDir, "image.png");
    public DisplaySpec Spec { get; } = new();

    public ContentStore(IOptions<HubOptions> opt, ILogger<ContentStore> log)
    {
        _log = log;
        DataDir = opt.Value.ResolvedDataDir;
        Directory.CreateDirectory(DataDir);
    }

    public DisplayContent Load()
    {
        lock (_gate)
        {
            if (_cached is not null) return _cached;
            if (File.Exists(ContentPath))
            {
                try
                {
                    _cached = JsonSerializer.Deserialize<DisplayContent>(File.ReadAllText(ContentPath), Json);
                }
                catch (Exception ex)
                {
                    _log.LogWarning(ex, "content.json unreadable, using welcome screen");
                }
            }
            _cached ??= DisplayContent.Welcome();
            return _cached.Normalize(Spec);
        }
    }

    public sealed class PushState
    {
        public string? LastPushHash { get; set; }
        public DateTimeOffset? LastPushAt { get; set; }
        public int PushCount { get; set; }
        public int? BatteryMv { get; set; }
        public DateTimeOffset? BatteryReadAt { get; set; }
    }

    public PushState LoadState()
    {
        try
        {
            if (File.Exists(StatePath))
                return JsonSerializer.Deserialize<PushState>(File.ReadAllText(StatePath), Json) ?? new PushState();
        }
        catch (Exception ex) { _log.LogWarning(ex, "state.json unreadable"); }
        return new PushState();
    }

    public void SaveState(PushState state)
    {
        try { File.WriteAllText(StatePath, JsonSerializer.Serialize(state, Json)); }
        catch (Exception ex) { _log.LogWarning(ex, "state.json not written"); }
    }

    public void Save(DisplayContent content)
    {
        lock (_gate)
        {
            content.Normalize(Spec);
            _cached = content;
            File.WriteAllText(ContentPath, JsonSerializer.Serialize(content, Json));
        }
    }
}
