using System.IO.Compression;
using System.Text.Json;
using System.Threading.Channels;
using Microsoft.Extensions.Logging;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Security.Cryptography;

namespace EpdHub.Ble;

/// <summary>
/// Nordic Secure DFU over BLE: buttonless entry from the application, then object transfer to the
/// bootloader ("DfuTarg", advertising on MAC+1). Port of the Python bleak script used for the first upgrade.
/// </summary>
public sealed class DfuClient
{
    public sealed record Progress(string Stage, long Done, long Total);

    private static readonly Guid ButtonlessUuid = Guid.Parse("8ec90003-f315-4f60-9fb8-838830daea50");
    private static readonly Guid DfuServiceUuid = Guid.Parse("0000fe59-0000-1000-8000-00805f9b34fb");
    private static readonly Guid ControlPointUuid = Guid.Parse("8ec90001-f315-4f60-9fb8-838830daea50");
    private static readonly Guid PacketUuid = Guid.Parse("8ec90002-f315-4f60-9fb8-838830daea50");

    private const byte OpCreate = 0x01, OpSetPrn = 0x02, OpCalcCrc = 0x03, OpExecute = 0x04, OpSelect = 0x06, OpResponse = 0x60;
    private static readonly Dictionary<byte, string> ResultNames = new()
    {
        [0x01] = "SUCCESS", [0x02] = "OP_NOT_SUPPORTED", [0x03] = "INVALID_PARAM", [0x04] = "INSUFFICIENT_RESOURCES",
        [0x05] = "INVALID_OBJECT", [0x07] = "UNSUPPORTED_TYPE", [0x08] = "OP_NOT_PERMITTED", [0x0A] = "OPERATION_FAILED", [0x0B] = "EXT_ERROR",
    };

    private readonly ILogger _log;
    private readonly Channel<byte[]> _cpNotifications = Channel.CreateUnbounded<byte[]>();
    private GattCharacteristic? _cp, _pkt;
    private int _chunk = 20;

    public DfuClient(ILogger log) => _log = log;

    // ---------------------------------------------------------------- package

    public static (byte[] Init, byte[] Firmware, string BinName) LoadPackage(string zipPath)
    {
        using var zip = ZipFile.OpenRead(zipPath);
        var manifestEntry = zip.GetEntry("manifest.json") ?? throw new InvalidDataException("manifest.json missing in package");
        using var doc = JsonDocument.Parse(ReadAll(manifestEntry));
        var app = doc.RootElement.GetProperty("manifest").GetProperty("application");
        string bin = app.GetProperty("bin_file").GetString()!;
        string dat = app.GetProperty("dat_file").GetString()!;
        byte[] fw = ReadAll(zip.GetEntry(bin) ?? throw new InvalidDataException(bin + " missing"));
        byte[] init = ReadAll(zip.GetEntry(dat) ?? throw new InvalidDataException(dat + " missing"));
        return (init, fw, bin);
    }

    private static byte[] ReadAll(ZipArchiveEntry e)
    {
        using var s = e.Open();
        using var ms = new MemoryStream();
        s.CopyTo(ms);
        return ms.ToArray();
    }

    // ---------------------------------------------------------------- flow

    /// <summary>Run a full update. Returns the application version read back after the reboot, or null.</summary>
    public async Task<byte?> UpdateAsync(ulong appAddress, string namePrefix, string zipPath, IProgress<Progress>? progress, CancellationToken ct)
    {
        var (init, fw, binName) = LoadPackage(zipPath);
        _log.LogInformation("DFU package {Bin}: init {I} B, firmware {F} B", binName, init.Length, fw.Length);

        progress?.Report(new("连接设备，进入 bootloader", 0, 1));
        if (appAddress == 0)
            appAddress = await EpdClient.WaitForAdvertisementAsync(0, namePrefix, TimeSpan.FromSeconds(20), ct);

        bool entered = false;
        try
        {
            entered = await EnterBootloaderAsync(appAddress, ct);
        }
        catch (Exception ex)
        {
            _log.LogWarning("Buttonless entry failed ({Msg}); checking whether the device is already in DFU mode", ex.Message);
        }
        if (entered) await Task.Delay(2000, ct);

        progress?.Report(new("查找 DfuTarg", 0, 1));
        ulong target = await FindBootloaderAsync(appAddress, TimeSpan.FromSeconds(40), ct);

        using (var device = await BluetoothLEDevice.FromBluetoothAddressAsync(target)
                            ?? throw new InvalidOperationException("bootloader device not found"))
        {
            var svc = await device.GetGattServicesForUuidAsync(DfuServiceUuid, BluetoothCacheMode.Uncached);
            if (svc.Status != GattCommunicationStatus.Success || svc.Services.Count == 0)
                throw new InvalidOperationException("DFU service not found on DfuTarg");
            using var service = svc.Services[0];

            _cp = await GetChar(service, ControlPointUuid);
            _pkt = await GetChar(service, PacketUuid);
            _cp.ValueChanged += OnCpValueChanged;
            var st = await _cp.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.Notify);
            if (st != GattCommunicationStatus.Success) throw new InvalidOperationException("enable DFU notifications failed: " + st);

            try
            {
                var session = await GattSession.FromDeviceIdAsync(device.BluetoothDeviceId);
                _chunk = Math.Clamp(session.MaxPduSize - 3, 20, 244);
            }
            catch { _chunk = 20; }
            _log.LogInformation("Connected to bootloader {Addr}, chunk {Chunk} B", EpdClient.FormatAddress(target), _chunk);

            await CommandAsync(new[] { OpSetPrn, (byte)0, (byte)0 }, ct);
            await SendObjectAsync(1, init, "发送初始化包", progress, ct);
            await SendObjectAsync(2, fw, "发送固件", progress, ct);

            _cp.ValueChanged -= OnCpValueChanged;
            _cp = null; _pkt = null;
        }
        GC.Collect();
        GC.WaitForPendingFinalizers();
        _log.LogInformation("DFU transfer complete, bootloader is rebooting into the application");

        progress?.Report(new("等待应用重启", 0, 1));
        await Task.Delay(3000, ct);
        try
        {
            await EpdClient.WaitForAdvertisementAsync(appAddress, namePrefix, TimeSpan.FromSeconds(30), ct);
            await using var app = new EpdClient(_log);
            await app.ConnectAsync(appAddress, namePrefix, TimeSpan.FromSeconds(20), ct);
            progress?.Report(new("完成", 1, 1));
            return app.FirmwareVersion;
        }
        catch (Exception ex)
        {
            _log.LogWarning("Application did not report back after DFU: {Msg}", ex.Message);
            progress?.Report(new("完成（未能读回版本）", 1, 1));
            return null;
        }
    }

    private static async Task<GattCharacteristic> GetChar(GattDeviceService service, Guid uuid)
    {
        var r = await service.GetCharacteristicsForUuidAsync(uuid, BluetoothCacheMode.Uncached);
        if (r.Status != GattCommunicationStatus.Success || r.Characteristics.Count == 0)
            throw new InvalidOperationException($"characteristic {uuid} not found");
        return r.Characteristics[0];
    }

    /// <summary>Write 0x01 to the buttonless characteristic; the app reboots into the bootloader.</summary>
    private async Task<bool> EnterBootloaderAsync(ulong appAddress, CancellationToken ct)
    {
        await EpdClient.WaitForAdvertisementAsync(appAddress, "", TimeSpan.FromSeconds(25), ct);
        using var device = await BluetoothLEDevice.FromBluetoothAddressAsync(appAddress)
                           ?? throw new InvalidOperationException("application device not found");
        var svc = await device.GetGattServicesForUuidAsync(DfuServiceUuid, BluetoothCacheMode.Uncached);
        if (svc.Status != GattCommunicationStatus.Success || svc.Services.Count == 0)
            throw new InvalidOperationException("no DFU service on the application (firmware without buttonless DFU?)");
        using var service = svc.Services[0];
        var bl = await GetChar(service, ButtonlessUuid);

        var got = new TaskCompletionSource<byte[]>(TaskCreationOptions.RunContinuationsAsynchronously);
        void Handler(GattCharacteristic s, GattValueChangedEventArgs a)
        {
            CryptographicBuffer.CopyToByteArray(a.CharacteristicValue, out var b);
            got.TrySetResult(b);
        }
        bl.ValueChanged += Handler;
        try
        {
            var st = await bl.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.Indicate);
            if (st != GattCommunicationStatus.Success) throw new InvalidOperationException("enable buttonless indications failed: " + st);

            var w = await bl.WriteValueAsync(CryptographicBuffer.CreateFromByteArray(new byte[] { 0x01 }), GattWriteOption.WriteWithResponse);
            if (w != GattCommunicationStatus.Success) throw new InvalidOperationException("buttonless write failed: " + w);

            var done = await Task.WhenAny(got.Task, Task.Delay(5000, ct));
            if (done == got.Task)
            {
                var r = got.Task.Result;
                _log.LogInformation("Buttonless response {Hex}", Convert.ToHexString(r));
                if (r.Length >= 3 && r[2] != 0x01) throw new InvalidOperationException("buttonless DFU rejected, code " + r[2]);
            }
            else _log.LogInformation("No buttonless indication (device probably rebooted already)");
            return true;
        }
        finally
        {
            bl.ValueChanged -= Handler;
            service.Dispose();
            device.Dispose();
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }
    }

    /// <summary>The bootloader advertises as "DfuTarg" on the application address + 1.</summary>
    private static async Task<ulong> FindBootloaderAsync(ulong appAddress, TimeSpan timeout, CancellationToken ct)
    {
        ulong expected = (appAddress & ~0xFFUL) | ((appAddress + 1) & 0xFF);
        var tcs = new TaskCompletionSource<ulong>(TaskCreationOptions.RunContinuationsAsynchronously);
        var watcher = new BluetoothLEAdvertisementWatcher { ScanningMode = BluetoothLEScanningMode.Active };
        watcher.Received += (_, a) =>
        {
            if (a.BluetoothAddress == expected || a.Advertisement.LocalName == "DfuTarg")
                tcs.TrySetResult(a.BluetoothAddress);
        };
        watcher.Start();
        try
        {
            using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
            cts.CancelAfter(timeout);
            using var reg = cts.Token.Register(() => tcs.TrySetException(new TimeoutException("DfuTarg not found")));
            return await tcs.Task;
        }
        finally { watcher.Stop(); }
    }

    // ---------------------------------------------------------------- object transfer

    private void OnCpValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        CryptographicBuffer.CopyToByteArray(args.CharacteristicValue, out var b);
        _cpNotifications.Writer.TryWrite(b);
    }

    private async Task<byte[]> CommandAsync(byte[] payload, CancellationToken ct, int timeoutSeconds = 10)
    {
        while (_cpNotifications.Reader.TryRead(out _)) { }
        var st = await _cp!.WriteValueAsync(CryptographicBuffer.CreateFromByteArray(payload), GattWriteOption.WriteWithResponse);
        if (st != GattCommunicationStatus.Success) throw new InvalidOperationException($"control point write failed: {st}");

        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        cts.CancelAfter(TimeSpan.FromSeconds(timeoutSeconds));
        byte[] r;
        try { r = await _cpNotifications.Reader.ReadAsync(cts.Token); }
        catch (OperationCanceledException) when (!ct.IsCancellationRequested)
        { throw new TimeoutException($"no response to DFU op 0x{payload[0]:x2}"); }

        if (r.Length < 3 || r[0] != OpResponse || r[1] != payload[0])
            throw new InvalidOperationException($"bad DFU response {Convert.ToHexString(r)} to op 0x{payload[0]:x2}");
        if (r[2] != 0x01)
        {
            string name = ResultNames.TryGetValue(r[2], out var n) ? n : $"0x{r[2]:x2}";
            string ext = r[2] == 0x0B && r.Length > 3 ? $" ext=0x{r[3]:x2}" : "";
            throw new InvalidOperationException($"DFU op 0x{payload[0]:x2} failed: {name}{ext}");
        }
        return r[3..];
    }

    private async Task SendObjectAsync(byte type, byte[] data, string stage, IProgress<Progress>? progress, CancellationToken ct)
    {
        var sel = await CommandAsync(new[] { OpSelect, type }, ct);
        uint maxSize = BitConverter.ToUInt32(sel, 0);
        uint offset = BitConverter.ToUInt32(sel, 4);
        if (offset != 0) _log.LogInformation("Bootloader reports existing offset {Off}, restarting object from 0", offset);

        long total = data.Length, sent = 0;
        uint runningCrc = 0;
        progress?.Report(new(stage, 0, total));
        while (sent < total)
        {
            ct.ThrowIfCancellationRequested();
            int n = (int)Math.Min(maxSize, total - sent);
            var create = new byte[6];
            create[0] = OpCreate; create[1] = type;
            BitConverter.TryWriteBytes(create.AsSpan(2), (uint)n);
            await CommandAsync(create, ct);

            var obj = data.AsMemory((int)sent, n);
            for (int i = 0; i < n; i += _chunk)
            {
                var piece = obj.Slice(i, Math.Min(_chunk, n - i)).ToArray();
                var st = await _pkt!.WriteValueAsync(CryptographicBuffer.CreateFromByteArray(piece), GattWriteOption.WriteWithoutResponse);
                if (st != GattCommunicationStatus.Success) throw new InvalidOperationException("packet write failed: " + st);
            }
            runningCrc = Crc32.Update(runningCrc, obj.Span);

            var crc = await CommandAsync(new[] { OpCalcCrc }, ct);
            uint devOffset = BitConverter.ToUInt32(crc, 0);
            uint devCrc = BitConverter.ToUInt32(crc, 4);
            if (devOffset != sent + n || devCrc != runningCrc)
                throw new InvalidOperationException($"CRC mismatch at {devOffset}: device {devCrc:x8} vs local {runningCrc:x8}");

            await CommandAsync(new[] { OpExecute }, ct, timeoutSeconds: 30);
            sent += n;
            progress?.Report(new(stage, sent, total));
        }
        _log.LogInformation("{Stage}: {Bytes} bytes ok", stage, total);
    }

    /// <summary>zlib-compatible CRC-32 (what nrfutil / the bootloader use).</summary>
    private static class Crc32
    {
        private static readonly uint[] Table = Build();
        private static uint[] Build()
        {
            var t = new uint[256];
            for (uint i = 0; i < 256; i++)
            {
                uint c = i;
                for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                t[i] = c;
            }
            return t;
        }
        public static uint Update(uint crc, ReadOnlySpan<byte> data)
        {
            crc ^= 0xFFFFFFFFu;
            foreach (byte b in data) crc = Table[(crc ^ b) & 0xFF] ^ (crc >> 8);
            return crc ^ 0xFFFFFFFFu;
        }
    }
}
