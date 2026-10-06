using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Threading.Channels;
using EpdHub.Ble;
using EpdHub.Render;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpdHub.Service;

public sealed record PushResult(bool Pushed, string Message, string FrameHash, TimeSpan Elapsed);

public sealed class DisplayStatus
{
    public string DeviceAddress { get; set; } = "";
    public string? DeviceFirmware { get; set; }
    public EpdConfig? DeviceConfig { get; set; }
    public DateTimeOffset? LastPushAt { get; set; }
    public string? LastPushHash { get; set; }
    public string? LastResult { get; set; }
    public string? LastError { get; set; }
    public DateTimeOffset? LastRenderAt { get; set; }
    public string? CurrentFrameHash { get; set; }
    public string ContentMode { get; set; } = DisplayContent.ModeTemplate;
    public int? BatteryMv { get; set; }
    public int? BatteryPercent { get; set; }
    public DateTimeOffset? BatteryReadAt { get; set; }
    public List<string> RenderWarnings { get; set; } = new();
    public DateTimeOffset? LastContactAt { get; set; }
    public int ConsecutiveFailures { get; set; }
    public int? DeviceDisplayMode { get; set; }
    public TimeSpan? DeviceUptime { get; set; }
    public DateTimeOffset? LastHealthCheckAt { get; set; }
    public string? LastHealthResult { get; set; }
    public int PushCount { get; set; }
    public bool Busy { get; set; }
    public string PreviewPath { get; set; } = "";
    public int UpdateIntervalMinutes { get; set; }
}

/// <summary>
/// Owns the render → diff → push pipeline and the periodic scheduler.
/// All device access is serialized through the push lock (see <see cref="RunExclusiveAsync"/>).
/// </summary>
public sealed class DisplayService : BackgroundService
{
    private readonly HubOptions _opt;
    private readonly ContentStore _store;
    private readonly ILogger<DisplayService> _log;
    private readonly ILoggerFactory _loggerFactory;
    private readonly SemaphoreSlim _pushLock = new(1, 1);
    private readonly Channel<bool> _requests = Channel.CreateBounded<bool>(new BoundedChannelOptions(4) { FullMode = BoundedChannelFullMode.DropOldest });

    public DisplayStatus Status { get; } = new();
    public DisplaySpec Spec => _store.Spec;
    public HubOptions Options => _opt;

    /// <summary>Raised after every push attempt (success or failure). Not on the UI thread.</summary>
    public event Action<PushResult>? PushCompleted;
    /// <summary>Raised when the tag became unreachable (true) after AlertAfterFailures attempts, or reachable again (false).</summary>
    public event Action<bool, string>? ConnectivityChanged;

    public DisplayService(IOptions<HubOptions> opt, ContentStore store, ILogger<DisplayService> log, ILoggerFactory lf)
    {
        _opt = opt.Value;
        _store = store;
        _log = log;
        _loggerFactory = lf;
        Status.DeviceAddress = _opt.DeviceAddress;
        Status.PreviewPath = store.PreviewPath;
        Status.UpdateIntervalMinutes = _opt.UpdateIntervalMinutes;
        var state = store.LoadState();
        Status.LastPushHash = state.LastPushHash;
        Status.LastPushAt = state.LastPushAt;
        Status.LastContactAt = state.LastPushAt;
        Status.PushCount = state.PushCount;
        if (state.BatteryMv is { } mv)
        {
            Status.BatteryMv = mv;
            Status.BatteryPercent = BatteryPercentFromMv(mv);
            Status.BatteryReadAt = state.BatteryReadAt;
        }
    }

    /// <summary>Ask the scheduler loop to push soon (returns immediately).</summary>
    public void RequestPush(bool force) => _requests.Writer.TryWrite(force);

    /// <summary>Battery for rendering: percent rounded to 10 % steps, so normal jitter does not alter the frame.</summary>
    public SceneContext Context => new(Status.BatteryMv, Status.BatteryPercent is { } p ? (int)Math.Round(p / 10.0) * 10 : null);

    /// <summary>Render the current content (scene, template or stored image) to planes and write preview.png.</summary>
    public Frame RenderCurrent() => RenderCurrent(null);

    public Frame RenderCurrent(List<string>? warnings)
    {
        var content = _store.Load();
        warnings ??= new List<string>();
        Frame frame;
        if (content.Mode == DisplayContent.ModeImage && content.ImagePath is not null && File.Exists(content.ImagePath))
        {
            using var bmp = LoadBitmap(content.ImagePath);
            frame = FrameRenderer.ToPlanes(bmp);
        }
        else if (content.Mode == DisplayContent.ModeScene && !string.IsNullOrWhiteSpace(content.SceneJson))
        {
            Scene scene;
            try { scene = Scene.Parse(content.SceneJson); }
            catch (Exception ex)
            {
                warnings.Add("scene JSON invalid: " + ex.Message);
                scene = new Scene { Elements = { new SceneElement { Type = "text", X = 12, Y = 12, W = 376, Wrap = true, Text = "场景 JSON 无效: " + ex.Message } } };
            }
            using var bmp = SceneRenderer.Render(scene, _store.Spec, Context, warnings);
            frame = FrameRenderer.ToPlanes(bmp);
            foreach (var w in warnings) _log.LogWarning("scene: {Warning}", w);
        }
        else
        {
            using var bmp = FrameRenderer.Render(content, _store.Spec, null, Context);
            frame = FrameRenderer.ToPlanes(bmp);
        }
        Status.RenderWarnings = warnings;
        try
        {
            using var preview = FrameRenderer.FromPlanes(frame);
            preview.Save(_store.PreviewPath, ImageFormat.Png);
        }
        catch (Exception ex) { _log.LogWarning(ex, "preview.png not written"); }
        Status.LastRenderAt = DateTimeOffset.Now;
        Status.CurrentFrameHash = frame.Hash;
        Status.ContentMode = content.Mode;
        return frame;
    }

    /// <summary>Render a scene to scene-preview.png without touching the stored content.</summary>
    public (string Hash, string Path) RenderScenePreview(Scene scene, List<string> warnings)
    {
        using var bmp = SceneRenderer.Render(scene, _store.Spec, Context, warnings);
        var frame = FrameRenderer.ToPlanes(bmp);
        string path = Path.Combine(_store.DataDir, "scene-preview.png");
        using var preview = FrameRenderer.FromPlanes(frame);
        preview.Save(path, ImageFormat.Png);
        return (frame.Hash, path);
    }

    /// <summary>Battery curve from the firmware (GUI.c batt_cal), voltage in mV -> percent.</summary>
    public static int BatteryPercentFromMv(int mv)
    {
        int adc = mv * 2047 / 3600;
        if (adc > 1705) return 100;
        if (adc > 1584) return 28 + (int)((long)(adc - 1584) * 72 / (1705 - 1584));
        if (adc > 1360) return 4 + (int)((long)(adc - 1360) * 24 / (1584 - 1360));
        if (adc > 1136) return (int)((long)(adc - 1136) * 4 / (1360 - 1136));
        return 0;
    }

    private void NoteContactOk()
    {
        Status.LastContactAt = DateTimeOffset.Now;
        if (Status.ConsecutiveFailures >= Math.Max(1, _opt.AlertAfterFailures))
        {
            _log.LogInformation("Tag reachable again after {N} failed attempts", Status.ConsecutiveFailures);
            ConnectivityChanged?.Invoke(false, $"墨水屏已恢复联系（此前连续 {Status.ConsecutiveFailures} 次失败）");
        }
        Status.ConsecutiveFailures = 0;
    }

    private void NoteContactFailed(string reason)
    {
        Status.ConsecutiveFailures++;
        int limit = Math.Max(1, _opt.AlertAfterFailures);
        if (Status.ConsecutiveFailures == limit)
        {
            _log.LogError("Tag unreachable {N} times in a row: {Reason}", Status.ConsecutiveFailures, reason);
            ConnectivityChanged?.Invoke(true, $"已连续 {Status.ConsecutiveFailures} 次联系不上墨水屏：{reason}");
        }
    }

    /// <summary>
    /// Connect without pushing: battery, display mode, uptime. Returns a reason when the panel needs a
    /// forced push (the tag rebooted, or left picture mode), otherwise null.
    /// </summary>
    public async Task<string?> HealthCheckAsync(CancellationToken ct)
    {
        string? needPush = null;
        try
        {
            await RunExclusiveAsync(async token =>
            {
                ulong addr = string.IsNullOrWhiteSpace(_opt.DeviceAddress) ? 0 : EpdClient.ParseAddress(_opt.DeviceAddress);
                await using var client = new EpdClient(_loggerFactory.CreateLogger<EpdClient>());
                await client.ConnectAsync(addr, _opt.DeviceNamePrefix, TimeSpan.FromSeconds(_opt.ScanTimeoutSeconds), token);
                Status.DeviceFirmware = $"0x{client.FirmwareVersion:x2}";
                Status.DeviceConfig = client.Config;
                await client.InitAsync(_opt.ModelId, token);
                Status.DeviceUptime = client.Uptime;
                var info = await client.GetInfoAsync(token);
                RecordInfo(info);
                if (info is not null) Status.DeviceDisplayMode = info.DisplayMode;

                if (info is not null && info.DisplayMode != 0)
                    needPush = $"display mode is {info.DisplayMode} (calendar), tag was reset";
                else if (client.Uptime is { } up && Status.LastPushAt is { } last && up < DateTimeOffset.Now - last)
                    needPush = $"tag rebooted {up.TotalMinutes:F0} min ago (after the last push), screen is blank";
            }, ct);
            NoteContactOk();
            Status.LastHealthCheckAt = DateTimeOffset.Now;
            Status.LastHealthResult = needPush ?? "ok";
            _log.LogInformation("Health check: battery {Mv} mV, mode {Mode}, uptime {Up}, {Result}",
                Status.BatteryMv, Status.DeviceDisplayMode, Status.DeviceUptime, needPush ?? "ok");
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested) { throw; }
        catch (Exception ex)
        {
            Status.LastHealthCheckAt = DateTimeOffset.Now;
            Status.LastHealthResult = "failed: " + ex.Message;
            _log.LogWarning("Health check failed: {Msg}", ex.Message);
            NoteContactFailed(ex.Message);
        }
        return needPush;
    }

    private void RecordInfo(EpdClient.DeviceInfo? info)
    {
        if (info is null || info.VoltageMv <= 0) return;
        Status.BatteryMv = info.VoltageMv;
        Status.BatteryPercent = BatteryPercentFromMv(info.VoltageMv);
        Status.BatteryReadAt = DateTimeOffset.Now;
        var st = _store.LoadState();
        st.BatteryMv = Status.BatteryMv;
        st.BatteryReadAt = Status.BatteryReadAt;
        _store.SaveState(st);
    }

    private static Bitmap LoadBitmap(string path)
    {
        using var fs = File.OpenRead(path);
        using var img = Image.FromStream(fs);
        var bmp = new Bitmap(img.Width, img.Height, PixelFormat.Format32bppArgb);
        using var g = Graphics.FromImage(bmp);
        g.DrawImageUnscaled(img, 0, 0);
        return bmp;
    }

    /// <summary>Store a processed (already 400x300, palette-only) picture and switch the display to image mode.</summary>
    public async Task<PushResult> SetImageAsync(Bitmap processed, string source, bool pushNow, CancellationToken ct)
    {
        using (var copy = new Bitmap(processed))
            copy.Save(_store.ImagePath, ImageFormat.Png);
        var content = _store.Load();
        content.Mode = DisplayContent.ModeImage;
        content.ImagePath = _store.ImagePath;
        content.UpdatedAt = DateTimeOffset.Now;
        content.Source = source;
        _store.Save(content);
        if (!pushNow)
        {
            var frame = RenderCurrent();
            return new PushResult(false, "saved, will push on next schedule", frame.Hash, TimeSpan.Zero);
        }
        return await PushAsync(force: false, ct);
    }

    /// <summary>Run device work (e.g. a firmware update) while holding the push lock.</summary>
    public async Task RunExclusiveAsync(Func<CancellationToken, Task> work, CancellationToken ct)
    {
        await _pushLock.WaitAsync(ct);
        Status.Busy = true;
        try { await work(ct); }
        finally { Status.Busy = false; _pushLock.Release(); }
    }

    /// <summary>Render, compare with the last pushed frame and push over BLE when needed.</summary>
    public async Task<PushResult> PushAsync(bool force, CancellationToken ct)
    {
        var sw = Stopwatch.StartNew();
        await _pushLock.WaitAsync(ct);
        Status.Busy = true;
        PushResult result;
        try
        {
            var frame = RenderCurrent();

            if (!force && frame.Hash == Status.LastPushHash)
                return new PushResult(false, "unchanged", frame.Hash, sw.Elapsed);

            if (Status.LastPushAt is { } last &&
                DateTimeOffset.Now - last < TimeSpan.FromSeconds(_opt.MinPushIntervalSeconds))
            {
                var wait = TimeSpan.FromSeconds(_opt.MinPushIntervalSeconds) - (DateTimeOffset.Now - last);
                _log.LogInformation("Rate limit: waiting {Wait:F0}s before next push", wait.TotalSeconds);
                await Task.Delay(wait, ct);
            }

            Exception? lastEx = null;
            int attempts = Math.Max(1, _opt.PushAttempts);
            for (int attempt = 1; attempt <= attempts; attempt++)
            {
                try
                {
                    await PushFrameAsync(frame, ct);
                    Status.LastPushAt = DateTimeOffset.Now;
                    Status.LastPushHash = frame.Hash;
                    Status.PushCount++;
                    Status.LastError = null;
                    NoteContactOk();
                    Status.LastResult = $"pushed in {sw.Elapsed.TotalSeconds:F1}s";
                    _store.SaveState(new ContentStore.PushState
                    {
                        LastPushHash = frame.Hash, LastPushAt = Status.LastPushAt, PushCount = Status.PushCount,
                        BatteryMv = Status.BatteryMv, BatteryReadAt = Status.BatteryReadAt,
                    });
                    _log.LogInformation("Frame {Hash} pushed in {S:F1}s", frame.Hash, sw.Elapsed.TotalSeconds);
                    result = new PushResult(true, Status.LastResult, frame.Hash, sw.Elapsed);
                    return result;
                }
                catch (OperationCanceledException) when (ct.IsCancellationRequested)
                {
                    _log.LogWarning("Push cancelled by caller after {S:F0}s", sw.Elapsed.TotalSeconds);
                    throw;
                }
                catch (Exception ex)
                {
                    lastEx = ex;
                    _log.LogWarning("Push attempt {N}/{Total} failed at {S:F0}s: {Type} {Msg}", attempt, attempts, sw.Elapsed.TotalSeconds, ex.GetType().Name, ex.Message);
                    if (attempt < attempts) await Task.Delay(TimeSpan.FromSeconds(5), ct);
                }
            }
            Status.LastError = lastEx!.Message;
            Status.LastResult = "failed";
            NoteContactFailed(lastEx.Message);
            result = new PushResult(false, "failed: " + lastEx.Message, frame.Hash, sw.Elapsed);
            return result;
        }
        finally
        {
            Status.Busy = false;
            _pushLock.Release();
        }
    }

    private async Task PushFrameAsync(Frame frame, CancellationToken ct)
    {
        ulong addr = string.IsNullOrWhiteSpace(_opt.DeviceAddress) ? 0 : EpdClient.ParseAddress(_opt.DeviceAddress);
        await using var client = new EpdClient(_loggerFactory.CreateLogger<EpdClient>()) { WritesPerAck = _opt.WritesPerAck };

        await client.ConnectAsync(addr, _opt.DeviceNamePrefix, TimeSpan.FromSeconds(_opt.ScanTimeoutSeconds), ct);
        Status.DeviceFirmware = $"0x{client.FirmwareVersion:x2}";
        Status.DeviceConfig = client.Config;

        await client.InitAsync(_opt.ModelId, ct);
        Status.DeviceUptime = client.Uptime;
        RecordInfo(await client.GetInfoAsync(ct));
        await client.WritePlaneAsync(frame.Black, red: false, null, ct);
        await client.WritePlaneAsync(frame.Red, red: true, null, ct);
        await client.RefreshAsync();
        // Keep the link up while the panel refreshes; the firmware sleeps the panel on disconnect.
        _log.LogInformation("REFRESH sent, waiting {S}s for the panel", _opt.RefreshSettleSeconds);
        await Task.Delay(TimeSpan.FromSeconds(_opt.RefreshSettleSeconds), ct);
    }

    /// <summary>Connect only to read firmware version and config (used by the GUI).</summary>
    public async Task<(byte Version, EpdConfig? Config)> ReadDeviceInfoAsync(CancellationToken ct)
    {
        (byte, EpdConfig?) info = default;
        await RunExclusiveAsync(async token =>
        {
            ulong addr = string.IsNullOrWhiteSpace(_opt.DeviceAddress) ? 0 : EpdClient.ParseAddress(_opt.DeviceAddress);
            await using var client = new EpdClient(_loggerFactory.CreateLogger<EpdClient>());
            await client.ConnectAsync(addr, _opt.DeviceNamePrefix, TimeSpan.FromSeconds(_opt.ScanTimeoutSeconds), token);
            Status.DeviceFirmware = $"0x{client.FirmwareVersion:x2}";
            Status.DeviceConfig = client.Config;
            var di = await client.GetInfoAsync(token);
            RecordInfo(di);
            if (di is not null) Status.DeviceDisplayMode = di.DisplayMode;
            info = (client.FirmwareVersion, client.Config);
            NoteContactOk();
        }, ct);
        return info;
    }

    protected override async Task ExecuteAsync(CancellationToken ct)
    {
        _log.LogInformation("Scheduler started: every {Min} min, data dir {Dir}", _opt.UpdateIntervalMinutes, _store.DataDir);
        // First run: push only if the stored content differs from what was last pushed (state.json).
        RequestPush(force: false);

        var interval = TimeSpan.FromMinutes(Math.Max(1, _opt.UpdateIntervalMinutes));
        while (!ct.IsCancellationRequested)
        {
            bool force = false;
            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(ct);
            timeout.CancelAfter(interval);
            try { force = await _requests.Reader.ReadAsync(timeout.Token); }
            catch (OperationCanceledException) when (!ct.IsCancellationRequested) { /* periodic tick */ }

            try
            {
                var r = await PushAsync(force, ct);
                if (r.Pushed || r.Message.StartsWith("failed")) PushCompleted?.Invoke(r);

                if (_opt.HealthCheckMinutes > 0 && !r.Pushed)
                {
                    var silence = DateTimeOffset.Now - (Status.LastContactAt ?? Status.LastPushAt ?? DateTimeOffset.MinValue);
                    if (silence >= TimeSpan.FromMinutes(_opt.HealthCheckMinutes))
                    {
                        var reason = await HealthCheckAsync(ct);
                        if (reason is not null)
                        {
                            _log.LogWarning("Restoring the panel: {Reason}", reason);
                            var rr = await PushAsync(force: true, ct);
                            PushCompleted?.Invoke(rr);
                        }
                    }
                }
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested) { break; }
            catch (Exception ex) { _log.LogError(ex, "Scheduled push failed"); }
        }
    }
}
