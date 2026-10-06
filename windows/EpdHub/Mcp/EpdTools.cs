using System.ComponentModel;
using EpdHub.Render;
using EpdHub.Service;
using ModelContextProtocol.Server;

namespace EpdHub.Mcp;

/// <summary>MCP tools exposed to AI clients: free-form scenes, the text template, pictures, status.</summary>
[McpServerToolType]
public sealed class EpdTools
{
    private readonly ContentStore _store;
    private readonly DisplayService _display;

    public EpdTools(ContentStore store, DisplayService display)
    {
        _store = store;
        _display = display;
    }

    /// <summary>A push must finish even if the MCP client goes away; cap it at 5 minutes instead.</summary>
    private static CancellationToken PushToken() => new CancellationTokenSource(TimeSpan.FromMinutes(5)).Token;

    [McpServerTool(Name = "get_display_spec"),
     Description("Describe the e-paper display: size, colours, the scene JSON language (free layout: text, emoji, icons, rects, lines, images, bars, battery, QR) and the limits of the simple text template. Call this once before composing content.")]
    public DisplaySpec GetDisplaySpec() => _store.Spec;

    [McpServerTool(Name = "get_content"),
     Description("Return the content currently stored for the display: mode (scene | template | image), the scene JSON or template fields, and when it was last changed.")]
    public DisplayContent GetContent() => _store.Load();

    [McpServerTool(Name = "render_scene"),
     Description("Draw a free-form page from scene JSON (see get_display_spec for the element types) and push it to the panel. Returns warnings for elements that could not be drawn. Use preview_scene first if you want to inspect the result as PNG.")]
    public async Task<SceneResult> RenderScene(
        [Description("Scene JSON: {\"background\":\"white\",\"elements\":[{\"type\":\"text\",...},...]}")] string scene,
        [Description("Who produced this content, for the status log.")] string? source = null,
        [Description("Push to the panel immediately (true) or wait for the scheduler (false).")] bool pushNow = true,
        CancellationToken ct = default)
    {
        var parsed = Scene.Parse(scene);   // throws a readable error on bad JSON
        var content = _store.Load();
        content.Mode = DisplayContent.ModeScene;
        content.SceneJson = scene;
        content.Source = source;
        content.UpdatedAt = DateTimeOffset.Now;
        _store.Save(content);

        var warnings = new List<string>();
        var frame = _display.RenderCurrent(warnings);
        if (!pushNow)
            return new SceneResult(false, "saved, will push on next schedule", frame.Hash, _store.PreviewPath, warnings, parsed.Elements.Count);
        var r = await _display.PushAsync(force: false, PushToken());
        return new SceneResult(r.Pushed, r.Message, r.FrameHash, _store.PreviewPath, warnings, parsed.Elements.Count);
    }

    [McpServerTool(Name = "preview_scene"),
     Description("Render scene JSON to a PNG file without changing the display. Returns the file path (open it with an image viewer / read tool to check the layout) and warnings.")]
    public SceneResult PreviewScene(
        [Description("Scene JSON, same format as render_scene.")] string scene)
    {
        var parsed = Scene.Parse(scene);
        var warnings = new List<string>();
        var (hash, path) = _display.RenderScenePreview(parsed, warnings);
        return new SceneResult(false, "preview only", hash, path, warnings, parsed.Elements.Count);
    }

    [McpServerTool(Name = "set_content"),
     Description("Use the built-in simple text template: title, optional subtitle, up to 6 lines, footer. Prefix a line with '!' for red, 'label|text' for a bold label. For anything richer use render_scene.")]
    public async Task<PushResult> SetContent(
        [Description("Header title, keep it short (see MaxTitleChars).")] string title,
        [Description("Body lines, at most MaxLines.")] string[] lines,
        [Description("Optional one-line subtitle under the header.")] string? subtitle = null,
        [Description("Optional footer text.")] string? footer = null,
        [Description("Who produced this content, for the status log.")] string? source = null,
        [Description("Push to the panel immediately (true) or wait for the scheduler (false).")] bool pushNow = true,
        CancellationToken ct = default)
    {
        var content = new DisplayContent
        {
            Mode = DisplayContent.ModeTemplate,
            Title = title ?? "",
            Subtitle = subtitle,
            Footer = footer,
            Source = source,
            UpdatedAt = DateTimeOffset.Now,
            Lines = (lines ?? Array.Empty<string>()).Select(DisplayLine.Parse).ToList(),
        };
        _store.Save(content);

        if (!pushNow)
        {
            var frame = _display.RenderCurrent();
            return new PushResult(false, "saved, will push on next schedule", frame.Hash, TimeSpan.Zero);
        }
        return await _display.PushAsync(force: false, PushToken());
    }

    [McpServerTool(Name = "set_image"),
     Description("Show a full-screen picture (png/jpg/bmp/gif file, http(s) URL or data:image/...;base64) instead of any layout. Scaled/cropped to 400x300, rotated if portrait, dithered to black/white/red.")]
    public async Task<PushResult> SetImage(
        [Description("Absolute file path, URL or data URI of the image.")] string path,
        [Description("'cover' (fill, crop edges) or 'contain' (fit inside, white borders).")] string fit = "cover",
        [Description("'three_color' (black/white/red dither), 'bw' (black/white dither) or 'threshold'.")] string color = "three_color",
        [Description("Rotate portrait pictures by 90 degrees to fill the landscape panel.")] bool autoRotate = true,
        [Description("Push to the panel immediately.")] bool pushNow = true,
        CancellationToken ct = default)
    {
        var opts = new ImageOptions
        {
            Fit = fit.Equals("contain", StringComparison.OrdinalIgnoreCase) ? FitMode.Contain : FitMode.Cover,
            Color = color.ToLowerInvariant() switch
            {
                "bw" => ColorMode.BwDither,
                "threshold" => ColorMode.BwThreshold,
                _ => ColorMode.ThreeColorDither,
            },
            AutoRotate = autoRotate,
        };
        using var src = SceneRenderer.LoadImagePublic(path);
        using var processed = ImageProcessor.Process(src, _store.Spec.Width, _store.Spec.Height, opts);
        string name = path.StartsWith("data:", StringComparison.OrdinalIgnoreCase) ? "data-uri" : Path.GetFileName(path);
        return await _display.SetImageAsync(processed, name, pushNow, PushToken());
    }

    [McpServerTool(Name = "push_now"),
     Description("Re-render the stored content and push it to the panel now. With force=true the panel is refreshed even if nothing changed.")]
    public Task<PushResult> PushNow(bool force = false, CancellationToken ct = default) => _display.PushAsync(force, PushToken());

    [McpServerTool(Name = "get_status"),
     Description("Device and scheduler status: firmware version, battery voltage/percent (measured on the last connection), last push time and result, current frame hash, content mode, preview image path.")]
    public DisplayStatus GetStatus() => _display.Status;

    [McpServerTool(Name = "read_device"),
     Description("Connect to the tag now and read firmware version, panel config and battery voltage (takes ~10-20 s). Use when get_status has no battery reading yet.")]
    public async Task<DisplayStatus> ReadDevice(CancellationToken ct = default)
    {
        await _display.ReadDeviceInfoAsync(PushToken());
        return _display.Status;
    }
}

public sealed record SceneResult(bool Pushed, string Message, string FrameHash, string PreviewPath, List<string> Warnings, int ElementCount);
