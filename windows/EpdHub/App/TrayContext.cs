using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using EpdHub.Service;
using Microsoft.Extensions.DependencyInjection;

namespace EpdHub.App;

/// <summary>Tray icon + context menu; owns the (lazily created) control panel window.</summary>
public sealed class TrayContext : ApplicationContext
{
    private readonly IServiceProvider _sp;
    private readonly DisplayService _display;
    private readonly NotifyIcon _icon;
    private readonly ToolStripMenuItem _autostartItem;
    private EpdHub.App.MainForm? _form;

    public TrayContext(IServiceProvider sp)
    {
        _sp = sp;
        _display = sp.GetRequiredService<DisplayService>();

        var menu = new ContextMenuStrip { Font = new Font("Microsoft YaHei UI", 9f) };
        menu.Items.Add("打开控制面板", null, (_, _) => ShowForm(null));
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("立即推送（有变化才刷）", null, (_, _) => _display.RequestPush(false));
        menu.Items.Add("强制刷屏", null, (_, _) => _display.RequestPush(true));
        menu.Items.Add("导入图片…", null, (_, _) => ShowForm(EpdHub.App.MainForm.TabImage, importImage: true));
        menu.Items.Add("打开预览图", null, (_, _) => OpenPreview());
        menu.Items.Add(new ToolStripSeparator());
        var mcp = new ToolStripMenuItem("MCP: " + _display.Options.McpUrl + "/mcp") { Enabled = false };
        menu.Items.Add(mcp);
        _autostartItem = new ToolStripMenuItem("开机自启") { CheckOnClick = true, Checked = SafeIsAutostart() };
        _autostartItem.CheckedChanged += (_, _) =>
        {
            try { Autostart.Set(_autostartItem.Checked); }
            catch (Exception ex) { MessageBox.Show("写入自启动失败: " + ex.Message, "EpdHub"); }
        };
        menu.Items.Add(_autostartItem);
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("退出", null, (_, _) => Exit());

        _icon = new NotifyIcon
        {
            Icon = CreateIcon(),
            Text = "EpdHub – 墨水屏主机",
            ContextMenuStrip = menu,
            Visible = true,
        };
        _icon.DoubleClick += (_, _) => ShowForm(null);

        _display.ConnectivityChanged += (lost, msg) =>
        {
            try { _icon.ShowBalloonTip(8000, "EpdHub", msg, lost ? ToolTipIcon.Warning : ToolTipIcon.Info); } catch { }
        };
        _display.PushCompleted += r =>
        {
            try
            {
                _icon.ShowBalloonTip(3000, "EpdHub",
                    r.Pushed ? $"屏幕已更新（{r.Elapsed.TotalSeconds:F0}s）" : "推送失败: " + r.Message,
                    r.Pushed ? ToolTipIcon.Info : ToolTipIcon.Warning);
            }
            catch { }
        };
    }

    private static bool SafeIsAutostart()
    {
        try { return Autostart.IsEnabled(); } catch { return false; }
    }

    public void ShowForm(string? tab, bool importImage = false)
    {
        if (_form is null || _form.IsDisposed) _form = new EpdHub.App.MainForm(_sp);
        _form.Show();
        if (_form.WindowState == FormWindowState.Minimized) _form.WindowState = FormWindowState.Normal;
        _form.Activate();
        if (tab is not null) _form.SelectTab(tab);
        if (importImage) _form.BeginInvoke(() => _form.ImportImage());
    }

    private void OpenPreview()
    {
        var path = _display.Status.PreviewPath;
        if (!File.Exists(path)) { _display.RenderCurrent(); }
        if (File.Exists(path)) Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
    }

    private void Exit()
    {
        _icon.Visible = false;
        _icon.Dispose();
        _form?.Close();
        ExitThread();
    }

    /// <summary>A small e-paper glyph: white card, black frame, red top bar and two text lines.</summary>
    private static Icon CreateIcon()
    {
        using var bmp = new Bitmap(32, 32);
        using (var g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.Clear(Color.Transparent);
            using var path = RoundedRect(new Rectangle(2, 4, 28, 24), 4);
            g.FillPath(Brushes.White, path);
            using var pen = new Pen(Color.Black, 2f);
            g.DrawPath(pen, path);
            g.FillRectangle(Brushes.Red, 5, 7, 22, 4);
            g.FillRectangle(Brushes.Black, 6, 14, 18, 2);
            g.FillRectangle(Brushes.Black, 6, 19, 13, 2);
        }
        return Icon.FromHandle(bmp.GetHicon());
    }

    private static GraphicsPath RoundedRect(Rectangle r, int radius)
    {
        var p = new GraphicsPath();
        int d = radius * 2;
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }
}
