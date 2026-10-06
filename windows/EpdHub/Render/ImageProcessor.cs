using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

namespace EpdHub.Render;

public enum FitMode { Cover, Contain }
public enum ColorMode { ThreeColorDither, BwDither, BwThreshold }

public sealed record ImageOptions
{
    public FitMode Fit { get; init; } = FitMode.Cover;
    /// <summary>Rotate portrait sources by 90° so they fill the landscape panel.</summary>
    public bool AutoRotate { get; init; } = true;
    /// <summary>Extra manual rotation in degrees: 0, 90, 180, 270.</summary>
    public int Rotate { get; init; } = 0;
    public ColorMode Color { get; init; } = ColorMode.ThreeColorDither;
    /// <summary>Stretch the 1%..99% luminance range to full black..white before dithering.</summary>
    public bool AutoContrast { get; init; } = true;
    /// <summary>-100..100, applied before dithering.</summary>
    public int Brightness { get; init; } = 0;
}

/// <summary>Turns an arbitrary picture into a 400x300 three-colour bitmap the panel can show.</summary>
public static class ImageProcessor
{
    /// <summary>Load a file and apply its EXIF orientation.</summary>
    public static Bitmap Load(string path)
    {
        using var fs = File.OpenRead(path);
        using var img = Image.FromStream(fs, useEmbeddedColorManagement: false, validateImageData: false);
        var bmp = new Bitmap(img);
        try
        {
            const int OrientationId = 0x0112;
            if (img.PropertyIdList.Contains(OrientationId))
            {
                int o = img.GetPropertyItem(OrientationId)?.Value?[0] ?? 1;
                var flip = o switch
                {
                    2 => RotateFlipType.RotateNoneFlipX, 3 => RotateFlipType.Rotate180FlipNone,
                    4 => RotateFlipType.RotateNoneFlipY, 5 => RotateFlipType.Rotate90FlipX,
                    6 => RotateFlipType.Rotate90FlipNone, 7 => RotateFlipType.Rotate270FlipX,
                    8 => RotateFlipType.Rotate270FlipNone, _ => RotateFlipType.RotateNoneFlipNone,
                };
                if (flip != RotateFlipType.RotateNoneFlipNone) bmp.RotateFlip(flip);
            }
        }
        catch { /* no EXIF */ }
        return bmp;
    }

    public static Bitmap Process(Image source, int width, int height, ImageOptions o)
    {
        using var fitted = Fit(source, width, height, o);
        return Dither(fitted, o);
    }

    /// <summary>Rotate + scale + crop/letterbox onto a white width×height canvas.</summary>
    public static Bitmap Fit(Image source, int width, int height, ImageOptions o)
    {
        Bitmap src = new(source);
        try
        {
            int rot = o.Rotate;
            if (o.AutoRotate && (src.Height > src.Width) != (height > width)) rot += 90;
            rot = ((rot % 360) + 360) % 360;
            if (rot == 90) src.RotateFlip(RotateFlipType.Rotate90FlipNone);
            else if (rot == 180) src.RotateFlip(RotateFlipType.Rotate180FlipNone);
            else if (rot == 270) src.RotateFlip(RotateFlipType.Rotate270FlipNone);

            var canvas = new Bitmap(width, height, PixelFormat.Format24bppRgb);
            using var g = Graphics.FromImage(canvas);
            g.Clear(Color.White);
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.SmoothingMode = SmoothingMode.HighQuality;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.CompositingQuality = CompositingQuality.HighQuality;

            double sx = (double)width / src.Width, sy = (double)height / src.Height;
            double s = o.Fit == FitMode.Cover ? Math.Max(sx, sy) : Math.Min(sx, sy);
            int dw = (int)Math.Round(src.Width * s), dh = (int)Math.Round(src.Height * s);
            int dx = (width - dw) / 2, dy = (height - dh) / 2;
            using var attrs = new ImageAttributes();
            attrs.SetWrapMode(WrapMode.TileFlipXY);   // avoids the half-pixel transparent border
            g.DrawImage(src, new Rectangle(dx, dy, dw, dh), 0, 0, src.Width, src.Height, GraphicsUnit.Pixel, attrs);
            return canvas;
        }
        finally { src.Dispose(); }
    }

    /// <summary>Reduce to the panel palette with Floyd–Steinberg error diffusion (or a plain threshold).</summary>
    public static Bitmap Dither(Bitmap src, ImageOptions o)
    {
        int w = src.Width, h = src.Height;
        var r = new float[w * h]; var g = new float[w * h]; var b = new float[w * h];

        var rect = new Rectangle(0, 0, w, h);
        var data = src.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
        try
        {
            unsafe
            {
                for (int y = 0; y < h; y++)
                {
                    byte* row = (byte*)data.Scan0 + y * data.Stride;
                    for (int x = 0; x < w; x++)
                    {
                        int i = y * w + x;
                        b[i] = row[x * 3]; g[i] = row[x * 3 + 1]; r[i] = row[x * 3 + 2];
                    }
                }
            }
        }
        finally { src.UnlockBits(data); }

        // --- tone adjustments
        if (o.AutoContrast)
        {
            var hist = new int[256];
            for (int i = 0; i < r.Length; i++) hist[(int)(0.299f * r[i] + 0.587f * g[i] + 0.114f * b[i])]++;
            int lo = Percentile(hist, r.Length, 0.01), hi = Percentile(hist, r.Length, 0.99);
            if (hi - lo > 32)
            {
                float scale = 255f / (hi - lo);
                for (int i = 0; i < r.Length; i++)
                {
                    r[i] = Math.Clamp((r[i] - lo) * scale, 0, 255);
                    g[i] = Math.Clamp((g[i] - lo) * scale, 0, 255);
                    b[i] = Math.Clamp((b[i] - lo) * scale, 0, 255);
                }
            }
        }
        if (o.Brightness != 0)
        {
            float add = o.Brightness * 1.28f;
            for (int i = 0; i < r.Length; i++)
            {
                r[i] = Math.Clamp(r[i] + add, 0, 255);
                g[i] = Math.Clamp(g[i] + add, 0, 255);
                b[i] = Math.Clamp(b[i] + add, 0, 255);
            }
        }

        // --- palette
        (float r, float g, float b)[] palette = o.Color == ColorMode.ThreeColorDither
            ? new[] { (255f, 255f, 255f), (0f, 0f, 0f), (255f, 0f, 0f) }
            : new[] { (255f, 255f, 255f), (0f, 0f, 0f) };

        var outIdx = new byte[w * h];
        if (o.Color == ColorMode.BwThreshold)
        {
            for (int i = 0; i < r.Length; i++)
                outIdx[i] = (0.299f * r[i] + 0.587f * g[i] + 0.114f * b[i]) >= 140 ? (byte)0 : (byte)1;
        }
        else
        {
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    int i = y * w + x;
                    float or = r[i], og = g[i], ob = b[i];
                    int best = 0; float bestD = float.MaxValue;
                    for (int p = 0; p < palette.Length; p++)
                    {
                        float dr = or - palette[p].r, dg = og - palette[p].g, db = ob - palette[p].b;
                        float d = dr * dr + dg * dg + db * db;
                        if (d < bestD) { bestD = d; best = p; }
                    }
                    outIdx[i] = (byte)best;
                    float er = or - palette[best].r, eg = og - palette[best].g, eb = ob - palette[best].b;
                    Spread(x + 1, y, 7f / 16); Spread(x - 1, y + 1, 3f / 16); Spread(x, y + 1, 5f / 16); Spread(x + 1, y + 1, 1f / 16);

                    void Spread(int px, int py, float k)
                    {
                        if (px < 0 || px >= w || py >= h) return;
                        int j = py * w + px;
                        r[j] += er * k; g[j] += eg * k; b[j] += eb * k;
                    }
                }
            }
        }

        var dst = new Bitmap(w, h, PixelFormat.Format24bppRgb);
        var ddata = dst.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
        try
        {
            unsafe
            {
                for (int y = 0; y < h; y++)
                {
                    byte* row = (byte*)ddata.Scan0 + y * ddata.Stride;
                    for (int x = 0; x < w; x++)
                    {
                        var p = palette[outIdx[y * w + x]];
                        row[x * 3] = (byte)p.b; row[x * 3 + 1] = (byte)p.g; row[x * 3 + 2] = (byte)p.r;
                    }
                }
            }
        }
        finally { dst.UnlockBits(ddata); }
        return dst;
    }

    private static int Percentile(int[] hist, int total, double q)
    {
        long acc = 0; long target = (long)(total * q);
        for (int i = 0; i < hist.Length; i++)
        {
            acc += hist[i];
            if (acc >= target) return i;
        }
        return 255;
    }
}
