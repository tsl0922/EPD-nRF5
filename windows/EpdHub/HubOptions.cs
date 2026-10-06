namespace EpdHub;

public sealed class HubOptions
{
    public const string Section = "Hub";

    /// <summary>BLE MAC of the tag, e.g. "F8:71:B0:51:F3:D2". Empty = scan by name prefix.</summary>
    public string DeviceAddress { get; set; } = "";

    /// <summary>Advertised name prefix used when scanning.</summary>
    public string DeviceNamePrefix { get; set; } = "NRF_EPD";

    /// <summary>EPD model id to send with INIT. 0 = use the id stored on the device.</summary>
    public int ModelId { get; set; } = 0;

    /// <summary>How often the scheduler re-renders and (if changed) pushes.</summary>
    public int UpdateIntervalMinutes { get; set; } = 10;

    /// <summary>Never push twice within this window, even when forced.</summary>
    public int MinPushIntervalSeconds { get; set; } = 60;

    /// <summary>Time to wait after REFRESH before the panel is considered idle.</summary>
    public int RefreshSettleSeconds { get; set; } = 25;

    /// <summary>The tag advertises once per second and Windows only listens part of the time; discovery took 4..25 s in practice.</summary>
    public int ScanTimeoutSeconds { get; set; } = 60;

    /// <summary>Connection attempts per push before giving up.</summary>
    public int PushAttempts { get; set; } = 3;

    /// <summary>
    /// When the tag has not been contacted for this long, connect and check it: battery, display mode and
    /// uptime. A reboot (calendar or blank screen) is repaired with a forced push. 0 disables.
    /// </summary>
    public int HealthCheckMinutes { get; set; } = 60;

    /// <summary>Raise a tray warning after this many consecutive failed contacts.</summary>
    public int AlertAfterFailures { get; set; } = 3;

    /// <summary>Send one write-with-response after this many write-without-response packets.</summary>
    public int WritesPerAck { get; set; } = 4;

    /// <summary>Where content.json and preview.png live. Empty = %LOCALAPPDATA%\EpdHub.</summary>
    public string DataDir { get; set; } = "";

    public string McpUrl { get; set; } = "http://127.0.0.1:5077";

    public string ResolvedDataDir =>
        string.IsNullOrWhiteSpace(DataDir)
            ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EpdHub")
            : DataDir;
}
