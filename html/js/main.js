let bleDevice;
let gattServer;
let epdService;
let epdCharacteristic;
let reconnectTrys = 0;

let canvas;
let startTime;

const MAX_PACKET_SIZE = 20;
const EpdCmd = {
  SET_PINS:  0x00,
  INIT:      0x01,
  CLEAR:     0x02,
  SEND_CMD:  0x03,
  SEND_DATA: 0x04,
  DISPLAY:   0x05,
  SLEEP:     0x06,

  SET_CONFIG: 0x90,
  SYS_RESET:  0x91,
  SYS_SLEEP:  0x92,
  CFG_ERASE:  0x99,
};

function resetVariables() {
  gattServer = null;
  epdService = null;
  epdCharacteristic = null;
  document.getElementById("log").value = '';
}

async function handleError(error) {
  console.log(error);
  resetVariables();
  if (bleDevice == null)
    return;
  if (reconnectTrys <= 5) {
    reconnectTrys++;
    await connect();
  }
  else {
    addLog("连接失败！");
    reconnectTrys = 0;
  }
}

async function write(cmd, data, withResponse=true) {
  if (!epdCharacteristic) {
    addLog("服务不可用，请检查蓝牙连接");
    return;
  }
  let payload = [cmd];
  if (data) {
    if (typeof data == 'string') data = hex2bytes(data);
    if (data instanceof Uint8Array) data = Array.from(data);
    payload.push(...data)
  };
  if (payload.length > MAX_PACKET_SIZE) {
    throw new Error("BLE packet too large!");
  }
  addLog(`<span class="action">⇑</span> ${bytes2hex(payload)}`);
  if (withResponse)
    await epdCharacteristic.writeValueWithResponse(Uint8Array.from(payload));
  else
    await epdCharacteristic.writeValueWithoutResponse(Uint8Array.from(payload));
}

async function epdWrite(cmd, data) {
  const chunkSize = MAX_PACKET_SIZE - 1;
  const count = Math.round(data.length / chunkSize);
  const interleavedCount = document.getElementById('interleavedcount').value;
  let chunkIdx = 0;
  let noReplyCount = interleavedCount;

  if (typeof data == 'string') data = hex2bytes(data);

  await write(EpdCmd.SEND_CMD, [cmd]);
  for (let i = 0; i < data.length; i += chunkSize) {
    let currentTime = (new Date().getTime() - startTime) / 1000.0;
    setStatus(`命令：0x${cmd.toString(16)}, 数据块: ${chunkIdx+1}/${count+1}, 总用时: ${currentTime}s`);
    if (noReplyCount > 0) {
      await write(EpdCmd.SEND_DATA, data.slice(i, i + chunkSize), false);
      noReplyCount--;
    } else {
      await write(EpdCmd.SEND_DATA, data.slice(i, i + chunkSize), true);
      noReplyCount = interleavedCount;
    }
    chunkIdx++;
  }
}

async function setDriver() {
  await write(EpdCmd.SET_PINS, document.getElementById("epdpins").value);
  await write(EpdCmd.INIT, document.getElementById("epddriver").value);
}

async function clearscreen() {
  if(confirm('确认清除屏幕内容?')) {
    await write(EpdCmd.CLEAR);
  }
}

async function sendcmd() {
  const cmdTXT = document.getElementById('cmdTXT').value;
  if (cmdTXT == '') return;
  const bytes = hex2bytes(cmdTXT);
  await write(bytes[0], bytes.length > 1 ? bytes.slice(1) : null);
}

async function send4GrayLut() {
  await epdWrite(0x20, "000A0000000160141400000100140000000100130A010001000000000000000000000000000000000000"); // vcom
  await epdWrite(0x21, "400A0000000190141400000110140A000001A01301000001000000000000000000000000000000000000"); // red not use
  await epdWrite(0x22, "400A0000000190141400000100140A000001990C01030401000000000000000000000000000000000000"); // bw r
  await epdWrite(0x23, "400A0000000190141400000100140A000001990B04040101000000000000000000000000000000000000"); // wb w
  await epdWrite(0x24, "800A0000000190141400000120140A000001501301000001000000000000000000000000000000000000"); // bb b
  await epdWrite(0x25, "400A0000000190141400000110140A000001A01301000001000000000000000000000000000000000000"); // vcom
}

function getImageData(canvas, driver, mode) {
  if (mode === '4gray') {
    return canvas2gray(canvas);
  } else {
    let data = canvas2bytes(canvas, 'bw');
    if (driver === '03' && mode.startsWith('bwr')) {
      data.push(...canvas2bytes(canvas, 'red'));
    }
    return data;
  }
}

async function sendimg() {
  startTime = new Date().getTime();
  const canvas = document.getElementById("canvas");
  const driver = document.getElementById("epddriver").value;
  const mode = document.getElementById('dithering').value;
  const imgArray = getImageData(canvas, driver, mode);
  const ramSize = canvas.width * canvas.height / 8;

  if (imgArray.length == ramSize * 2) {
    await epdWrite(0x10, imgArray.slice(0, ramSize));
    await epdWrite(0x13, imgArray.slice(ramSize));
  } else {
    await epdWrite(driver === "03" ? 0x10 : 0x13, imgArray);
  }

  if (mode === "4gray") {
    await epdWrite(0x00, [0x3F]); // Load LUT from register
    await send4GrayLut();
    await write(EpdCmd.DISPLAY);
    await epdWrite(0x00, [0x1F]); // Load LUT from OTP
  } else {
    await write(EpdCmd.DISPLAY);
  }

  const sendTime = (new Date().getTime() - startTime) / 1000.0;
  addLog(`发送完成！耗时: ${sendTime}s`);
  setStatus(`发送完成！耗时: ${sendTime}s`);
}

function updateButtonStatus() {
  const connected = gattServer != null && gattServer.connected;
  const status = connected ? null : 'disabled';
  document.getElementById("reconnectbutton").disabled = (gattServer == null || gattServer.connected) ? 'disabled' : null;
  document.getElementById("sendcmdbutton").disabled = status;
  document.getElementById("clearscreenbutton").disabled = status;
  document.getElementById("sendimgbutton").disabled = status;
  document.getElementById("setDriverbutton").disabled = status;
}

function disconnect() {
  updateButtonStatus();
  resetVariables();
  addLog('已断开连接.');
  document.getElementById("connectbutton").innerHTML = '连接';
}

async function preConnect() {
  if (gattServer != null && gattServer.connected) {
    if (bleDevice != null && bleDevice.gatt.connected) {
      await write(EpdCmd.SLEEP);
      bleDevice.gatt.disconnect();
    }
  }
  else {
    connectTrys = 0;
    try {
      bleDevice = await navigator.bluetooth.requestDevice({
        optionalServices: ['62750001-d828-918d-fb46-b6c11c675aec'],
        acceptAllDevices: true
      });
    } catch (e) {
      if (e.message) addLog(e.message);
      return;
    }

    await bleDevice.addEventListener('gattserverdisconnected', disconnect);
    try {
      await connect();
    } catch (e) {
      await handleError(e);
    }
  }
}

async function reConnect() {
  connectTrys = 0;
  if (bleDevice != null && bleDevice.gatt.connected)
    bleDevice.gatt.disconnect();
  resetVariables();
  addLog("正在重连");
  setTimeout(async function () { await connect(); }, 300);
}

async function connect() {
  if (epdCharacteristic == null && bleDevice != null) {
    addLog("正在连接: " + bleDevice.name);

    gattServer = await bleDevice.gatt.connect();
    addLog('  找到 GATT Server');

    epdService = await gattServer.getPrimaryService('62750001-d828-918d-fb46-b6c11c675aec');
    addLog('  找到 EPD Service');

    epdCharacteristic = await epdService.getCharacteristic('62750002-d828-918d-fb46-b6c11c675aec');
    addLog('  找到 Characteristic');

    await epdCharacteristic.startNotifications();
    epdCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
      addLog(`<span class="action">⇓</span> ${bytes2hex(event.target.value.buffer)}`);
      document.getElementById("epdpins").value = bytes2hex(event.target.value.buffer.slice(0, 7));
      document.getElementById("epddriver").value = bytes2hex(event.target.value.buffer.slice(7, 8));
    });

    await write(EpdCmd.INIT);

    document.getElementById("connectbutton").innerHTML = '断开';
    updateButtonStatus();
  }
}

function setStatus(statusText) {
  document.getElementById("status").innerHTML = statusText;
}

function addLog(logTXT) {
  const log = document.getElementById("log");
  const now = new Date();
  const time = String(now.getHours()).padStart(2, '0') + ":" +
         String(now.getMinutes()).padStart(2, '0') + ":" +
         String(now.getSeconds()).padStart(2, '0') + " ";
  log.innerHTML += '<span class="time">' + time + '</span>' + logTXT + '<br>';
  log.scrollTop = log.scrollHeight;
  while ((log.innerHTML.match(/<br>/g) || []).length > 20) {
    var logs_br_position = log.innerHTML.search("<br>");
    log.innerHTML = log.innerHTML.substring(logs_br_position + 4);
    log.scrollTop = log.scrollHeight;
  }
}

function clearLog() {
  document.getElementById("log").innerHTML = '';
}

function hex2bytes(hex) {
  for (var bytes = [], c = 0; c < hex.length; c += 2)
    bytes.push(parseInt(hex.substr(c, 2), 16));
  return new Uint8Array(bytes);
}

function bytes2hex(data) {
  return new Uint8Array(data).reduce(
    function (memo, i) {
      return memo + ("0" + i.toString(16)).slice(-2);
    }, "");
}

function intToHex(intIn) {
  let stringOut = ("0000" + intIn.toString(16)).substr(-4)
  return stringOut.substring(2, 4) + stringOut.substring(0, 2);
}

async function update_image () {
  const canvas = document.getElementById("canvas");
  const ctx = canvas.getContext("2d");

  let image = new Image();;
  const image_file = document.getElementById('image_file');
  if (image_file.files.length > 0) {
    const file = image_file.files[0];
    image.src = URL.createObjectURL(file);
  } else {
    image.src = document.getElementById('demo-img').src;
  }

  image.onload = function(event) {
    URL.revokeObjectURL(this.src);
    ctx.drawImage(image, 0, 0, image.width, image.height, 0, 0, canvas.width, canvas.height);
    convert_dithering()
  }
}

function clear_canvas() {
  if(confirm('确认清除画布内容?')) {
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
  }
}

function convert_dithering() {
  const ctx = canvas.getContext("2d");
  const mode = document.getElementById('dithering').value;
  if (mode.startsWith('bwr')) {
    ditheringCanvasByPalette(canvas, bwrPalette, mode);
  } else if (mode === '4gray') {
    dithering(ctx, canvas.width, canvas.height, 4, "gray");
  } else {
    dithering(ctx, canvas.width, canvas.height, parseInt(document.getElementById('threshold').value), mode);
  }
}

document.body.onload = () => {
  canvas = document.getElementById('canvas');

  updateButtonStatus();
  update_image();

  document.getElementById('dithering').value = 'none';
}