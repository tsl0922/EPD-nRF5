namespace EpdHub.Render;

/// <summary>Fixed capabilities of the panel and the template. Returned to the AI by get_display_spec.</summary>
public sealed record DisplaySpec
{
    public int Width { get; init; } = 400;
    public int Height { get; init; } = 300;
    public string Colors { get; init; } = "black, white, red";
    public int MaxLines { get; init; } = 6;
    public int MaxLineChars { get; init; } = 24;
    public int MaxLabelChars { get; init; } = 6;
    public int MaxTitleChars { get; init; } = 14;
    public int MaxSubtitleChars { get; init; } = 30;
    public int MaxFooterChars { get; init; } = 36;
    public string Notes { get; init; } =
        "Landscape 4.2\" three-colour e-paper. Layout is fixed: black header bar with the title on the left " +
        "and today's date on the right; optional subtitle line; up to MaxLines body lines, each with an optional " +
        "short bold label and an optional red highlight; small footer line. Character limits count CJK characters " +
        "as 2. A full refresh takes about 20 s and wears the panel, so only change content when it matters. " +
        "set_image replaces the template with a picture; render_scene draws a free layout. " +
        "Prefer render_scene for anything beyond a plain list.";

    /// <summary>Reference for the free-layout scene JSON accepted by render_scene / preview_scene.</summary>
    public string SceneGuide { get; init; } = SceneRenderer.Guide;
}

public sealed class DisplayLine
{
    /// <summary>Short bold label drawn before the text, e.g. "天气" or "TODO". Optional.</summary>
    public string? Label { get; set; }
    public string Text { get; set; } = "";
    /// <summary>Draw the line in red instead of black.</summary>
    public bool Highlight { get; set; }

    /// <summary>Line syntax used by the MCP tool and the GUI: "!label|text" (! = red, label| optional).</summary>
    public static DisplayLine Parse(string raw)
    {
        raw ??= "";
        bool highlight = raw.StartsWith('!');
        if (highlight) raw = raw[1..];
        string? label = null;
        int bar = raw.IndexOf('|');
        if (bar > 0 && bar <= 12)
        {
            label = raw[..bar].Trim();
            raw = raw[(bar + 1)..];
        }
        return new DisplayLine { Label = label, Text = raw.Trim(), Highlight = highlight };
    }

    public string ToSyntax() => (Highlight ? "!" : "") + (string.IsNullOrEmpty(Label) ? "" : Label + "|") + Text;
}

public sealed class DisplayContent
{
    public const string ModeTemplate = "template";
    public const string ModeImage = "image";
    public const string ModeScene = "scene";

    /// <summary>Scene JSON (free layout) when Mode == "scene".</summary>
    public string? SceneJson { get; set; }

    /// <summary>"template" (title/lines) or "image" (a processed picture stored at ImagePath).</summary>
    public string Mode { get; set; } = ModeTemplate;
    public string? ImagePath { get; set; }

    public string Title { get; set; } = "EPD Hub";
    public string? Subtitle { get; set; }
    public List<DisplayLine> Lines { get; set; } = new();
    public string? Footer { get; set; }
    /// <summary>When the content was last changed. Shown in the footer; drives the change detection.</summary>
    public DateTimeOffset UpdatedAt { get; set; } = DateTimeOffset.Now;
    /// <summary>Free-form tag of who produced the content (an agent name, "manual", ...).</summary>
    public string? Source { get; set; }

    public static DisplayContent Welcome() => new()
    {
        Title = "EPD Hub",
        Subtitle = "等待内容",
        Lines =
        {
            new DisplayLine { Label = "状态", Text = "已连接 Windows 主机" },
            new DisplayLine { Label = "MCP", Text = "set_content 可更新此屏幕" },
            new DisplayLine { Text = "内容由 AI 定期整理后推送", Highlight = true },
        },
        Footer = "EPD-nRF5 · 400×300 · BWR",
        Source = "welcome",
    };

    /// <summary>Clamp everything to the spec so a sloppy caller cannot break the layout.</summary>
    public DisplayContent Normalize(DisplaySpec spec)
    {
        if (Mode != ModeImage && Mode != ModeScene) Mode = ModeTemplate;
        Title = Clip(Title ?? "", spec.MaxTitleChars);
        Subtitle = string.IsNullOrWhiteSpace(Subtitle) ? null : Clip(Subtitle!, spec.MaxSubtitleChars);
        Footer = string.IsNullOrWhiteSpace(Footer) ? null : Clip(Footer!, spec.MaxFooterChars);
        Lines = (Lines ?? new()).Where(l => l is not null).Take(spec.MaxLines).ToList();
        foreach (var l in Lines)
        {
            l.Label = string.IsNullOrWhiteSpace(l.Label) ? null : Clip(l.Label!, spec.MaxLabelChars);
            l.Text = Clip(l.Text ?? "", spec.MaxLineChars);
        }
        return this;
    }

    /// <summary>Display width in "half-width cells": CJK and other wide characters count as 2.</summary>
    public static int Cells(string s) => s.Sum(c => IsWide(c) ? 2 : 1);

    public static string Clip(string s, int maxCells)
    {
        s = s.Replace("\r", "").Replace("\n", " ").Trim();
        if (Cells(s) <= maxCells) return s;
        var sb = new System.Text.StringBuilder();
        int cells = 0;
        foreach (char c in s)
        {
            int w = IsWide(c) ? 2 : 1;
            if (cells + w > maxCells - 1) break;
            sb.Append(c);
            cells += w;
        }
        return sb.Append('…').ToString();
    }

    private static bool IsWide(char c) =>
        c >= 0x1100 && (c <= 0x115F || c == 0x2329 || c == 0x232A ||
        (c >= 0x2E80 && c <= 0xA4CF && c != 0x303F) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE6F) ||
        (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6));
}
