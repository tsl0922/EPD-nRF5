using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.Security.Cryptography;

namespace EpdHub.Render;

/// <summary>Two 1-bit planes in the firmware's format: bit=1 white, bit=0 black; red plane bit=0 red.</summary>
public sealed record Frame(int Width, int Height, byte[] Black, byte[] Red, string Hash)
{
    public int BytesPerRow => (Width + 7) / 8;
}

/// <summary>Renders the fixed 400x300 template with GDI+ and converts it to panel bit planes.</summary>
public static class FrameRenderer
{
    private static readonly Color RedInk = Color.FromArgb(255, 0, 0);
    private const string FontFamily = "Microsoft YaHei";

    public static Bitmap Render(DisplayContent c, DisplaySpec spec, DateTime? today = null, SceneContext? ctx = null)
    {
        int w = spec.Width, h = spec.Height;
        var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
        using var g = Graphics.FromImage(bmp);
        g.Clear(Color.White);
        // No anti-aliasing: the panel is 1-bit, grey pixels would be thresholded unpredictably.
        g.TextRenderingHint = TextRenderingHint.SingleBitPerPixelGridFit;
        g.SmoothingMode = SmoothingMode.None;
        g.PixelOffsetMode = PixelOffsetMode.None;

        using var titleFont = new Font(FontFamily, 22, FontStyle.Bold, GraphicsUnit.Pixel);
        using var dateFont = new Font(FontFamily, 15, FontStyle.Regular, GraphicsUnit.Pixel);
        using var subFont = new Font(FontFamily, 15, FontStyle.Regular, GraphicsUnit.Pixel);
        using var labelFont = new Font(FontFamily, 16, FontStyle.Bold, GraphicsUnit.Pixel);
        using var bodyFont = new Font(FontFamily, 16, FontStyle.Regular, GraphicsUnit.Pixel);
        using var footFont = new Font(FontFamily, 12, FontStyle.Regular, GraphicsUnit.Pixel);
        using var black = new SolidBrush(Color.Black);
        using var white = new SolidBrush(Color.White);
        using var red = new SolidBrush(RedInk);
        using var noWrap = new StringFormat(StringFormat.GenericTypographic)
        {
            FormatFlags = StringFormatFlags.NoWrap | StringFormatFlags.NoClip,
            Trimming = StringTrimming.EllipsisCharacter,
        };
        using var right = new StringFormat(noWrap) { Alignment = StringAlignment.Far };

        // ---- header: black bar, white title, date on the right, red rule underneath
        const int headerH = 40;
        g.FillRectangle(black, 0, 0, w, headerH);
        g.DrawString(c.Title, titleFont, white, new RectangleF(12, 7, w - 150, headerH - 8), noWrap);
        var day = today ?? DateTime.Now;
        string dateText = $"{day:M月d日} {DayName(day.DayOfWeek)}";
        g.DrawString(dateText, dateFont, white, new RectangleF(w - 150 - 4, 12, 150 - 8, 20), right);
        g.FillRectangle(red, 0, headerH, w, 3);

        int y = headerH + 3 + 8;

        // ---- subtitle
        if (!string.IsNullOrEmpty(c.Subtitle))
        {
            g.DrawString(c.Subtitle, subFont, black, new RectangleF(12, y, w - 24, 20), noWrap);
            y += 22;
            using var pen = new Pen(Color.Black, 1) { DashStyle = DashStyle.Dot };
            g.DrawLine(pen, 12, y + 2, w - 12, y + 2);
            y += 8;
        }

        // ---- body lines
        const int footerH = 26;
        int bodyBottom = h - footerH;
        int lines = Math.Max(1, c.Lines.Count);
        int lineH = Math.Clamp((bodyBottom - y) / Math.Max(lines, 4), 24, 36);
        foreach (var line in c.Lines)
        {
            if (y + lineH > bodyBottom) break;
            var ink = line.Highlight ? red : black;
            float x = 12;
            // bullet
            g.FillRectangle(ink, x, y + lineH / 2f - 3, 6, 6);
            x += 14;
            if (!string.IsNullOrEmpty(line.Label))
            {
                var size = g.MeasureString(line.Label, labelFont, int.MaxValue, noWrap);
                g.DrawString(line.Label, labelFont, ink, new RectangleF(x, y + (lineH - 20) / 2f, size.Width + 2, 20), noWrap);
                x += size.Width + 10;
            }
            g.DrawString(line.Text, bodyFont, ink, new RectangleF(x, y + (lineH - 20) / 2f, w - x - 12, 20), noWrap);
            y += lineH;
        }

        // ---- footer
        g.FillRectangle(black, 0, h - footerH, w, 1);
        string footLeft = c.Footer ?? "";
        string footRight = $"更新 {c.UpdatedAt.ToLocalTime():MM-dd HH:mm}";
        const float rightBlock = 118;
        float leftWidth = w - 12 - rightBlock - 12;
        if (ctx?.BatteryPercent is { } pct)
        {
            // battery glyph + voltage to the left of the timestamp
            float bx = w - 12 - rightBlock - 76;
            SceneRenderer.DrawBattery(g, bx, h - footerH + 7, 22, 11, pct, Color.Black);
            string vt = $"{pct}%";
            g.DrawString(vt, footFont, black, new RectangleF(bx + 27, h - footerH + 6, 46, 16), noWrap);
            leftWidth = bx - 12 - 6;
        }
        g.DrawString(footLeft, footFont, black, new RectangleF(12, h - footerH + 6, leftWidth, 16), noWrap);
        g.DrawString(footRight, footFont, black, new RectangleF(w - 12 - rightBlock, h - footerH + 6, rightBlock, 16), right);

        return bmp;
    }

    private static string DayName(DayOfWeek d) => d switch
    {
        DayOfWeek.Monday => "周一", DayOfWeek.Tuesday => "周二", DayOfWeek.Wednesday => "周三",
        DayOfWeek.Thursday => "周四", DayOfWeek.Friday => "周五", DayOfWeek.Saturday => "周六", _ => "周日",
    };

    /// <summary>Threshold the bitmap into black and red planes (same rules as the web client's threeColor mode).</summary>
    public static Frame ToPlanes(Bitmap bmp)
    {
        int w = bmp.Width, h = bmp.Height, bpr = (w + 7) / 8;
        var black = new byte[bpr * h];
        var red = new byte[bpr * h];
        var rect = new Rectangle(0, 0, w, h);
        var data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try
        {
            unsafe
            {
                for (int y = 0; y < h; y++)
                {
                    byte* row = (byte*)data.Scan0 + y * data.Stride;
                    for (int x = 0; x < w; x++)
                    {
                        byte b = row[x * 4], gch = row[x * 4 + 1], r = row[x * 4 + 2];
                        int gray = (299 * r + 587 * gch + 114 * b) / 1000;
                        bool isRed = r > 160 && r > gch + 60 && r > b + 60;
                        bool isWhite = isRed || gray >= 140;
                        int idx = y * bpr + (x >> 3);
                        int bit = 0x80 >> (x & 7);
                        if (isWhite) black[idx] |= (byte)bit;
                        if (!isRed) red[idx] |= (byte)bit;
                    }
                }
            }
        }
        finally { bmp.UnlockBits(data); }

        var hash = Convert.ToHexString(SHA256.HashData(black.Concat(red).ToArray()))[..16];
        return new Frame(w, h, black, red, hash);
    }

    /// <summary>Reconstruct what the panel will show from the planes (for the preview image).</summary>
    public static Bitmap FromPlanes(Frame f)
    {
        var bmp = new Bitmap(f.Width, f.Height, PixelFormat.Format24bppRgb);
        for (int y = 0; y < f.Height; y++)
            for (int x = 0; x < f.Width; x++)
            {
                int idx = y * f.BytesPerRow + (x >> 3);
                int bit = 0x80 >> (x & 7);
                bool isRed = (f.Red[idx] & bit) == 0;
                bool isWhite = (f.Black[idx] & bit) != 0;
                bmp.SetPixel(x, y, isRed ? RedInk : isWhite ? Color.White : Color.Black);
            }
        return bmp;
    }
}
