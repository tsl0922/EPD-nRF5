using System.Runtime.InteropServices;
using EpdHub.App;
using EpdHub.Ble;
using EpdHub.Render;
using EpdHub.Service;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace EpdHub;

internal static class Program
{
    [DllImport("kernel32.dll")] private static extern bool AttachConsole(int dwProcessId);
    [DllImport("kernel32.dll")] private static extern bool FreeConsole();
    private const int AttachParentProcess = -1;

    [STAThread]
    private static int Main(string[] args)
    {
        string command = args.Length > 0 ? args[0].ToLowerInvariant() : "tray";
        string[] rest = args.Skip(1).ToArray();

        if (command is "tray" or "serve")
            return RunTray(rest);

        // CLI mode: this is a WinExe, so re-attach to the parent console for output.
        AttachConsole(AttachParentProcess);
        Console.SetOut(new StreamWriter(Console.OpenStandardOutput()) { AutoFlush = true });
        Console.SetError(new StreamWriter(Console.OpenStandardError()) { AutoFlush = true });
        try
        {
            Console.WriteLine();
            return RunCliAsync(command, rest).GetAwaiter().GetResult();
        }
        finally { FreeConsole(); }
    }

    private static int RunTray(string[] args)
    {
        using var mutex = new Mutex(true, "EpdHub.SingleInstance", out bool created);
        if (!created)
        {
            MessageBox.Show("EpdHub 已经在运行（托盘图标）。", "EpdHub", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return 0;
        }

        Application.EnableVisualStyles();
        Application.SetHighDpiMode(HighDpiMode.SystemAware);
        Application.SetCompatibleTextRenderingDefault(false);

        var app = HostRunner.BuildWeb(args);
        Task.Run(() => app.StartAsync()).GetAwaiter().GetResult();
        try
        {
            var ctx = new TrayContext(app.Services);
            if (args.Contains("--show")) ctx.ShowForm(null);
            Application.Run(ctx);
        }
        finally
        {
            Task.Run(() => app.StopAsync(TimeSpan.FromSeconds(5))).GetAwaiter().GetResult();
        }
        return 0;
    }

    private static async Task<int> RunCliAsync(string command, string[] rest)
    {
        switch (command)
        {
            case "scan":
            {
                Console.WriteLine("scanning 8s...");
                var list = await EpdClient.ScanAsync("NRF_EPD", TimeSpan.FromSeconds(8), CancellationToken.None);
                foreach (var d in list) Console.WriteLine($"{d.Rssi,5} dBm  {d.AddressText}  {d.Name}");
                if (list.Count == 0) Console.WriteLine("no NRF_EPD devices found");
                return 0;
            }
            case "preview":
            {
                using var host = HostRunner.BuildCli(rest);
                var svc = host.Services.GetRequiredService<DisplayService>();
                var frame = svc.RenderCurrent();
                Console.WriteLine($"frame {frame.Hash} -> {svc.Status.PreviewPath}");
                return 0;
            }
            case "push":
            {
                using var host = HostRunner.BuildCli(rest);
                bool force = rest.Contains("--force");
                if (rest.Contains("--demo"))
                    host.Services.GetRequiredService<ContentStore>().Save(DisplayContent.Welcome());
                var svc = host.Services.GetRequiredService<DisplayService>();
                var result = await svc.PushAsync(force, CancellationToken.None);
                Console.WriteLine($"{(result.Pushed ? "PUSHED" : "SKIPPED")}: {result.Message} (frame {result.FrameHash}, {result.Elapsed.TotalSeconds:F1}s)");
                return result.Message.StartsWith("failed") ? 1 : 0;
            }
            case "image":
            {
                string? file = rest.FirstOrDefault(a => !a.StartsWith("--"));
                if (file is null || !File.Exists(file)) { Console.WriteLine("usage: EpdHub image <file> [--contain] [--bw] [--no-rotate]"); return 2; }
                using var host = HostRunner.BuildCli(rest);
                var svc = host.Services.GetRequiredService<DisplayService>();
                var opts = new ImageOptions
                {
                    Fit = rest.Contains("--contain") ? FitMode.Contain : FitMode.Cover,
                    Color = rest.Contains("--bw") ? ColorMode.BwDither : ColorMode.ThreeColorDither,
                    AutoRotate = !rest.Contains("--no-rotate"),
                };
                using var src = ImageProcessor.Load(file);
                using var processed = ImageProcessor.Process(src, svc.Spec.Width, svc.Spec.Height, opts);
                var result = await svc.SetImageAsync(processed, Path.GetFileName(file), pushNow: !rest.Contains("--no-push"), CancellationToken.None);
                Console.WriteLine($"{(result.Pushed ? "PUSHED" : "SKIPPED")}: {result.Message} -> preview {svc.Status.PreviewPath}");
                return 0;
            }
            case "dfu":
            {
                string? zip = rest.FirstOrDefault(a => !a.StartsWith("--"));
                if (zip is null || !File.Exists(zip)) { Console.WriteLine("usage: EpdHub dfu <package.zip>"); return 2; }
                using var host = HostRunner.BuildCli(rest);
                var opt = host.Services.GetRequiredService<Microsoft.Extensions.Options.IOptions<HubOptions>>().Value;
                var log = host.Services.GetRequiredService<ILoggerFactory>().CreateLogger<DfuClient>();
                ulong addr = string.IsNullOrWhiteSpace(opt.DeviceAddress) ? 0 : EpdClient.ParseAddress(opt.DeviceAddress);
                var progress = new Progress<DfuClient.Progress>(p => Console.WriteLine($"{p.Stage}: {p.Done}/{p.Total}"));
                var ver = await new DfuClient(log).UpdateAsync(addr, opt.DeviceNamePrefix, zip, progress, CancellationToken.None);
                Console.WriteLine(ver is null ? "DFU done, app did not report back" : $"DFU done, app version 0x{ver:x2}");
                Console.WriteLine("The firmware restarts in calendar mode; pushing current content with --force...");
                var svc = host.Services.GetRequiredService<DisplayService>();
                var r = await svc.PushAsync(force: true, CancellationToken.None);
                Console.WriteLine($"{(r.Pushed ? "PUSHED" : "SKIPPED")}: {r.Message}");
                return 0;
            }
            default:
                Console.WriteLine("""
                    EpdHub - Windows host for the EPD-nRF5 e-paper tag

                      EpdHub                     tray app: scheduler + MCP server + control panel
                      EpdHub push [--demo] [--force]
                      EpdHub image <file> [--contain] [--bw] [--no-rotate] [--no-push]
                      EpdHub dfu <package.zip>   OTA firmware update over BLE
                      EpdHub preview             render content.json to preview.png only
                      EpdHub scan                list NRF_EPD_* devices in range

                    Settings: appsettings.json next to the exe (section "Hub").
                    """);
                return 0;
        }
    }
}
