# EpdHub — Windows host for the EPD-nRF5 tag

A .NET 8 tray application that renders a fixed 400×300 black/white/red layout (or a user picture),
pushes it to the tag over Bluetooth LE, exposes the content as MCP tools for AI agents, and can
flash firmware over the air.

```
windows/
  EpdHub/          C# project (net8.0-windows, WinForms tray + WinRT BLE + ASP.NET Core + MCP SDK)
  build.sh         build/publish inside the mcr.microsoft.com/dotnet/sdk:8.0 container
  publish/         self-contained win-x64 output (created by build.sh, git-ignored)
```

## Build (Docker, from WSL)

```sh
cd /mnt/d/Users/NT/Desktop/EPD-nRF5/windows
sh ./build.sh build      # compile only
sh ./build.sh publish    # -> windows/publish/EpdHub.exe (self-contained)
```

The project targets `net8.0-windows10.0.19041.0` with `EnableWindowsTargeting=true` and
`UseWindowsForms=true`, so the Linux SDK image can build it; the exe only runs on Windows 10 19041+.

## Run

`EpdHub.exe` with no arguments starts the **tray app**: scheduler, MCP server and the control panel
(double-click the tray icon). Closing the window hides it; "退出" in the tray menu stops the app.
`EpdHub.exe tray --show` opens the control panel immediately.

Control panel tabs:

| tab | what it does |
|---|---|
| 状态 | device / firmware / battery / last push, live log, 立即推送, 强制刷屏, 读取设备信息, 开机自启 checkbox |
| 场景 JSON | free layout editor with preview (same JSON as `render_scene`), 载入示例 |
| 文字内容 | edit title / subtitle / lines / footer of the template, preview, push |
| 图片 | 导入图片… → fit (裁剪填满 / 留白适应), rotate, auto-rotate portrait, colour (黑白红抖动 / 黑白抖动 / 黑白阈值), auto-contrast, brightness → 推送到屏幕 |
| 固件升级 | pick a nrfutil `*-ota.zip`, 开始升级: buttonless entry → DfuTarg → Secure DFU transfer → version read-back |

CLI (output goes to the console you start it from):

```
EpdHub.exe push [--demo] [--force]
EpdHub.exe image <file> [--contain] [--bw] [--no-rotate] [--no-push]
EpdHub.exe dfu <package.zip>
EpdHub.exe preview
EpdHub.exe scan
```

Only one process may talk to the tag at a time. Use the GUI for DFU while the tray app runs, or quit
the tray app before running `EpdHub.exe dfu` from the command line.

Settings live in `publish/appsettings.json`, section `Hub`:

| key | meaning |
|---|---|
| `DeviceAddress` | tag MAC (`F8:71:B0:51:F3:D2`). Empty = connect to the first `NRF_EPD_*` seen |
| `ModelId` | panel id for INIT, `0` = use the id stored on the tag |
| `UpdateIntervalMinutes` | scheduler period; a push only happens when the rendered frame changed |
| `MinPushIntervalSeconds` | hard floor between two pushes |
| `RefreshSettleSeconds` | how long to stay connected after REFRESH (BWR panel ≈ 20 s) |
| `DataDir` | where `content.json`, `image.png`, `preview.png`, `state.json`, `epdhub.log` live, default `%LOCALAPPDATA%\EpdHub` |
| `ScanTimeoutSeconds` | how long to wait for the tag's advertisement (discovery took 0.1..40 s on this PC), default 60 |
| `PushAttempts` | connection attempts per push, default 3 |
| `HealthCheckMinutes` | when the tag was silent this long, connect and read battery / mode / uptime; a reboot (calendar or blank screen) is repaired by a forced push. 0 = off |
| `AlertAfterFailures` | tray warning after this many consecutive failed contacts (and an all-clear when it recovers) |
| `McpUrl` | listen URL, default `http://127.0.0.1:5077` |

While running: `http://127.0.0.1:5077/status` (JSON), `/preview.png`, MCP endpoint `/mcp`.

## MCP tools

| tool | purpose |
|---|---|
| `get_display_spec` | size, colours, the **scene JSON guide** (`SceneGuide`) and the text-template limits |
| `get_content` | content currently stored (mode, scene JSON or template fields) |
| `render_scene(scene, source?, pushNow=true)` | **free layout**: draw a scene JSON and push it; returns warnings |
| `preview_scene(scene)` | render a scene to `scene-preview.png` without touching the display |
| `set_content(title, lines[], subtitle?, footer?, source?, pushNow=true)` | simple text template; `!line` = red, `label\|text` = bold label |
| `set_image(path, fit='cover', color='three_color', autoRotate=true, pushNow=true)` | full-screen picture (file path, URL or data URI) |
| `push_now(force=false)` | re-render and push; `force` refreshes even if unchanged |
| `get_status` | firmware, **battery mV / %**, last push, errors, content mode, preview path |
| `read_device` | connect now and read firmware/config/battery |

### Scene JSON (free layout)

```json
{ "background": "white",
  "elements": [
    { "type": "rect",    "x": 0,  "y": 0,  "w": 400, "h": 40, "fill": "black" },
    { "type": "text",    "x": 12, "y": 6,  "w": 280, "h": 28, "text": "今日概览", "size": 22, "bold": true, "color": "white", "valign": "middle" },
    { "type": "battery", "x": 340, "y": 13, "color": "white", "showText": false },
    { "type": "emoji",   "x": 14, "y": 54, "text": "☀️", "size": 46 },
    { "type": "text",    "x": 76, "y": 56, "text": "东京 24°C 晴", "size": 22, "bold": true },
    { "type": "line",    "x": 12, "y": 118, "x2": 388, "y2": 118, "dashed": true },
    { "type": "image",   "x": 300, "y": 130, "w": 88, "h": 60, "src": "C:/pics/logo.png", "fit": "contain", "dither": "bw" },
    { "type": "bar",     "x": 14, "y": 218, "w": 220, "h": 14, "value": 0.7 },
    { "type": "qr",      "x": 300, "y": 196, "w": 88, "text": "https://example.com" }
  ] }
```

Element types: `text`, `emoji` (Segoe UI Emoji outlines), `icon` (Segoe MDL2 glyph code), `rect`, `ellipse`,
`line`, `image` (file / URL / data URI, dithered), `bar`, `battery` (uses the last measured voltage), `qr`.
Colours are `black`, `white`, `red`. The same JSON can be edited in the 场景 JSON tab of the control panel.

### Battery

Firmware 0x1b adds command `0x93 GET_INFO` which answers `v=<mV> m=<mode>`. EpdHub reads it on every
push and on 读取设备信息; the value shows in the status tab, `get_status`, the template footer and the
`battery` scene element. Percent uses the firmware's own curve (≈3.0 V = 100 %, 2.0 V = 0 %).

Register with Claude Code (HTTP transport):

```
claude mcp add --transport http epd http://127.0.0.1:5077/mcp
```

Inside this repository no registration is needed: the root `.mcp.json` declares the `epd` server, so
Claude Code picks it up automatically (approve it once when asked). The server also sends MCP
`instructions` at handshake (see `HostRunner.ServerInstructions`) with the usage rules: read the spec
first, prefer `render_scene`, iterate with `preview_scene`, do not refresh needlessly.

## Notes

* The firmware is used as-is (picture mode). Never send `SET_TIME` while in picture mode:
  the firmware switches mode and redraws a blank calendar.
* Transfer uses the firmware's RLE; a text frame is ~12 packets, a dithered photo ~70. The remaining
  time is the panel's own full refresh, which cannot be shortened on a three-colour panel.
* WinRT keeps the BLE link open while any GATT object is alive. `EpdClient.DisposeAsync` forces a GC
  for that reason; without it the tag stops advertising until the process exits.
* Autostart writes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\EpdHub`.
* Reliability: failed pushes keep the content stored and are retried on the next scheduler tick; the
  hourly health check uses the firmware clock reported by INIT (starts at 2025-01-01 on every boot) as
  an uptime counter to detect reboots. The firmware never sleeps in this configuration (wakeup pin 0xFF).
