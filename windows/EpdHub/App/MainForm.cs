using System.Drawing;
using EpdHub.Ble;
using EpdHub.Render;
using EpdHub.Service;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

namespace EpdHub.App;

/// <summary>Control panel: status/log, text content editor, picture import, firmware update.</summary>
public sealed class MainForm : Form
{
    public const string TabStatus = "status", TabContent = "content", TabImage = "image", TabFirmware = "firmware";

    private readonly DisplayService _display;
    private readonly ContentStore _store;
    private readonly ILogger _log;
    private readonly TabControl _tabs = new() { Dock = DockStyle.Fill };
    private readonly System.Windows.Forms.Timer _timer = new() { Interval = 2000 };

    // status tab
    private readonly Label _lblDevice = new(), _lblFw = new(), _lblLast = new(), _lblHash = new(), _lblMode = new(), _lblMcp = new(), _lblBattery = new(), _lblUpdated = new(), _lblHealth = new();
    private readonly Label _contentNotice = new() { AutoSize = true, ForeColor = Color.DarkRed, Margin = new Padding(0, 6, 0, 0) };
    private readonly PictureBox _statusPreview = new() { Size = new Size(400, 300), BorderStyle = BorderStyle.FixedSingle, SizeMode = PictureBoxSizeMode.Normal };
    private readonly Label _statusPreviewCaption = new() { AutoSize = true, Text = "当前画面" };

    // change tracking: what the editors currently reflect, so external (MCP) updates can be merged safely
    private string? _loadedPreviewHash;
    private string? _loadedContentStamp;
    private string _sceneLoadedText = "";
    private string _templateLoadedSnapshot = "";

    // scene tab
    private readonly TextBox _sceneJson = new() { Multiline = true, ScrollBars = ScrollBars.Both, AcceptsReturn = true, AcceptsTab = true, WordWrap = false, Font = new Font("Consolas", 9.5f) };
    private readonly PictureBox _scenePreview = new() { Size = new Size(400, 300), BorderStyle = BorderStyle.FixedSingle, SizeMode = PictureBoxSizeMode.Normal };
    private readonly Label _sceneWarnings = new() { AutoSize = true, MaximumSize = new Size(400, 0), ForeColor = Color.DarkRed };
    private readonly CheckBox _chkAutostart = new() { Text = "开机自启（登录后在托盘运行）", AutoSize = true };
    private readonly TextBox _logBox = new() { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill, Font = new Font("Consolas", 9f) };

    // content tab
    private readonly TextBox _title = new(), _subtitle = new(), _footer = new();
    private readonly TextBox _lines = new() { Multiline = true, ScrollBars = ScrollBars.Vertical, AcceptsReturn = true };
    private readonly PictureBox _preview = new() { Size = new Size(400, 300), BorderStyle = BorderStyle.FixedSingle, SizeMode = PictureBoxSizeMode.Normal };

    // image tab
    private readonly PictureBox _imgSrc = new() { Size = new Size(280, 210), BorderStyle = BorderStyle.FixedSingle, SizeMode = PictureBoxSizeMode.Zoom };
    private readonly PictureBox _imgOut = new() { Size = new Size(400, 300), BorderStyle = BorderStyle.FixedSingle, SizeMode = PictureBoxSizeMode.Normal };
    private readonly ComboBox _fit = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 140 };
    private readonly ComboBox _color = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 140 };
    private readonly ComboBox _rotate = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 80 };
    private readonly CheckBox _autoRotate = new() { Text = "竖图自动横放", AutoSize = true, Checked = true };
    private readonly CheckBox _autoContrast = new() { Text = "自动对比度", AutoSize = true, Checked = true };
    private readonly TrackBar _brightness = new() { Minimum = -100, Maximum = 100, Value = 0, TickFrequency = 25, Width = 160 };
    private readonly Label _imgInfo = new() { AutoSize = true, Text = "未导入图片" };
    private readonly Button _btnPushImage = new() { Text = "推送到屏幕", Enabled = false, Width = 120 };
    private Bitmap? _source;
    private Bitmap? _processed;

    // firmware tab
    private readonly Label _lblFwInfo = new() { AutoSize = true, Text = "设备固件: 未知（点“读取设备信息”）" };
    private readonly TextBox _zip = new() { Width = 420 };
    private readonly ProgressBar _bar = new() { Width = 420, Height = 18 };
    private readonly Label _dfuStatus = new() { AutoSize = true, Text = "" };
    private readonly Button _btnDfu = new() { Text = "开始升级", Width = 120 };

    public MainForm(IServiceProvider sp)
    {
        _display = sp.GetRequiredService<DisplayService>();
        _store = sp.GetRequiredService<ContentStore>();
        _log = sp.GetRequiredService<ILoggerFactory>().CreateLogger("GUI");

        Text = "EpdHub 控制面板";
        Font = new Font("Microsoft YaHei UI", 9f);
        StartPosition = FormStartPosition.CenterScreen;
        ClientSize = new Size(900, 680);
        MinimumSize = new Size(820, 560);
        Icon = null;

        _tabs.TabPages.Add(BuildStatusTab());
        _tabs.TabPages.Add(BuildSceneTab());
        _tabs.TabPages.Add(BuildContentTab());
        _tabs.TabPages.Add(BuildImageTab());
        _tabs.TabPages.Add(BuildFirmwareTab());
        Controls.Add(_tabs);

        UiLog.Instance.LineAdded += OnLogLine;
        foreach (var l in UiLog.Instance.Lines.TakeLast(200)) _logBox.AppendText(l + Environment.NewLine);

        _timer.Tick += (_, _) => { RefreshStatus(); SyncFromStore(); };
        _timer.Start();
        RefreshStatus();
        LoadContentFields();
        LoadPreview();

        FormClosing += (_, e) =>
        {
            // Closing the window keeps the tray app alive; hide instead.
            if (e.CloseReason == CloseReason.UserClosing) { e.Cancel = true; Hide(); }
        };
        FormClosed += (_, _) => { UiLog.Instance.LineAdded -= OnLogLine; _timer.Stop(); };
    }

    public void SelectTab(string name)
    {
        foreach (TabPage p in _tabs.TabPages)
            if ((string?)p.Tag == name) { _tabs.SelectedTab = p; break; }
    }

    // ------------------------------------------------------------------ status tab

    private TabPage BuildStatusTab()
    {
        var page = new TabPage("状态") { Tag = TabStatus, Padding = new Padding(10) };
        var grid = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 2 };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 90));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        void Row(string caption, Label value)
        {
            value.AutoSize = true; value.Margin = new Padding(3, 6, 3, 6); value.MaximumSize = new Size(330, 0);
            grid.Controls.Add(new Label { Text = caption, AutoSize = true, Margin = new Padding(3, 6, 3, 6), ForeColor = SystemColors.GrayText });
            grid.Controls.Add(value);
        }
        Row("设备", _lblDevice);
        Row("固件版本", _lblFw);
        Row("上次推送", _lblLast);
        Row("当前帧", _lblHash);
        Row("内容模式", _lblMode);
        Row("电池", _lblBattery);
        Row("最近更新", _lblUpdated);
        Row("连接健康", _lblHealth);
        Row("MCP 地址", _lblMcp);

        var buttons = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(0, 8, 0, 8) };
        var btnPush = new Button { Text = "立即推送", Width = 110 };
        btnPush.Click += (_, _) => _display.RequestPush(false);
        var btnForce = new Button { Text = "强制刷屏", Width = 110 };
        btnForce.Click += (_, _) => _display.RequestPush(true);
        var btnInfo = new Button { Text = "读取设备信息", Width = 120 };
        btnInfo.Click += async (_, _) => await ReadDeviceInfoAsync(btnInfo);
        var btnCopy = new Button { Text = "复制 MCP 注册命令", Width = 160 };
        btnCopy.Click += (_, _) =>
        {
            Clipboard.SetText($"claude mcp add --transport http epd {_display.Options.McpUrl}/mcp");
            btnCopy.Text = "已复制";
        };
        buttons.Controls.AddRange(new Control[] { btnPush, btnForce, btnInfo, btnCopy });

        _chkAutostart.Checked = SafeIsAutostart();
        _chkAutostart.CheckedChanged += (_, _) =>
        {
            try { Autostart.Set(_chkAutostart.Checked); }
            catch (Exception ex) { MessageBox.Show(this, "写入自启动失败: " + ex.Message, "EpdHub"); }
        };
        var autoPanel = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true };
        autoPanel.Controls.Add(_chkAutostart);

        var logLabel = new Label { Text = "日志", Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(0, 8, 0, 2) };

        // Top area: info column on the left, the frame currently on the panel on the right.
        var info = new Panel { Dock = DockStyle.Fill, AutoSize = true };
        info.Controls.Add(autoPanel);
        info.Controls.Add(buttons);
        info.Controls.Add(grid);
        var side = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true };
        side.Controls.Add(_statusPreviewCaption);
        side.Controls.Add(_statusPreview);
        var top = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 2, Height = 330 };
        top.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        top.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 410));
        top.Controls.Add(info, 0, 0);
        top.Controls.Add(side, 1, 0);

        // Dock order: last added = top-most in the docking stack, so add in reverse.
        page.Controls.Add(_logBox);
        page.Controls.Add(logLabel);
        page.Controls.Add(top);
        return page;
    }

    private static bool SafeIsAutostart()
    {
        try { return Autostart.IsEnabled(); } catch { return false; }
    }

    private void RefreshStatus()
    {
        var s = _display.Status;
        _lblDevice.Text = string.IsNullOrEmpty(s.DeviceAddress) ? "自动（按名称扫描）" : s.DeviceAddress;
        _lblFw.Text = s.DeviceFirmware is null ? "未知" :
            $"{s.DeviceFirmware}   型号 0x{s.DeviceConfig?.ModelId ?? 0:x2}   模式 {(s.DeviceConfig?.DisplayMode == 0 ? "图片" : "日历")}";
        _lblLast.Text = s.LastPushAt is null ? "从未" :
            $"{s.LastPushAt:yyyy-MM-dd HH:mm:ss}   {s.LastResult}   共 {s.PushCount} 次" + (s.LastError is null ? "" : "   错误: " + s.LastError);
        _lblHash.Text = (s.CurrentFrameHash ?? "-") + (s.Busy ? "   （忙碌中…）" : s.CurrentFrameHash == s.LastPushHash ? "   已同步到屏幕" : "   未推送");
        _lblMode.Text = s.ContentMode switch { DisplayContent.ModeImage => "图片", DisplayContent.ModeScene => "场景 JSON", _ => "文字模板" };
        _lblBattery.Text = s.BatteryMv is null ? "未知（需固件 0x1b+，推送或读取设备信息后可见）" :
            $"{s.BatteryMv / 1000.0:F2} V   约 {s.BatteryPercent}%   （{s.BatteryReadAt:HH:mm:ss} 测得）";
        _lblMcp.Text = _display.Options.McpUrl + "/mcp";
        var c = _store.Load();
        _lblUpdated.Text = $"{c.UpdatedAt:yyyy-MM-dd HH:mm:ss}   来源 {c.Source ?? "-"}   模式 {ModeName(c.Mode)}";
        string up = s.DeviceUptime is { } u ? $"设备已运行 {(int)u.TotalHours}h{u.Minutes:D2}m" : "设备运行时长未知";
        _lblHealth.Text = (s.ConsecutiveFailures > 0 ? $"连续失败 {s.ConsecutiveFailures} 次   " : "正常   ") +
            $"最近联系 {(s.LastContactAt is { } lc ? lc.ToString("HH:mm:ss") : "-")}   {up}" +
            (s.LastHealthCheckAt is { } hc ? $"   检查 {hc:HH:mm} {s.LastHealthResult}" : "");
        _lblHealth.ForeColor = s.ConsecutiveFailures >= 3 ? Color.DarkRed : SystemColors.ControlText;
        _statusPreviewCaption.Text = s.Busy ? "当前画面（推送中…）" :
            s.CurrentFrameHash == s.LastPushHash ? "当前画面（已在屏幕上）" : "当前画面（待推送，屏幕仍是上一帧）";
    }

    private static string ModeName(string mode) => mode switch
    {
        DisplayContent.ModeImage => "图片", DisplayContent.ModeScene => "场景 JSON", _ => "文字模板",
    };

    /// <summary>Pick up content changed by MCP / CLI: refresh previews, reload editors unless they have unsaved edits.</summary>
    private void SyncFromStore()
    {
        var s = _display.Status;
        if (s.CurrentFrameHash != _loadedPreviewHash)
        {
            _loadedPreviewHash = s.CurrentFrameHash;
            LoadPreview();
        }

        var c = _store.Load();
        string stamp = $"{c.UpdatedAt:O}|{c.Mode}|{c.Source}";
        if (stamp == _loadedContentStamp) return;
        bool first = _loadedContentStamp is null;
        _loadedContentStamp = stamp;
        if (first || c.Source == "gui") return;   // our own save, editors already match

        string who = $"内容已由 {c.Source ?? "外部程序"} 在 {c.UpdatedAt:HH:mm:ss} 更新（{ModeName(c.Mode)}）";
        bool sceneClean = _sceneJson.Text == _sceneLoadedText;
        bool templateClean = TemplateSnapshot() == _templateLoadedSnapshot;

        if (c.Mode == DisplayContent.ModeScene && !string.IsNullOrEmpty(c.SceneJson))
        {
            if (sceneClean) { _sceneJson.Text = c.SceneJson; _sceneLoadedText = c.SceneJson; _sceneWarnings.Text = who; }
            else _sceneWarnings.Text = who + "，你有未保存的编辑，点“载入当前”查看新内容";
        }
        else if (c.Mode == DisplayContent.ModeTemplate)
        {
            if (templateClean) { LoadContentFields(); _contentNotice.Text = who; }
            else _contentNotice.Text = who + "，你有未保存的编辑，点“重新载入”查看新内容";
        }
        else
        {
            _contentNotice.Text = who;
        }
    }

    private string TemplateSnapshot() => string.Join("\u0001", _title.Text, _subtitle.Text, _footer.Text, _lines.Text);

    private void OnLogLine(string line)
    {
        if (IsDisposed) return;
        try
        {
            BeginInvoke(() =>
            {
                if (_logBox.TextLength > 200_000) _logBox.Clear();
                _logBox.AppendText(line + Environment.NewLine);
            });
        }
        catch { }
    }

    private async Task ReadDeviceInfoAsync(Button btn)
    {
        btn.Enabled = false;
        try
        {
            var (ver, cfg) = await _display.ReadDeviceInfoAsync(CancellationToken.None);
            _lblFwInfo.Text = $"设备固件: 0x{ver:x2}   型号 0x{cfg?.ModelId ?? 0:x2}   引脚 {cfg?.Mosi}/{cfg?.Sclk}/{cfg?.Cs}/{cfg?.Dc}/{cfg?.Rst}/{cfg?.Busy}";
            RefreshStatus();
        }
        catch (Exception ex) { MessageBox.Show(this, "读取失败: " + ex.Message, "EpdHub", MessageBoxButtons.OK, MessageBoxIcon.Warning); }
        finally { btn.Enabled = true; }
    }

    // ------------------------------------------------------------------ scene tab

    private TabPage BuildSceneTab()
    {
        var page = new TabPage("场景 JSON") { Tag = "scene", Padding = new Padding(10) };
        var split = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2 };
        split.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        split.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 420));

        var left = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1 };
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        left.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        left.Controls.Add(new Label { Text = "自由排版：元素列表 JSON（AI 通过 MCP 的 render_scene 提交的也是同样的内容）", AutoSize = true });
        _sceneJson.Dock = DockStyle.Fill; _sceneJson.Margin = new Padding(0, 4, 8, 4);
        left.Controls.Add(_sceneJson);
        var btns = new FlowLayoutPanel { AutoSize = true };
        var btnPreview = new Button { Text = "预览", Width = 90 };
        btnPreview.Click += (_, _) => PreviewScene();
        var btnPush = new Button { Text = "保存并推送", Width = 110 };
        btnPush.Click += async (_, _) =>
        {
            if (!SaveScene()) return;
            btnPush.Enabled = false;
            try
            {
                var r = await Task.Run(() => _display.PushAsync(false, CancellationToken.None));
                LoadPreview(); RefreshStatus();
                if (!r.Pushed) MessageBox.Show(this, r.Message == "unchanged" ? "内容没有变化，屏幕未刷新。" : r.Message, "EpdHub");
            }
            finally { btnPush.Enabled = true; }
        };
        var btnExample = new Button { Text = "载入示例", Width = 90 };
        btnExample.Click += (_, _) => { _sceneJson.Text = SceneExample(); PreviewScene(); };
        var btnLoad = new Button { Text = "载入当前", Width = 90 };
        btnLoad.Click += (_, _) =>
        {
            var c = _store.Load();
            if (!string.IsNullOrEmpty(c.SceneJson)) { _sceneJson.Text = c.SceneJson; _sceneLoadedText = c.SceneJson; _sceneWarnings.Text = ""; PreviewScene(); }
        };
        btns.Controls.AddRange(new Control[] { btnPreview, btnPush, btnExample, btnLoad });
        left.Controls.Add(btns);

        var right = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false };
        right.Controls.Add(new Label { Text = "预览（屏幕实际效果）", AutoSize = true });
        right.Controls.Add(_scenePreview);
        right.Controls.Add(_sceneWarnings);

        split.Controls.Add(left, 0, 0);
        split.Controls.Add(right, 1, 0);
        page.Controls.Add(split);

        var current = _store.Load();
        _sceneJson.Text = string.IsNullOrEmpty(current.SceneJson) ? SceneExample() : current.SceneJson;
        _sceneLoadedText = _sceneJson.Text;
        return page;
    }

    private static string SceneExample() => """
        {
          "background": "white",
          "elements": [
            { "type": "rect", "x": 0, "y": 0, "w": 400, "h": 40, "fill": "black" },
            { "type": "text", "x": 12, "y": 6, "w": 280, "h": 28, "text": "今日概览", "size": 22, "bold": true, "color": "white", "valign": "middle" },
            { "type": "battery", "x": 340, "y": 13, "color": "white", "showText": false },
            { "type": "line", "x": 0, "y": 40, "x2": 400, "y2": 40, "width": 3, "color": "red" },
            { "type": "emoji", "x": 14, "y": 54, "text": "☀️", "size": 46 },
            { "type": "text", "x": 76, "y": 56, "text": "东京 24°C 晴", "size": 22, "bold": true },
            { "type": "text", "x": 76, "y": 86, "text": "湿度 55%  ·  东风 3 m/s", "size": 14 },
            { "type": "line", "x": 12, "y": 118, "x2": 388, "y2": 118, "dashed": true },
            { "type": "emoji", "x": 14, "y": 128, "text": "📅", "size": 24 },
            { "type": "text", "x": 48, "y": 132, "w": 340, "text": "15:00 周会  ·  17:30 牙医", "size": 16 },
            { "type": "emoji", "x": 14, "y": 160, "text": "✅", "size": 24 },
            { "type": "text", "x": 48, "y": 164, "w": 340, "text": "EPD-nRF5 固件 PR 待审", "size": 16, "color": "red" },
            { "type": "text", "x": 14, "y": 200, "text": "本周进度", "size": 13 },
            { "type": "bar", "x": 14, "y": 218, "w": 220, "h": 14, "value": 0.7 },
            { "type": "text", "x": 240, "y": 216, "text": "70%", "size": 14, "bold": true },
            { "type": "qr", "x": 300, "y": 196, "w": 88, "text": "https://github.com/tsl0922/EPD-nRF5" },
            { "type": "text", "x": 14, "y": 276, "w": 280, "text": "数据源: 天气 / 日历 / 券商 API", "size": 11 }
          ]
        }
        """;

    private bool SaveScene()
    {
        try { Scene.Parse(_sceneJson.Text); }
        catch (Exception ex) { MessageBox.Show(this, "JSON 无效: " + ex.Message, "EpdHub", MessageBoxButtons.OK, MessageBoxIcon.Warning); return false; }
        var c = _store.Load();
        c.Mode = DisplayContent.ModeScene;
        c.SceneJson = _sceneJson.Text;
        c.Source = "gui";
        c.UpdatedAt = DateTimeOffset.Now;
        _store.Save(c);
        _sceneLoadedText = _sceneJson.Text;
        _sceneWarnings.Text = "";
        return true;
    }

    private void PreviewScene()
    {
        try
        {
            var scene = Scene.Parse(_sceneJson.Text);
            var warnings = new List<string>();
            var (_, path) = _display.RenderScenePreview(scene, warnings);
            using var fs = File.OpenRead(path);
            using var img = Image.FromStream(fs);
            _scenePreview.Image?.Dispose();
            _scenePreview.Image = new Bitmap(img);
            _sceneWarnings.Text = warnings.Count == 0 ? "" : string.Join(Environment.NewLine, warnings);
        }
        catch (Exception ex)
        {
            _sceneWarnings.Text = "错误: " + ex.Message;
        }
    }

    // ------------------------------------------------------------------ content tab

    private TabPage BuildContentTab()
    {
        var page = new TabPage("文字内容") { Tag = TabContent, Padding = new Padding(10) };
        var split = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2 };
        split.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        split.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 420));

        var left = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, AutoSize = true };
        void Field(string caption, TextBox box)
        {
            left.Controls.Add(new Label { Text = caption, AutoSize = true, Margin = new Padding(0, 6, 0, 0) });
            box.Dock = DockStyle.Top; box.Margin = new Padding(0, 0, 8, 0);
            left.Controls.Add(box);
        }
        Field("标题", _title);
        Field("副标题（可空）", _subtitle);
        left.Controls.Add(new Label { Text = "正文（每行一条；行首 ! 为红色；“标签|文字” 加粗标签）", AutoSize = true, Margin = new Padding(0, 6, 0, 0) });
        _lines.Height = 170; _lines.Dock = DockStyle.Top; _lines.Margin = new Padding(0, 0, 8, 0);
        left.Controls.Add(_lines);
        Field("页脚（可空）", _footer);

        var btns = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 10, 0, 0) };
        var btnPreview = new Button { Text = "保存并预览", Width = 110 };
        btnPreview.Click += (_, _) => { SaveContent(); _display.RenderCurrent(); LoadPreview(); RefreshStatus(); };
        var btnPush = new Button { Text = "保存并推送", Width = 110 };
        btnPush.Click += async (_, _) =>
        {
            SaveContent();
            btnPush.Enabled = false;
            try
            {
                var r = await Task.Run(() => _display.PushAsync(false, CancellationToken.None));
                LoadPreview(); RefreshStatus();
                if (!r.Pushed) MessageBox.Show(this, r.Message == "unchanged" ? "内容没有变化，屏幕未刷新。" : r.Message, "EpdHub");
            }
            finally { btnPush.Enabled = true; }
        };
        var btnReload = new Button { Text = "重新载入", Width = 90 };
        btnReload.Click += (_, _) => LoadContentFields();
        btns.Controls.AddRange(new Control[] { btnPreview, btnPush, btnReload });
        left.Controls.Add(btns);
        left.Controls.Add(_contentNotice);

        var right = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown };
        right.Controls.Add(new Label { Text = "预览（屏幕实际效果）", AutoSize = true });
        right.Controls.Add(_preview);

        split.Controls.Add(left, 0, 0);
        split.Controls.Add(right, 1, 0);
        page.Controls.Add(split);
        return page;
    }

    private void LoadContentFields()
    {
        var c = _store.Load();
        _title.Text = c.Title;
        _subtitle.Text = c.Subtitle ?? "";
        _footer.Text = c.Footer ?? "";
        _lines.Text = string.Join(Environment.NewLine, c.Lines.Select(l => l.ToSyntax()));
        _templateLoadedSnapshot = TemplateSnapshot();
    }

    private void SaveContent()
    {
        var c = new DisplayContent
        {
            Mode = DisplayContent.ModeTemplate,
            Title = _title.Text,
            Subtitle = _subtitle.Text,
            Footer = _footer.Text,
            Source = "gui",
            UpdatedAt = DateTimeOffset.Now,
            Lines = _lines.Text.Split('\n').Select(s => s.TrimEnd('\r')).Where(s => s.Trim().Length > 0).Select(DisplayLine.Parse).ToList(),
        };
        _store.Save(c);
        _templateLoadedSnapshot = TemplateSnapshot();
        _contentNotice.Text = "";
    }

    private void LoadPreview()
    {
        try
        {
            if (!File.Exists(_store.PreviewPath)) return;
            using var fs = File.OpenRead(_store.PreviewPath);
            using var img = Image.FromStream(fs);
            _preview.Image?.Dispose();
            _preview.Image = new Bitmap(img);
            _statusPreview.Image?.Dispose();
            _statusPreview.Image = new Bitmap(img);
            if (_store.Load().Mode == DisplayContent.ModeScene)
            {
                _scenePreview.Image?.Dispose();
                _scenePreview.Image = new Bitmap(img);
            }
        }
        catch { }
    }

    // ------------------------------------------------------------------ image tab

    private TabPage BuildImageTab()
    {
        var page = new TabPage("图片") { Tag = TabImage, Padding = new Padding(10) };

        _fit.Items.AddRange(new object[] { "裁剪填满", "留白适应" }); _fit.SelectedIndex = 0;
        _color.Items.AddRange(new object[] { "黑白红抖动", "黑白抖动", "黑白阈值" }); _color.SelectedIndex = 0;
        _rotate.Items.AddRange(new object[] { "0°", "90°", "180°", "270°" }); _rotate.SelectedIndex = 0;

        var controls = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = true };
        var btnImport = new Button { Text = "导入图片…", Width = 110 };
        btnImport.Click += (_, _) => ImportImage();
        controls.Controls.Add(btnImport);
        controls.Controls.Add(new Label { Text = "适配", AutoSize = true, Margin = new Padding(12, 8, 3, 0) });
        controls.Controls.Add(_fit);
        controls.Controls.Add(new Label { Text = "颜色", AutoSize = true, Margin = new Padding(12, 8, 3, 0) });
        controls.Controls.Add(_color);
        controls.Controls.Add(new Label { Text = "旋转", AutoSize = true, Margin = new Padding(12, 8, 3, 0) });
        controls.Controls.Add(_rotate);
        _autoRotate.Margin = new Padding(12, 7, 3, 0); controls.Controls.Add(_autoRotate);
        _autoContrast.Margin = new Padding(12, 7, 3, 0); controls.Controls.Add(_autoContrast);
        controls.Controls.Add(new Label { Text = "亮度", AutoSize = true, Margin = new Padding(12, 8, 3, 0) });
        controls.Controls.Add(_brightness);
        _btnPushImage.Margin = new Padding(20, 3, 3, 3);
        controls.Controls.Add(_btnPushImage);

        foreach (var cb in new[] { _fit, _color, _rotate }) cb.SelectedIndexChanged += (_, _) => Reprocess();
        _autoRotate.CheckedChanged += (_, _) => Reprocess();
        _autoContrast.CheckedChanged += (_, _) => Reprocess();
        _brightness.MouseUp += (_, _) => Reprocess();
        _brightness.KeyUp += (_, _) => Reprocess();
        _btnPushImage.Click += async (_, _) => await PushImageAsync();

        var pics = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoScroll = true };
        var srcPanel = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true };
        srcPanel.Controls.Add(new Label { Text = "原图", AutoSize = true });
        srcPanel.Controls.Add(_imgSrc);
        srcPanel.Controls.Add(_imgInfo);
        var outPanel = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true, Margin = new Padding(20, 0, 0, 0) };
        outPanel.Controls.Add(new Label { Text = "处理结果（400×300，屏幕实际效果）", AutoSize = true });
        outPanel.Controls.Add(_imgOut);
        pics.Controls.Add(srcPanel);
        pics.Controls.Add(outPanel);

        page.Controls.Add(pics);
        page.Controls.Add(controls);
        return page;
    }

    public void ImportImage()
    {
        using var dlg = new OpenFileDialog
        {
            Title = "选择图片",
            Filter = "图片|*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp;*.tif;*.tiff|所有文件|*.*",
        };
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        try
        {
            var bmp = ImageProcessor.Load(dlg.FileName);
            _source?.Dispose();
            _source = bmp;
            _imgSrc.Image = _source;
            _imgInfo.Text = $"{Path.GetFileName(dlg.FileName)}  {bmp.Width}×{bmp.Height}";
            _imgInfo.Tag = dlg.FileName;
            Reprocess();
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, "无法读取图片: " + ex.Message, "EpdHub", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private ImageOptions CurrentImageOptions() => new()
    {
        Fit = _fit.SelectedIndex == 1 ? FitMode.Contain : FitMode.Cover,
        Color = _color.SelectedIndex switch { 1 => ColorMode.BwDither, 2 => ColorMode.BwThreshold, _ => ColorMode.ThreeColorDither },
        Rotate = _rotate.SelectedIndex * 90,
        AutoRotate = _autoRotate.Checked,
        AutoContrast = _autoContrast.Checked,
        Brightness = _brightness.Value,
    };

    private void Reprocess()
    {
        if (_source is null) return;
        try
        {
            var result = ImageProcessor.Process(_source, _display.Spec.Width, _display.Spec.Height, CurrentImageOptions());
            _processed?.Dispose();
            _processed = result;
            _imgOut.Image = _processed;
            _btnPushImage.Enabled = true;
        }
        catch (Exception ex)
        {
            _log.LogWarning(ex, "image processing failed");
        }
    }

    private async Task PushImageAsync()
    {
        if (_processed is null) return;
        _btnPushImage.Enabled = false;
        try
        {
            var name = _imgInfo.Tag as string ?? "image";
            var bmp = new Bitmap(_processed);   // detach from the UI-owned bitmap
            var r = await Task.Run(async () =>
            {
                using (bmp) return await _display.SetImageAsync(bmp, Path.GetFileName(name), pushNow: true, CancellationToken.None);
            });
            LoadPreview(); RefreshStatus();
            if (!r.Pushed) MessageBox.Show(this, r.Message == "unchanged" ? "这张图和屏幕上的一样，未刷新。" : r.Message, "EpdHub");
        }
        catch (Exception ex) { MessageBox.Show(this, "推送失败: " + ex.Message, "EpdHub", MessageBoxButtons.OK, MessageBoxIcon.Warning); }
        finally { _btnPushImage.Enabled = true; }
    }

    // ------------------------------------------------------------------ firmware tab

    private TabPage BuildFirmwareTab()
    {
        var page = new TabPage("固件升级") { Tag = TabFirmware, Padding = new Padding(10) };
        var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false };

        flow.Controls.Add(new Label
        {
            Text = "通过蓝牙 OTA 刷写 nrfutil 生成的 DFU 包（*-ota.zip）。设备会重启进入 bootloader（DfuTarg），传输约 1 分钟。\n" +
                   "升级期间不要关闭程序；失败时设备会停留在 bootloader 模式，可直接再次升级。",
            AutoSize = true, MaximumSize = new Size(800, 0), Margin = new Padding(0, 0, 0, 10),
        });
        var infoRow = new FlowLayoutPanel { AutoSize = true };
        var btnInfo = new Button { Text = "读取设备信息", Width = 120 };
        btnInfo.Click += async (_, _) => await ReadDeviceInfoAsync(btnInfo);
        infoRow.Controls.Add(btnInfo);
        _lblFwInfo.Margin = new Padding(10, 8, 0, 0);
        infoRow.Controls.Add(_lblFwInfo);
        flow.Controls.Add(infoRow);

        flow.Controls.Add(new Label { Text = "DFU 包", AutoSize = true, Margin = new Padding(0, 12, 0, 2) });
        var zipRow = new FlowLayoutPanel { AutoSize = true };
        zipRow.Controls.Add(_zip);
        var btnBrowse = new Button { Text = "浏览…", Width = 80 };
        btnBrowse.Click += (_, _) =>
        {
            using var dlg = new OpenFileDialog { Title = "选择 DFU 包", Filter = "DFU 包|*.zip" };
            if (dlg.ShowDialog(this) == DialogResult.OK) _zip.Text = dlg.FileName;
        };
        zipRow.Controls.Add(btnBrowse);
        flow.Controls.Add(zipRow);

        _bar.Margin = new Padding(0, 10, 0, 4);
        flow.Controls.Add(_bar);
        flow.Controls.Add(_dfuStatus);
        _btnDfu.Margin = new Padding(0, 10, 0, 0);
        _btnDfu.Click += async (_, _) => await RunDfuAsync();
        flow.Controls.Add(_btnDfu);

        page.Controls.Add(flow);
        return page;
    }

    private async Task RunDfuAsync()
    {
        string zip = _zip.Text.Trim();
        if (!File.Exists(zip)) { MessageBox.Show(this, "请先选择 DFU 包。", "EpdHub"); return; }
        try
        {
            var (init, fw, bin) = DfuClient.LoadPackage(zip);
            if (MessageBox.Show(this, $"将刷写 {bin}（{fw.Length} 字节）到设备，确定？", "EpdHub",
                    MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK) return;
        }
        catch (Exception ex) { MessageBox.Show(this, "无效的 DFU 包: " + ex.Message, "EpdHub"); return; }

        _btnDfu.Enabled = false;
        _bar.Value = 0;
        var progress = new Progress<DfuClient.Progress>(p =>
        {
            _bar.Maximum = (int)Math.Max(1, Math.Min(int.MaxValue, p.Total));
            _bar.Value = (int)Math.Min(p.Done, _bar.Maximum);
            _dfuStatus.Text = p.Total > 1 ? $"{p.Stage}  {p.Done}/{p.Total}" : p.Stage;
        });
        try
        {
            var opt = _display.Options;
            ulong addr = string.IsNullOrWhiteSpace(opt.DeviceAddress) ? 0 : EpdClient.ParseAddress(opt.DeviceAddress);
            byte? ver = null;
            await _display.RunExclusiveAsync(async ct =>
            {
                ver = await new DfuClient(_log).UpdateAsync(addr, opt.DeviceNamePrefix, zip, progress, ct);
            }, CancellationToken.None);
            _dfuStatus.Text = ver is null ? "升级完成（未能读回版本）" : $"升级完成，设备固件 0x{ver:x2}";
            if (ver is not null) _lblFwInfo.Text = $"设备固件: 0x{ver:x2}";
            // The rebooted firmware comes up in calendar mode; a forced push puts the panel back to our content.
            _display.RequestPush(force: true);
            RefreshStatus();
            MessageBox.Show(this, _dfuStatus.Text, "EpdHub", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            _dfuStatus.Text = "升级失败: " + ex.Message;
            _log.LogError(ex, "DFU failed");
            MessageBox.Show(this, "升级失败: " + ex.Message, "EpdHub", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally { _btnDfu.Enabled = true; }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _source?.Dispose();
            _processed?.Dispose();
            _timer.Dispose();
        }
        base.Dispose(disposing);
    }
}
