using System.Text;
using System.Threading.Channels;
using Microsoft.Extensions.Logging;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Security.Cryptography;
using Windows.Storage.Streams;

namespace EpdHub.Ble;

/// <summary>Configuration struct pushed by the firmware when notifications are enabled (epd_config_t).</summary>
public sealed record EpdConfig(
    byte Mosi, byte Sclk, byte Cs, byte Dc, byte Rst, byte Busy, byte Bs,
    byte ModelId, byte WakeupPin, byte LedPin, byte EnPin, byte DisplayMode, byte WeekStart)
{
    public static EpdConfig? Parse(byte[] b) => b.Length < 13 ? null :
        new EpdConfig(b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12]);
}

public sealed record ScanResult(ulong Address, string Name, short Rssi)
{
    public string AddressText => EpdClient.FormatAddress(Address);
}

/// <summary>
/// Minimal client for the EPD-nRF5 BLE service (62750001-...). One instance = one connection.
/// </summary>
public sealed class EpdClient : IAsyncDisposable
{
    public static readonly Guid ServiceUuid = Guid.Parse("62750001-d828-918d-fb46-b6c11c675aec");
    public static readonly Guid DataUuid = Guid.Parse("62750002-d828-918d-fb46-b6c11c675aec");
    public static readonly Guid VersionUuid = Guid.Parse("62750003-d828-918d-fb46-b6c11c675aec");

    private const byte CmdInit = 0x01;
    private const byte CmdRefresh = 0x05;
    private const byte CmdWriteImage = 0x30;
    private const byte CmdGetInfo = 0x93;      // firmware >= 0x1b

    private readonly ILogger _log;
    private readonly Channel<byte[]> _notifications = Channel.CreateUnbounded<byte[]>();

    private BluetoothLEDevice? _device;
    private GattDeviceService? _service;
    private GattCharacteristic? _data;

    public EpdConfig? Config { get; private set; }
    public byte FirmwareVersion { get; private set; }
    /// <summary>Max payload per write as reported by the firmware ("mtu=NNN"), default 20.</summary>
    public int MaxDataLen { get; private set; } = 20;
    public bool RleSupported { get; private set; }
    /// <summary>Firmware clock as reported by INIT. It starts at 2025-01-01 00:00 UTC on every boot and is never
    /// synced in picture mode, so (value - 1735689600) is the uptime in seconds.</summary>
    public uint? DeviceTimestamp { get; private set; }
    public const uint FirmwareEpoch = 1735689600;
    public TimeSpan? Uptime => DeviceTimestamp is { } t && t >= FirmwareEpoch ? TimeSpan.FromSeconds(t - FirmwareEpoch) : null;
    public int WritesPerAck { get; set; } = 4;

    public EpdClient(ILogger log) => _log = log;

    public static ulong ParseAddress(string text) => Convert.ToUInt64(text.Replace(":", "").Replace("-", ""), 16);
    public static string FormatAddress(ulong a) => string.Join(":", Enumerable.Range(0, 6).Select(i => ((a >> (8 * (5 - i))) & 0xFF).ToString("X2")));

    // ---------------------------------------------------------------- scanning

    public static async Task<List<ScanResult>> ScanAsync(string namePrefix, TimeSpan duration, CancellationToken ct)
    {
        var found = new Dictionary<ulong, ScanResult>();
        var watcher = new BluetoothLEAdvertisementWatcher { ScanningMode = BluetoothLEScanningMode.Active };
        watcher.Received += (_, a) =>
        {
            string name = a.Advertisement.LocalName ?? "";
            if (name.StartsWith(namePrefix, StringComparison.OrdinalIgnoreCase))
                lock (found) found[a.BluetoothAddress] = new ScanResult(a.BluetoothAddress, name, a.RawSignalStrengthInDBm);
        };
        watcher.Start();
        try { await Task.Delay(duration, ct); }
        finally { watcher.Stop(); }
        lock (found) return found.Values.OrderByDescending(r => r.Rssi).ToList();
    }

    /// <summary>Wait until the given address (or any device with the name prefix, when address is 0) advertises.</summary>
    public static async Task<ulong> WaitForAdvertisementAsync(ulong address, string namePrefix, TimeSpan timeout, CancellationToken ct)
    {
        var sw = System.Diagnostics.Stopwatch.StartNew();
        var tcs = new TaskCompletionSource<ulong>(TaskCreationOptions.RunContinuationsAsynchronously);
        // By address we only need the ADV_IND packet itself: passive scanning skips the scan-request/response
        // exchange, which is one more thing to lose under 2.4 GHz interference. Names may live in the scan
        // response, so name-prefix discovery stays active.
        var watcher = new BluetoothLEAdvertisementWatcher
        {
            ScanningMode = address != 0 ? BluetoothLEScanningMode.Passive : BluetoothLEScanningMode.Active,
        };
        watcher.Received += (_, a) =>
        {
            bool match = address != 0
                ? a.BluetoothAddress == address
                : (a.Advertisement.LocalName ?? "").StartsWith(namePrefix, StringComparison.OrdinalIgnoreCase);
            if (match) tcs.TrySetResult(a.BluetoothAddress);
        };
        watcher.Start();
        try
        {
            using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
            cts.CancelAfter(timeout);
            using var reg = cts.Token.Register(() => tcs.TrySetCanceled());
            try { return await tcs.Task; }
            catch (OperationCanceledException) when (!ct.IsCancellationRequested)
            {
                throw new TimeoutException(
                    $"{(address != 0 ? FormatAddress(address) : namePrefix + "*")} not seen advertising in {sw.Elapsed.TotalSeconds:F0}s " +
                    "(tag connected to another phone/PC/browser tab, out of range, or 2.4 GHz interference)");
            }
        }
        finally { watcher.Stop(); }
    }

    // ---------------------------------------------------------------- connection

    public async Task ConnectAsync(ulong address, string namePrefix, TimeSpan scanTimeout, CancellationToken ct)
    {
        // WinRT can only connect to an address it has recently seen advertising, so scan first.
        _log.LogInformation("Scanning for {Addr} (up to {S}s)...", address != 0 ? FormatAddress(address) : namePrefix + "*", scanTimeout.TotalSeconds);
        var scanSw = System.Diagnostics.Stopwatch.StartNew();
        address = await WaitForAdvertisementAsync(address, namePrefix, scanTimeout, ct);
        _log.LogInformation("Advertisement seen after {S:F1}s", scanSw.Elapsed.TotalSeconds);

        _device = await BluetoothLEDevice.FromBluetoothAddressAsync(address)
                  ?? throw new InvalidOperationException($"Device {FormatAddress(address)} not found");

        var svc = await _device.GetGattServicesForUuidAsync(ServiceUuid, BluetoothCacheMode.Uncached);
        if (svc.Status != GattCommunicationStatus.Success || svc.Services.Count == 0)
            throw new InvalidOperationException($"EPD service not found (status {svc.Status})");
        _service = svc.Services[0];

        var chars = await _service.GetCharacteristicsForUuidAsync(DataUuid, BluetoothCacheMode.Uncached);
        if (chars.Status != GattCommunicationStatus.Success || chars.Characteristics.Count == 0)
            throw new InvalidOperationException("EPD data characteristic not found");
        _data = chars.Characteristics[0];
        _data.ValueChanged += OnValueChanged;

        var cccd = await _data.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue.Notify);
        if (cccd != GattCommunicationStatus.Success)
            throw new InvalidOperationException($"Enable notifications failed: {cccd}");

        // Firmware pushes its config struct right after notifications are enabled.
        var cfg = await WaitNotificationAsync(TimeSpan.FromSeconds(5), ct);
        Config = cfg is null ? null : EpdConfig.Parse(cfg);

        var ver = await _service.GetCharacteristicsForUuidAsync(VersionUuid, BluetoothCacheMode.Uncached);
        if (ver.Status == GattCommunicationStatus.Success && ver.Characteristics.Count > 0)
        {
            var r = await ver.Characteristics[0].ReadValueAsync(BluetoothCacheMode.Uncached);
            if (r.Status == GattCommunicationStatus.Success)
            {
                CryptographicBuffer.CopyToByteArray(r.Value, out var vb);
                if (vb.Length > 0) FirmwareVersion = vb[0];
            }
        }

        _log.LogInformation("Connected to {Name} fw=0x{Ver:x2} model=0x{Model:x2} mode={Mode}",
            _device.Name, FirmwareVersion, Config?.ModelId ?? 0, Config?.DisplayMode ?? 0);
    }

    private void OnValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        CryptographicBuffer.CopyToByteArray(args.CharacteristicValue, out var bytes);
        _notifications.Writer.TryWrite(bytes);
    }

    private async Task<byte[]?> WaitNotificationAsync(TimeSpan timeout, CancellationToken ct)
    {
        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        cts.CancelAfter(timeout);
        try { return await _notifications.Reader.ReadAsync(cts.Token); }
        catch (OperationCanceledException) when (!ct.IsCancellationRequested) { return null; }
    }

    // ---------------------------------------------------------------- commands

    private async Task WriteAsync(byte[] payload, bool withResponse)
    {
        if (_data is null) throw new InvalidOperationException("not connected");
        var buf = CryptographicBuffer.CreateFromByteArray(payload);
        var opt = withResponse ? GattWriteOption.WriteWithResponse : GattWriteOption.WriteWithoutResponse;
        GattCommunicationStatus st;
        try { st = await _data.WriteValueAsync(buf, opt); }
        catch (OperationCanceledException ex)
        {
            // WinRT reports a dropped/blocked link as a cancelled async op; make it a normal retryable error.
            throw new IOException($"BLE write cancelled by the stack (cmd 0x{payload[0]:x2}, {(withResponse ? "with" : "without")} response): {ex.Message}", ex);
        }
        if (st != GattCommunicationStatus.Success)
            throw new InvalidOperationException($"BLE write failed: {st} (cmd 0x{payload[0]:x2})");
    }

    /// <summary>INIT: select the panel driver (0 = the id stored on the device). Firmware replies with "mtu=N rle=1" and "t=...".</summary>
    public async Task InitAsync(int modelId, CancellationToken ct)
    {
        await WriteAsync(modelId > 0 ? new[] { CmdInit, (byte)modelId } : new[] { CmdInit }, true);

        for (int i = 0; i < 2; i++)
        {
            var msg = await WaitNotificationAsync(TimeSpan.FromSeconds(3), ct);
            if (msg is null) break;
            string text = Encoding.ASCII.GetString(msg);
            if (text.StartsWith("t=") && uint.TryParse(text.AsSpan(2), out uint ts)) DeviceTimestamp = ts;
            if (text.StartsWith("mtu="))
            {
                var parts = text.Split(' ');
                if (int.TryParse(parts[0].AsSpan(4), out int mtu) && mtu >= 20) MaxDataLen = mtu;
                RleSupported = text.Contains("rle=1");
            }
        }
        _log.LogInformation("INIT ok, max data len {Len}, rle={Rle}", MaxDataLen, RleSupported);
    }

    /// <summary>Write one bit plane (black or red) into the panel RAM.</summary>
    public async Task WritePlaneAsync(byte[] plane, bool red, IProgress<(int sent, int total)>? progress, CancellationToken ct)
    {
        int chunkSize = MaxDataLen - 2;              // cmd + flags
        List<byte[]> chunks;
        bool useRle = false;
        if (RleSupported)
        {
            var rle = Rle.CompressChunked(plane, chunkSize);
            int rleLen = rle.Sum(c => c.Length);
            if (rleLen < plane.Length) { chunks = rle; useRle = true; }
            else chunks = Split(plane, chunkSize);
        }
        else chunks = Split(plane, chunkSize);

        int noReply = WritesPerAck;
        for (int i = 0; i < chunks.Count; i++)
        {
            ct.ThrowIfCancellationRequested();
            byte flags = (byte)((red ? 0x01 : 0x00) | (i == 0 ? 0x02 : 0x00) | (useRle ? 0x04 : 0x00));
            var payload = new byte[chunks[i].Length + 2];
            payload[0] = CmdWriteImage;
            payload[1] = flags;
            System.Buffer.BlockCopy(chunks[i], 0, payload, 2, chunks[i].Length);

            bool ack = noReply <= 0 || i == chunks.Count - 1;
            await WriteAsync(payload, ack);
            noReply = ack ? WritesPerAck : noReply - 1;
            progress?.Report((i + 1, chunks.Count));
        }
        _log.LogInformation("{Plane} plane: {Bytes} bytes in {N} packets (rle={Rle})",
            red ? "red" : "black", plane.Length, chunks.Count, useRle);
    }

    private static List<byte[]> Split(byte[] data, int size)
    {
        var list = new List<byte[]>((data.Length + size - 1) / size);
        for (int off = 0; off < data.Length; off += size)
            list.Add(data[off..Math.Min(off + size, data.Length)]);
        return list;
    }

    public sealed record DeviceInfo(int VoltageMv, int DisplayMode);

    /// <summary>GET_INFO (firmware 0x1b+): battery voltage in mV and display mode. Null on old firmware.</summary>
    public async Task<DeviceInfo?> GetInfoAsync(CancellationToken ct)
    {
        try { await WriteAsync(new[] { CmdGetInfo }, true); }
        catch (Exception ex) { _log.LogWarning("GET_INFO write failed: {Msg}", ex.Message); return null; }
        var deadline = DateTime.UtcNow.AddSeconds(3);
        while (DateTime.UtcNow < deadline)
        {
            var msg = await WaitNotificationAsync(deadline - DateTime.UtcNow, ct);
            if (msg is null) break;
            string text = Encoding.ASCII.GetString(msg);
            if (!text.StartsWith("v=")) continue;
            int mv = 0, mode = 0;
            foreach (var part in text.Split(' '))
            {
                if (part.StartsWith("v=")) int.TryParse(part.AsSpan(2), out mv);
                else if (part.StartsWith("m=")) int.TryParse(part.AsSpan(2), out mode);
            }
            _log.LogInformation("Device info: {Mv} mV, mode {Mode}", mv, mode);
            return new DeviceInfo(mv, mode);
        }
        _log.LogInformation("GET_INFO not answered (firmware without 0x93?)");
        return null;
    }

    /// <summary>REFRESH: display the RAM contents. The panel is busy for 2..25 s afterwards depending on type.</summary>
    public Task RefreshAsync() => WriteAsync(new[] { CmdRefresh }, true);

    public async ValueTask DisposeAsync()
    {
        if (_data is not null)
        {
            _data.ValueChanged -= OnValueChanged;
            try
            {
                await _data.WriteClientCharacteristicConfigurationDescriptorAsync(
                    GattClientCharacteristicConfigurationDescriptorValue.None);
            }
            catch { /* device may already be gone */ }
        }
        _service?.Dispose();
        _device?.Dispose();
        _data = null; _service = null; _device = null;
        // WinRT keeps the BLE link up while any GATT object is alive. Drop stray RCWs now so the tag
        // goes back to advertising; otherwise it stays connected to this process indefinitely.
        GC.Collect();
        GC.WaitForPendingFinalizers();
        await Task.Delay(500);
    }
}
