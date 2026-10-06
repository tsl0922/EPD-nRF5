using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using QRCoder;

namespace EpdHub.Render;

/// <summary>A free-form page described by the caller (usually an AI agent) as a list of elements.</summary>
public sealed class Scene
{
    public string Background { get; set; } = "white";
    public List<SceneElement> Elements { get; set; } = new();

    public static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        AllowTrailingCommas = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        NumberHandling = JsonNumberHandling.AllowReadingFromString,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    public static Scene Parse(string json)
    {
        var scene = JsonSerializer.Deserialize<Scene>(json, JsonOptions) ?? throw new JsonException("scene is empty");
        scene.Elements ??= new();
        return scene;
    }
}

public sealed class SceneElement
{
    /// <summary>text | rect | line | ellipse | image | emoji | icon | bar | battery | qr</summary>
    public string Type { get; set; } = "text";
    public float X { get; set; }
    public float Y { get; set; }
    public float W { get; set; }
    public float H { get; set; }

    // text / emoji / icon / qr
    public string? Text { get; set; }
    public float Size { get; set; } = 16;
    public bool Bold { get; set; }
    public string Color { get; set; } = "black";
    /// <summary>default | emoji | icon | mono | any installed family name</summary>
    public string? Font { get; set; }
    /// <summary>left | center | right (inside W when W &gt; 0)</summary>
    public string Align { get; set; } = "left";
    /// <summary>top | middle | bottom (inside H when H &gt; 0)</summary>
    public string VAlign { get; set; } = "top";
    public bool Wrap { get; set; }
    /// <summary>Segoe MDL2 glyph code for type=icon, e.g. "E753" (cloud).</summary>
    public string? Glyph { get; set; }

    // line
    public float X2 { get; set; }
    public float Y2 { get; set; }
    /// <summary>Stroke width for line / rect / ellipse / bar outlines.</summary>
    public float Width { get; set; } = 1;
    public bool Dashed { get; set; }

    // rect / ellipse
    public string? Fill { get; set; }
    public string? Stroke { get; set; }
    public float Radius { get; set; }

    // image
    /// <summary>File path, http(s) URL or data:image/...;base64,... </summary>
    public string? Src { get; set; }
    /// <summary>contain | cover</summary>
    public string Fit { get; set; } = "contain";
    /// <summary>bw | three_color | threshold</summary>
    public string Dither { get; set; } = "bw";

    // bar / battery
    /// <summary>bar: 0..1 fill ratio. battery: overrides the measured level (0..1).</summary>
    public double? Value { get; set; }
    public bool ShowText { get; set; } = true;
}

public sealed record SceneContext(int? BatteryMv, int? BatteryPercent);

/// <summary>Draws a <see cref="Scene"/> onto a panel-sized bitmap using only the three panel colours.</summary>
public static class SceneRenderer
{
    public static readonly Color Red = Color.FromArgb(255, 0, 0);
    private const string DefaultFont = "Microsoft YaHei";
    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(15) };

    public static Bitmap Render(Scene scene, DisplaySpec spec, SceneContext ctx, List<string> warnings)
    {
        var bmp = new Bitmap(spec.Width, spec.Height, PixelFormat.Format32bppArgb);
        using var g = Graphics.FromImage(bmp);
        g.Clear(ParseColor(scene.Background, Color.White));
        g.TextRenderingHint = TextRenderingHint.SingleBitPerPixelGridFit;
        g.SmoothingMode = SmoothingMode.None;
        g.PixelOffsetMode = PixelOffsetMode.None;

        int i = 0;
        foreach (var e in scene.Elements)
        {
            i++;
            try
            {
                switch ((e.Type ?? "").ToLowerInvariant())
                {
                    case "text": DrawText(g, e, spec, DefaultFontFor(e)); break;
                    case "emoji": DrawText(g, e, spec, "Segoe UI Emoji"); break;
                    case "icon": DrawIcon(g, e, warnings); break;
                    case "rect": DrawRect(g, e); break;
                    case "ellipse": DrawEllipse(g, e); break;
                    case "line": DrawLine(g, e); break;
                    case "image": DrawImage(g, e, warnings); break;
                    case "bar": DrawBar(g, e); break;
                    case "battery": DrawBatteryElement(g, e, ctx, warnings); break;
                    case "qr": DrawQr(g, e, warnings); break;
                    default: warnings.Add($"element {i}: unknown type '{e.Type}'"); break;
                }
            }
            catch (Exception ex)
            {
                warnings.Add($"element {i} ({e.Type}): {ex.Message}");
            }
        }
        return bmp;
    }

    // ------------------------------------------------------------------ helpers

    public static Color ParseColor(string? name, Color fallback)
    {
        switch ((name ?? "").Trim().ToLowerInvariant())
        {
            case "": return fallback;
            case "black": case "#000": case "#000000": return Color.Black;
            case "white": case "#fff": case "#ffffff": return Color.White;
            case "red": case "#f00": case "#ff0000": return Red;
        }
        try
        {
            var c = ColorTranslator.FromHtml(name!);
            // Snap to the nearest panel colour.
            bool isRed = c.R > 160 && c.R > c.G + 60 && c.R > c.B + 60;
            if (isRed) return Red;
            int gray = (299 * c.R + 587 * c.G + 114 * c.B) / 1000;
            return gray >= 140 ? Color.White : Color.Black;
        }
        catch { return fallback; }
    }

    private static string DefaultFontFor(SceneElement e) => (e.Font ?? "").Trim().ToLowerInvariant() switch
    {
        "" or "default" => DefaultFont,
        "emoji" => "Segoe UI Emoji",
        "icon" => "Segoe MDL2 Assets",
        "mono" => "Consolas",
        _ => e.Font!,
    };

    private static Font MakeFont(string family, float sizePx, bool bold)
    {
        try { return new Font(family, Math.Max(6, sizePx), bold ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Pixel); }
        catch { return new Font(DefaultFont, Math.Max(6, sizePx), bold ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Pixel); }
    }

    private static StringFormat MakeFormat(SceneElement e)
    {
        var f = new StringFormat(StringFormat.GenericTypographic)
        {
            Trimming = e.Wrap ? StringTrimming.Word : StringTrimming.EllipsisCharacter,
            FormatFlags = StringFormatFlags.NoClip | (e.Wrap ? 0 : StringFormatFlags.NoWrap),
            Alignment = (e.Align ?? "left").ToLowerInvariant() switch { "center" => StringAlignment.Center, "right" => StringAlignment.Far, _ => StringAlignment.Near },
            LineAlignment = (e.VAlign ?? "top").ToLowerInvariant() switch { "middle" or "center" => StringAlignment.Center, "bottom" => StringAlignment.Far, _ => StringAlignment.Near },
        };
        return f;
    }

    private static RectangleF Box(SceneElement e, DisplaySpec spec)
    {
        float w = e.W > 0 ? e.W : spec.Width - e.X;
        float h = e.H > 0 ? e.H : spec.Height - e.Y;
        return new RectangleF(e.X, e.Y, w, h);
    }

    private static void DrawText(Graphics g, SceneElement e, DisplaySpec spec, string family)
    {
        if (string.IsNullOrEmpty(e.Text)) return;
        using var font = MakeFont(family, e.Size, e.Bold);
        using var brush = new SolidBrush(ParseColor(e.Color, Color.Black));
        using var fmt = MakeFormat(e);
        var box = Box(e, spec);
        if (e.H <= 0)
        {
            // Unbounded height: measure so bottom alignment still works sensibly.
            var size = g.MeasureString(e.Text, font, (int)box.Width, fmt);
            box.Height = Math.Max(size.Height, e.Size * 1.3f);
        }
        g.DrawString(e.Text, font, brush, box, fmt);
    }

    private static void DrawIcon(Graphics g, SceneElement e, List<string> warnings)
    {
        string? glyph = e.Glyph ?? e.Text;
        if (string.IsNullOrWhiteSpace(glyph)) { warnings.Add("icon without glyph"); return; }
        string text = glyph;
        if (glyph.Length >= 4 && glyph.All(Uri.IsHexDigit))
            text = char.ConvertFromUtf32(Convert.ToInt32(glyph, 16));
        float size = e.Size > 0 ? e.Size : 32;
        using var font = MakeFont("Segoe MDL2 Assets", size, false);
        using var brush = new SolidBrush(ParseColor(e.Color, Color.Black));
        using var fmt = new StringFormat(StringFormat.GenericTypographic) { Alignment = StringAlignment.Near, LineAlignment = StringAlignment.Near };
        g.DrawString(text, font, brush, new RectangleF(e.X, e.Y, size * 1.5f, size * 1.5f), fmt);
    }

    private static Pen MakePen(SceneElement e, Color color)
    {
        var pen = new Pen(color, Math.Max(1, e.Width));
        if (e.Dashed) pen.DashStyle = DashStyle.Dash;
        return pen;
    }

    private static void DrawRect(Graphics g, SceneElement e)
    {
        var rect = new RectangleF(e.X, e.Y, Math.Max(1, e.W), Math.Max(1, e.H));
        using var path = e.Radius > 0 ? Rounded(rect, e.Radius) : null;
        if (!string.IsNullOrEmpty(e.Fill))
        {
            using var brush = new SolidBrush(ParseColor(e.Fill, Color.Black));
            if (path is null) g.FillRectangle(brush, rect); else g.FillPath(brush, path);
        }
        if (!string.IsNullOrEmpty(e.Stroke) || string.IsNullOrEmpty(e.Fill))
        {
            using var pen = MakePen(e, ParseColor(e.Stroke, Color.Black));
            if (path is null) g.DrawRectangle(pen, rect.X, rect.Y, rect.Width - 1, rect.Height - 1); else g.DrawPath(pen, path);
        }
    }

    private static void DrawEllipse(Graphics g, SceneElement e)
    {
        var rect = new RectangleF(e.X, e.Y, Math.Max(1, e.W), Math.Max(1, e.H));
        if (!string.IsNullOrEmpty(e.Fill))
        {
            using var brush = new SolidBrush(ParseColor(e.Fill, Color.Black));
            g.FillEllipse(brush, rect);
        }
        if (!string.IsNullOrEmpty(e.Stroke) || string.IsNullOrEmpty(e.Fill))
        {
            using var pen = MakePen(e, ParseColor(e.Stroke, Color.Black));
            g.DrawEllipse(pen, rect);
        }
    }

    private static void DrawLine(Graphics g, SceneElement e)
    {
        float x2 = e.X2 != 0 || e.Y2 != 0 ? e.X2 : e.X + e.W;
        float y2 = e.X2 != 0 || e.Y2 != 0 ? e.Y2 : e.Y + e.H;
        using var pen = MakePen(e, ParseColor(e.Color, Color.Black));
        g.DrawLine(pen, e.X, e.Y, x2, y2);
    }

    private static void DrawImage(Graphics g, SceneElement e, List<string> warnings)
    {
        if (string.IsNullOrWhiteSpace(e.Src)) { warnings.Add("image without src"); return; }
        using var src = LoadImage(e.Src);
        int w = (int)(e.W > 0 ? e.W : src.Width), h = (int)(e.H > 0 ? e.H : src.Height);
        var opts = new ImageOptions
        {
            Fit = e.Fit.Equals("cover", StringComparison.OrdinalIgnoreCase) ? FitMode.Cover : FitMode.Contain,
            AutoRotate = false,
            AutoContrast = false,
            Color = (e.Dither ?? "bw").ToLowerInvariant() switch
            {
                "three_color" or "3c" or "bwr" => ColorMode.ThreeColorDither,
                "threshold" or "none" => ColorMode.BwThreshold,
                _ => ColorMode.BwDither,
            },
        };
        using var processed = ImageProcessor.Process(src, w, h, opts);
        g.DrawImageUnscaled(processed, (int)e.X, (int)e.Y);
    }

    public static Bitmap LoadImagePublic(string src) => LoadImage(src);

    private static Bitmap LoadImage(string src)
    {
        src = src.Trim();
        if (src.StartsWith("data:", StringComparison.OrdinalIgnoreCase))
        {
            int comma = src.IndexOf(',');
            if (comma < 0) throw new FormatException("bad data URI");
            var bytes = Convert.FromBase64String(src[(comma + 1)..]);
            using var ms = new MemoryStream(bytes);
            using var img = Image.FromStream(ms);
            return new Bitmap(img);
        }
        if (src.StartsWith("http://", StringComparison.OrdinalIgnoreCase) || src.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
        {
            var bytes = Http.GetByteArrayAsync(src).GetAwaiter().GetResult();
            using var ms = new MemoryStream(bytes);
            using var img = Image.FromStream(ms);
            return new Bitmap(img);
        }
        if (src.StartsWith("file://", StringComparison.OrdinalIgnoreCase)) src = new Uri(src).LocalPath;
        return ImageProcessor.Load(src);
    }

    private static void DrawBar(Graphics g, SceneElement e)
    {
        var rect = new RectangleF(e.X, e.Y, Math.Max(2, e.W), Math.Max(2, e.H));
        var color = ParseColor(e.Color, Color.Black);
        using var pen = MakePen(e, ParseColor(e.Stroke, color));
        g.DrawRectangle(pen, rect.X, rect.Y, rect.Width - 1, rect.Height - 1);
        double v = Math.Clamp(e.Value ?? 0, 0, 1);
        float inner = (float)((rect.Width - 4) * v);
        if (inner > 0)
        {
            using var brush = new SolidBrush(ParseColor(e.Fill, color));
            g.FillRectangle(brush, rect.X + 2, rect.Y + 2, inner, rect.Height - 4);
        }
    }

    private static void DrawBatteryElement(Graphics g, SceneElement e, SceneContext ctx, List<string> warnings)
    {
        int? pct = e.Value is { } v ? (int)Math.Round(Math.Clamp(v, 0, 1) * 100) : ctx.BatteryPercent;
        float w = e.W > 0 ? e.W : 28, h = e.H > 0 ? e.H : 14;
        var color = ParseColor(e.Color, Color.Black);
        DrawBattery(g, e.X, e.Y, w, h, pct, color);
        if (e.ShowText)
        {
            string text = pct is null ? "?" : $"{pct}%";
            using var font = MakeFont(DefaultFont, e.Size > 0 ? e.Size : h, e.Bold);
            using var brush = new SolidBrush(color);
            using var fmt = new StringFormat(StringFormat.GenericTypographic) { LineAlignment = StringAlignment.Center, FormatFlags = StringFormatFlags.NoWrap };
            g.DrawString(text, font, brush, new RectangleF(e.X + w + 6, e.Y - 2, 120, h + 4), fmt);
        }
        if (pct is null) warnings.Add("battery level unknown (device not read yet)");
    }

    /// <summary>Battery glyph: outline, nub on the right, proportional fill (red when low).</summary>
    public static void DrawBattery(Graphics g, float x, float y, float w, float h, int? percent, Color color)
    {
        float nub = Math.Max(2, w * 0.08f);
        using var pen = new Pen(color, 1.5f);
        g.DrawRectangle(pen, x, y, w - nub - 1, h - 1);
        using var brush = new SolidBrush(color);
        g.FillRectangle(brush, x + w - nub - 1, y + h * 0.3f, nub, h * 0.4f);
        if (percent is { } p)
        {
            float inner = (w - nub - 5) * Math.Clamp(p, 0, 100) / 100f;
            using var fill = new SolidBrush(p <= 20 ? Red : color);
            if (inner > 0) g.FillRectangle(fill, x + 2, y + 2, inner, h - 5);
        }
        else
        {
            using var font = new Font(DefaultFont, Math.Max(6, h - 4), FontStyle.Bold, GraphicsUnit.Pixel);
            g.DrawString("?", font, brush, x + 3, y - 1);
        }
    }

    private static void DrawQr(Graphics g, SceneElement e, List<string> warnings)
    {
        if (string.IsNullOrWhiteSpace(e.Text)) { warnings.Add("qr without text"); return; }
        using var gen = new QRCodeGenerator();
        using var data = gen.CreateQrCode(e.Text, QRCodeGenerator.ECCLevel.M);
        var matrix = data.ModuleMatrix;          // includes the 4-module quiet zone
        int n = matrix.Count;
        float size = e.W > 0 ? e.W : e.H > 0 ? e.H : 100;
        int cell = Math.Max(1, (int)(size / n));
        int total = cell * n;
        using var white = new SolidBrush(Color.White);
        using var dark = new SolidBrush(ParseColor(e.Color, Color.Black));
        g.FillRectangle(white, e.X, e.Y, total, total);
        for (int r = 0; r < n; r++)
            for (int c = 0; c < n; c++)
                if (matrix[r][c]) g.FillRectangle(dark, e.X + c * cell, e.Y + r * cell, cell, cell);
    }

    private static GraphicsPath Rounded(RectangleF r, float radius)
    {
        var p = new GraphicsPath();
        float d = Math.Min(radius * 2, Math.Min(r.Width, r.Height));
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    /// <summary>Compact reference for AI callers, returned by get_display_spec.</summary>
    public const string Guide =
        "SCENE JSON: {\"background\":\"white\",\"elements\":[...]} drawn in order on a 400x300 canvas, origin top-left, units = pixels. " +
        "Colours: black | white | red (other values snap to the nearest). Fonts: default (Microsoft YaHei, CJK ok) | emoji (Segoe UI Emoji, monochrome) | icon (Segoe MDL2 Assets) | mono (Consolas). " +
        "Element types and fields: " +
        "text{x,y,w?,h?,text,size=16,bold,color,font,align=left|center|right,valign=top|middle|bottom,wrap=false}; " +
        "emoji{x,y,text,size} (any emoji/pictograph, e.g. ☀️🌧️🔔📅✅, rendered as outlines); " +
        "icon{x,y,glyph='E753',size,color} (Segoe MDL2 hex code; common: E706 sun/brightness, E753 cloud, E787 calendar, E715 mail, E701 wifi, E702 bluetooth, E7BA warning, E73E check, E734 star, E717 phone, E72E lock, E8D7 heart?); " +
        "rect{x,y,w,h,fill?,stroke?,width=1,radius=0,dashed}; ellipse{x,y,w,h,fill?,stroke?,width}; line{x,y,x2,y2,color,width,dashed}; " +
        "image{x,y,w,h,src,fit=contain|cover,dither=bw|three_color|threshold} (src = local file path, http(s) URL or data:image/png;base64,...); " +
        "bar{x,y,w,h,value=0..1,color,fill?,stroke?}; battery{x,y,w=28,h=14,showText=true,value?} (uses the last measured voltage; value overrides); " +
        "qr{x,y,w,text,color}. " +
        "Keep text >= 12px, red for emphasis only, leave ~8px margins. Example: " +
        "{\"elements\":[{\"type\":\"rect\",\"x\":0,\"y\":0,\"w\":400,\"h\":40,\"fill\":\"black\"}," +
        "{\"type\":\"text\",\"x\":12,\"y\":6,\"w\":300,\"h\":28,\"text\":\"今日概览\",\"size\":22,\"bold\":true,\"color\":\"white\",\"valign\":\"middle\"}," +
        "{\"type\":\"battery\",\"x\":330,\"y\":13,\"color\":\"white\",\"showText\":false}," +
        "{\"type\":\"emoji\",\"x\":16,\"y\":56,\"text\":\"☀️\",\"size\":48}," +
        "{\"type\":\"text\",\"x\":80,\"y\":60,\"text\":\"东京 24°C 晴\",\"size\":20}," +
        "{\"type\":\"text\",\"x\":80,\"y\":88,\"text\":\"湿度 55%  风 3m/s\",\"size\":14}," +
        "{\"type\":\"line\",\"x\":12,\"y\":120,\"x2\":388,\"y2\":120,\"dashed\":true}," +
        "{\"type\":\"text\",\"x\":12,\"y\":130,\"w\":376,\"text\":\"15:00 周会 · 17:30 牙医\",\"size\":16}," +
        "{\"type\":\"bar\",\"x\":12,\"y\":160,\"w\":200,\"h\":14,\"value\":0.7}," +
        "{\"type\":\"qr\",\"x\":300,\"y\":190,\"w\":90,\"text\":\"https://example.com\"}]}";
}
