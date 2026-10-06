using EpdHub.App;
using EpdHub.Mcp;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace EpdHub.Service;

/// <summary>Builds the DI container for the tray app (with MCP/HTTP) and for one-shot CLI commands.</summary>
public static class HostRunner
{
    public static WebApplication BuildWeb(string[] args)
    {
        var builder = WebApplication.CreateBuilder(new WebApplicationOptions
        {
            Args = args,
            ContentRootPath = AppContext.BaseDirectory,   // appsettings.json next to the exe, whatever the cwd is
        });
        ConfigureCommon(builder.Services, builder.Configuration, builder.Logging);
        builder.Services.AddHostedService(sp => sp.GetRequiredService<DisplayService>());
        builder.Services.AddMcpServer(o =>
            {
                o.ServerInfo = new() { Name = "EpdHub", Version = "1.2" };
                o.ServerInstructions = ServerInstructions;
            })
            .WithHttpTransport()
            .WithTools<EpdTools>();

        var app = builder.Build();
        var opt = app.Services.GetRequiredService<IOptions<HubOptions>>().Value;
        app.Urls.Add(opt.McpUrl);
        app.MapGet("/", () => "EpdHub is running. MCP endpoint: /mcp");
        app.MapGet("/preview.png", (ContentStore store) =>
            File.Exists(store.PreviewPath) ? Results.File(store.PreviewPath, "image/png") : Results.NotFound());
        app.MapGet("/status", (DisplayService d) => Results.Json(d.Status));
        app.MapMcp("/mcp");
        return app;
    }

    /// <summary>Sent to MCP clients at handshake; Claude Code shows it as the usage rules for this server.</summary>
    private const string ServerInstructions = """
        EpdHub drives a 400x300 black/white/red e-paper tag (4.2", landscape) over Bluetooth. Rules:
        1. Call get_display_spec once per session before composing anything; its SceneGuide documents the scene JSON.
        2. Prefer render_scene (free layout: text, emoji, icons, rects, lines, images, bars, battery, qr). set_content is only a quick fixed text template; set_image shows one full-screen picture.
        3. Iterate with preview_scene first: it writes a PNG and returns its path - read that image to check the layout, then call render_scene with the final JSON. Fix every warning it returns.
        4. A full refresh takes ~20 s and wears the panel: push only when the content really changed, never in a loop, and do not call push_now(force=true) unless the user asks for a redraw. A push can take up to 90 s; wait for it.
        5. Design for e-paper: no greys or gradients, text >= 12 px, red only for emphasis, ~8 px margins, CJK characters count double for width. Content must be self-contained - the tag shows no clock of its own.
        6. Battery, firmware and last-push info come from get_status (read_device connects to refresh them). Include a battery element or the percent from get_status when the user cares about power.
        """;

    public static IHost BuildCli(string[] args)
    {
        var builder = Host.CreateApplicationBuilder(new HostApplicationBuilderSettings
        {
            Args = args,
            ContentRootPath = AppContext.BaseDirectory,
        });
        ConfigureCommon(builder.Services, builder.Configuration, builder.Logging);
        return builder.Build();
    }

    private static void ConfigureCommon(IServiceCollection services, IConfiguration config, ILoggingBuilder logging)
    {
        services.Configure<HubOptions>(config.GetSection(HubOptions.Section));
        services.AddSingleton<ContentStore>();
        services.AddSingleton<DisplayService>();
        logging.AddProvider(UiLog.Instance);
        var dataDir = new HubOptions { DataDir = config[$"{HubOptions.Section}:DataDir"] ?? "" }.ResolvedDataDir;
        Directory.CreateDirectory(dataDir);
        UiLog.Instance.SetFile(Path.Combine(dataDir, "epdhub.log"));
    }
}
