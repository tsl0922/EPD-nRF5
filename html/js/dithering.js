// Ported from: https://e-paper-display.cn/usb2epd.html

// 固定的六色调色板
const rgbPalette = [
  { name: "黑色", r: 0, g: 0, b: 0, value: 0x00 },
  { name: "白色", r: 255, g: 255, b: 255, value: 0x01 },
  { name: "黄色", r: 255, g: 255, b: 0, value: 0x02 },
  { name: "红色", r: 255, g: 0, b: 0, value: 0x03 },
  { name: "蓝色", r: 0, g: 0, b: 255, value: 0x05 },
  { name: "绿色", r: 41, g: 204, b: 20, value: 0x06 }
];

// 四色调色板
const fourColorPalette = [
  { name: "黑色", r: 0, g: 0, b: 0, value: 0x00 },
  { name: "白色", r: 255, g: 255, b: 255, value: 0x01 },
  { name: "红色", r: 255, g: 0, b: 0, value: 0x03 },
  { name: "黄色", r: 255, g: 255, b: 0, value: 0x02 }
];

// 三色调色板
const threeColorPalette = [
  { name: "黑色", r: 0, g: 0, b: 0, value: 0x00 },
  { name: "白色", r: 255, g: 255, b: 255, value: 0x01 },
  { name: "红色", r: 255, g: 0, b: 0, value: 0x02 }
];

function adjustContrast(imageData, factor) {
  const data = imageData.data;
  for (let i = 0; i < data.length; i += 4) {
    data[i] = Math.min(255, Math.max(0, (data[i] - 128) * factor + 128));
    data[i + 1] = Math.min(255, Math.max(0, (data[i + 1] - 128) * factor + 128));
    data[i + 2] = Math.min(255, Math.max(0, (data[i + 2] - 128) * factor + 128));
  }
  return imageData;
}

function rgbToLab(r, g, b) {
  r = r / 255;
  g = g / 255;
  b = b / 255;

  r = r > 0.04045 ? Math.pow((r + 0.055) / 1.055, 2.4) : r / 12.92;
  g = g > 0.04045 ? Math.pow((g + 0.055) / 1.055, 2.4) : g / 12.92;
  b = b > 0.04045 ? Math.pow((b + 0.055) / 1.055, 2.4) : b / 12.92;

  r *= 100;
  g *= 100;
  b *= 100;

  let x = r * 0.4124 + g * 0.3576 + b * 0.1805;
  let y = r * 0.2126 + g * 0.7152 + b * 0.0722;
  let z = r * 0.0193 + g * 0.1192 + b * 0.9505;

  x /= 95.047;
  y /= 100.0;
  z /= 108.883;

  x = x > 0.008856 ? Math.pow(x, 1 / 3) : (7.787 * x) + (16 / 116);
  y = y > 0.008856 ? Math.pow(y, 1 / 3) : (7.787 * y) + (16 / 116);
  z = z > 0.008856 ? Math.pow(z, 1 / 3) : (7.787 * z) + (16 / 116);

  const l = (116 * y) - 16;
  const a = 500 * (x - y);
  const bLab = 200 * (y - z);

  return { l, a, b: bLab };
}

function labDistance(lab1, lab2) {
  const dl = lab1.l - lab2.l;
  const da = lab1.a - lab2.a;
  const db = lab1.b - lab2.b;
  return Math.sqrt(0.2 * dl * dl + 3 * da * da + 3 * db * db);
}

function colorDistance(c1, c2) {
  return Math.sqrt(
    Math.pow(c1.r - c2.r, 2) +
    Math.pow(c1.g - c2.g, 2) +
    Math.pow(c1.b - c2.b, 2)
  );
}

function findNearestColor(pixel, colors) {
  let nearestColor = colors[0];
  let minDistance = colorDistance(pixel, colors[0]);
  for (let i = 1; i < colors.length; i++) {
    const distance = colorDistance(pixel, colors[i]);
    if (distance < minDistance) {
      minDistance = distance;
      nearestColor = colors[i];
    }
  }
  return nearestColor;
}

function findClosestColor(r, g, b, mode) {
  let palette;

  if (mode === 'fourColor') {
    palette = fourColorPalette;
  } else if (mode === 'threeColor') {
    palette = threeColorPalette;
  } else {
    palette = rgbPalette;
  }

  // 蓝色特殊情况（仅限非三色、四色模式）
  if (mode !== 'fourColor' && mode !== 'threeColor' && r < 50 && g < 150 && b > 100) {
    return rgbPalette[4]; // 蓝色
  }

  if (mode === 'BWR') return findNearestColor({ r, g, b }, palette);

  const inputLab = rgbToLab(r, g, b);
  let minDistance = Infinity;
  let closestColor = palette[0];

  for (const color of palette) {
    const colorLab = rgbToLab(color.r, color.g, color.b);
    const distance = labDistance(inputLab, colorLab);
    if (distance < minDistance) {
      minDistance = distance;
      closestColor = color;
    }
  }

  return closestColor;
}

function floydSteinbergDither(imageData, strength, mode) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;
  const tempData = new Uint8ClampedArray(data);

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * 4;
      const r = tempData[idx];
      const g = tempData[idx + 1];
      const b = tempData[idx + 2];

      const closest = findClosestColor(r, g, b, mode);

      const errR = (r - closest.r) * strength;
      const errG = (g - closest.g) * strength;
      const errB = (b - closest.b) * strength;

      if (x + 1 < width) {
        const idxRight = idx + 4;
        tempData[idxRight] = Math.min(255, Math.max(0, tempData[idxRight] + errR * 7 / 16));
        tempData[idxRight + 1] = Math.min(255, Math.max(0, tempData[idxRight + 1] + errG * 7 / 16));
        tempData[idxRight + 2] = Math.min(255, Math.max(0, tempData[idxRight + 2] + errB * 7 / 16));
      }
      if (y + 1 < height) {
        if (x > 0) {
          const idxDownLeft = idx + width * 4 - 4;
          tempData[idxDownLeft] = Math.min(255, Math.max(0, tempData[idxDownLeft] + errR * 3 / 16));
          tempData[idxDownLeft + 1] = Math.min(255, Math.max(0, tempData[idxDownLeft + 1] + errG * 3 / 16));
          tempData[idxDownLeft + 2] = Math.min(255, Math.max(0, tempData[idxDownLeft + 2] + errB * 3 / 16));
        }
        const idxDown = idx + width * 4;
        tempData[idxDown] = Math.min(255, Math.max(0, tempData[idxDown] + errR * 5 / 16));
        tempData[idxDown + 1] = Math.min(255, Math.max(0, tempData[idxDown + 1] + errG * 5 / 16));
        tempData[idxDown + 2] = Math.min(255, Math.max(0, tempData[idxDown + 2] + errB * 5 / 16));
        if (x + 1 < width) {
          const idxDownRight = idx + width * 4 + 4;
          tempData[idxDownRight] = Math.min(255, Math.max(0, tempData[idxDownRight] + errR * 1 / 16));
          tempData[idxDownRight + 1] = Math.min(255, Math.max(0, tempData[idxDownRight + 1] + errG * 1 / 16));
          tempData[idxDownRight + 2] = Math.min(255, Math.max(0, tempData[idxDownRight + 2] + errB * 1 / 16));
        }
      }
    }
  }

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * 4;
      const r = tempData[idx];
      const g = tempData[idx + 1];
      const b = tempData[idx + 2];

      const closest = findClosestColor(r, g, b, mode);
      data[idx] = closest.r;
      data[idx + 1] = closest.g;
      data[idx + 2] = closest.b;
    }
  }

  return imageData;
}

function atkinsonDither(imageData, strength, mode) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;
  const tempData = new Uint8ClampedArray(data);

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * 4;
      const r = tempData[idx];
      const g = tempData[idx + 1];
      const b = tempData[idx + 2];

      const closest = findClosestColor(r, g, b, mode);

      data[idx] = closest.r;
      data[idx + 1] = closest.g;
      data[idx + 2] = closest.b;

      const errR = (r - closest.r) * strength;
      const errG = (g - closest.g) * strength;
      const errB = (b - closest.b) * strength;

      const fraction = 1 / 8;

      if (x + 1 < width) {
        const idxRight = idx + 4;
        tempData[idxRight] = Math.min(255, Math.max(0, tempData[idxRight] + errR * fraction));
        tempData[idxRight + 1] = Math.min(255, Math.max(0, tempData[idxRight + 1] + errG * fraction));
        tempData[idxRight + 2] = Math.min(255, Math.max(0, tempData[idxRight + 2] + errB * fraction));
      }
      if (x + 2 < width) {
        const idxRight2 = idx + 8;
        tempData[idxRight2] = Math.min(255, Math.max(0, tempData[idxRight2] + errR * fraction));
        tempData[idxRight2 + 1] = Math.min(255, Math.max(0, tempData[idxRight2 + 1] + errG * fraction));
        tempData[idxRight2 + 2] = Math.min(255, Math.max(0, tempData[idxRight2 + 2] + errB * fraction));
      }
      if (y + 1 < height) {
        if (x > 0) {
          const idxDownLeft = idx + width * 4 - 4;
          tempData[idxDownLeft] = Math.min(255, Math.max(0, tempData[idxDownLeft] + errR * fraction));
          tempData[idxDownLeft + 1] = Math.min(255, Math.max(0, tempData[idxDownLeft + 1] + errG * fraction));
          tempData[idxDownLeft + 2] = Math.min(255, Math.max(0, tempData[idxDownLeft + 2] + errB * fraction));
        }
        const idxDown = idx + width * 4;
        tempData[idxDown] = Math.min(255, Math.max(0, tempData[idxDown] + errR * fraction));
        tempData[idxDown + 1] = Math.min(255, Math.max(0, tempData[idxDown + 1] + errG * fraction));
        tempData[idxDown + 2] = Math.min(255, Math.max(0, tempData[idxDown + 2] + errB * fraction));
        if (x + 1 < width) {
          const idxDownRight = idx + width * 4 + 4;
          tempData[idxDownRight] = Math.min(255, Math.max(0, tempData[idxDownRight] + errR * fraction));
          tempData[idxDownRight + 1] = Math.min(255, Math.max(0, tempData[idxDownRight + 1] + errG * fraction));
          tempData[idxDownRight + 2] = Math.min(255, Math.max(0, tempData[idxDownRight + 2] + errB * fraction));
        }
      }
      if (y + 2 < height) {
        const idxDown2 = idx + width * 8;
        tempData[idxDown2] = Math.min(255, Math.max(0, tempData[idxDown2] + errR * fraction));
        tempData[idxDown2 + 1] = Math.min(255, Math.max(0, tempData[idxDown2 + 1] + errG * fraction));
        tempData[idxDown2 + 2] = Math.min(255, Math.max(0, tempData[idxDown2 + 2] + errB * fraction));
      }
    }
  }

  return imageData;
}

function stuckiDither(imageData, strength, mode) {
  // 执行Stucki错误扩散算法以处理图像
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;
  const tempData = new Uint8ClampedArray(data);

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * 4;
      const r = tempData[idx];
      const g = tempData[idx + 1];
      const b = tempData[idx + 2];

      const closest = findClosestColor(r, g, b, mode);

      const errR = (r - closest.r) * strength;
      const errG = (g - closest.g) * strength;
      const errB = (b - closest.b) * strength;

      const divisor = 42;

      if (x + 1 < width) {
        const idxRight = idx + 4;
        tempData[idxRight] = Math.min(255, Math.max(0, tempData[idxRight] + errR * 8 / divisor));
        tempData[idxRight + 1] = Math.min(255, Math.max(0, tempData[idxRight + 1] + errG * 8 / divisor));
        tempData[idxRight + 2] = Math.min(255, Math.max(0, tempData[idxRight + 2] + errB * 8 / divisor));
      }
      if (x + 2 < width) {
        const idxRight2 = idx + 8;
        tempData[idxRight2] = Math.min(255, Math.max(0, tempData[idxRight2] + errR * 4 / divisor));
        tempData[idxRight2 + 1] = Math.min(255, Math.max(0, tempData[idxRight2 + 1] + errG * 4 / divisor));
        tempData[idxRight2 + 2] = Math.min(255, Math.max(0, tempData[idxRight2 + 2] + errB * 4 / divisor));
      }
      if (y + 1 < height) {
        if (x > 1) {
          const idxDownLeft2 = idx + width * 4 - 8;
          tempData[idxDownLeft2] = Math.min(255, Math.max(0, tempData[idxDownLeft2] + errR * 2 / divisor));
          tempData[idxDownLeft2 + 1] = Math.min(255, Math.max(0, tempData[idxDownLeft2 + 1] + errG * 2 / divisor));
          tempData[idxDownLeft2 + 2] = Math.min(255, Math.max(0, tempData[idxDownLeft2 + 2] + errB * 2 / divisor));
        }
        if (x > 0) {
          const idxDownLeft = idx + width * 4 - 4;
          tempData[idxDownLeft] = Math.min(255, Math.max(0, tempData[idxDownLeft] + errR * 4 / divisor));
          tempData[idxDownLeft + 1] = Math.min(255, Math.max(0, tempData[idxDownLeft + 1] + errG * 4 / divisor));
          tempData[idxDownLeft + 2] = Math.min(255, Math.max(0, tempData[idxDownLeft + 2] + errB * 4 / divisor));
        }
        const idxDown = idx + width * 4;
        tempData[idxDown] = Math.min(255, Math.max(0, tempData[idxDown] + errR * 8 / divisor));
        tempData[idxDown + 1] = Math.min(255, Math.max(0, tempData[idxDown + 1] + errG * 8 / divisor));
        tempData[idxDown + 2] = Math.min(255, Math.max(0, tempData[idxDown + 2] + errB * 8 / divisor));
        if (x + 1 < width) {
          const idxDownRight1 = idx + width * 4 + 4;
          tempData[idxDownRight1] = Math.min(255, Math.max(0, tempData[idxDownRight1] + errR * 4 / divisor));
          tempData[idxDownRight1 + 1] = Math.min(255, Math.max(0, tempData[idxDownRight1 + 1] + errG * 4 / divisor));
          tempData[idxDownRight1 + 2] = Math.min(255, Math.max(0, tempData[idxDownRight1 + 2] + errB * 4 / divisor));
        }
        if (x + 2 < width) {
          const idxDownRight2 = idx + width * 4 + 8;
          tempData[idxDownRight2] = Math.min(255, Math.max(0, tempData[idxDownRight2] + errR * 2 / divisor));
          tempData[idxDownRight2 + 1] = Math.min(255, Math.max(0, tempData[idxDownRight2 + 1] + errG * 2 / divisor));
          tempData[idxDownRight2 + 2] = Math.min(255, Math.max(0, tempData[idxDownRight2 + 2] + errB * 2 / divisor));
        }
      }
      if (y + 2 < height) {
        if (x > 1) {
          const idxDown2Left2 = idx + width * 8 - 8;
          tempData[idxDown2Left2] = Math.min(255, Math.max(0, tempData[idxDown2Left2] + errR * 1 / divisor));
          tempData[idxDown2Left2 + 1] = Math.min(255, Math.max(0, tempData[idxDown2Left2 + 1] + errG * 1 / divisor));
          tempData[idxDown2Left2 + 2] = Math.min(255, Math.max(0, tempData[idxDown2Left2 + 2] + errB * 1 / divisor));
        }
        if (x > 0) {
          const idxDown2Left = idx + width * 8 - 4;
          tempData[idxDown2Left] = Math.min(255, Math.max(0, tempData[idxDown2Left] + errR * 2 / divisor));
          tempData[idxDown2Left + 1] = Math.min(255, Math.max(0, tempData[idxDown2Left + 1] + errG * 2 / divisor));
          tempData[idxDown2Left + 2] = Math.min(255, Math.max(0, tempData[idxDown2Left + 2] + errB * 2 / divisor));
        }
        const idxDown2 = idx + width * 8;
        tempData[idxDown2] = Math.min(255, Math.max(0, tempData[idxDown2] + errR * 4 / divisor));
        tempData[idxDown2 + 1] = Math.min(255, Math.max(0, tempData[idxDown2 + 1] + errG * 4 / divisor));
        tempData[idxDown2 + 2] = Math.min(255, Math.max(0, tempData[idxDown2 + 2] + errB * 4 / divisor));
        if (x + 1 < width) {
          const idxDown2Right = idx + width * 8 + 4;
          tempData[idxDown2Right] = Math.min(255, Math.max(0, tempData[idxDown2Right] + errR * 2 / divisor));
          tempData[idxDown2Right + 1] = Math.min(255, Math.max(0, tempData[idxDown2Right + 1] + errG * 2 / divisor));
          tempData[idxDown2Right + 2] = Math.min(255, Math.max(0, tempData[idxDown2Right + 2] + errB * 2 / divisor));
        }
        if (x + 2 < width) {
          const idxDown2Right2 = idx + width * 8 + 8;
          tempData[idxDown2Right2] = Math.min(255, Math.max(0, tempData[idxDown2Right2] + errR * 1 / divisor));
          tempData[idxDown2Right2 + 1] = Math.min(255, Math.max(0, tempData[idxDown2Right2 + 1] + errG * 1 / divisor));
          tempData[idxDown2Right2 + 2] = Math.min(255, Math.max(0, tempData[idxDown2Right2 + 2] + errB * 1 / divisor));
        }
      }
    }
  }

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * 4;
      const r = tempData[idx];
      const g = tempData[idx + 1];
      const b = tempData[idx + 2];

      const closest = findClosestColor(r, g, b, mode);
      data[idx] = closest.r;
      data[idx + 1] = closest.g;
      data[idx + 2] = closest.b;
    }
  }

  return imageData;
}

function jarvisDither(imageData, strength, mode) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;
  const tempData = new Uint8ClampedArray(data);

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * 4;
      const r = tempData[idx];
      const g = tempData[idx + 1];
      const b = tempData[idx + 2];

      const closest = findClosestColor(r, g, b, mode);

      data[idx] = closest.r;
      data[idx + 1] = closest.g;
      data[idx + 2] = closest.b;

      const errR = (r - closest.r) * strength;
      const errG = (g - closest.g) * strength;
      const errB = (b - closest.b) * strength;

      const divisor = 48;

      if (x + 1 < width) {
        const idxRight = idx + 4;
        tempData[idxRight] = Math.min(255, Math.max(0, tempData[idxRight] + errR * 7 / divisor));
        tempData[idxRight + 1] = Math.min(255, Math.max(0, tempData[idxRight + 1] + errG * 7 / divisor));
        tempData[idxRight + 2] = Math.min(255, Math.max(0, tempData[idxRight + 2] + errB * 7 / divisor));
      }
      if (x + 2 < width) {
        const idxRight2 = idx + 8;
        tempData[idxRight2] = Math.min(255, Math.max(0, tempData[idxRight2] + errR * 5 / divisor));
        tempData[idxRight2 + 1] = Math.min(255, Math.max(0, tempData[idxRight2 + 1] + errG * 5 / divisor));
        tempData[idxRight2 + 2] = Math.min(255, Math.max(0, tempData[idxRight2 + 2] + errB * 5 / divisor));
      }
      if (y + 1 < height) {
        if (x > 1) {
          const idxDownLeft2 = idx + width * 4 - 8;
          tempData[idxDownLeft2] = Math.min(255, Math.max(0, tempData[idxDownLeft2] + errR * 3 / divisor));
          tempData[idxDownLeft2 + 1] = Math.min(255, Math.max(0, tempData[idxDownLeft2 + 1] + errG * 3 / divisor));
          tempData[idxDownLeft2 + 2] = Math.min(255, Math.max(0, tempData[idxDownLeft2 + 2] + errB * 3 / divisor));
        }
        if (x > 0) {
          const idxDownLeft = idx + width * 4 - 4;
          tempData[idxDownLeft] = Math.min(255, Math.max(0, tempData[idxDownLeft] + errR * 5 / divisor));
          tempData[idxDownLeft + 1] = Math.min(255, Math.max(0, tempData[idxDownLeft + 1] + errG * 5 / divisor));
          tempData[idxDownLeft + 2] = Math.min(255, Math.max(0, tempData[idxDownLeft + 2] + errB * 5 / divisor));
        }
        const idxDown = idx + width * 4;
        tempData[idxDown] = Math.min(255, Math.max(0, tempData[idxDown] + errR * 7 / divisor));
        tempData[idxDown + 1] = Math.min(255, Math.max(0, tempData[idxDown + 1] + errG * 7 / divisor));
        tempData[idxDown + 2] = Math.min(255, Math.max(0, tempData[idxDown + 2] + errB * 7 / divisor));
        if (x + 1 < width) {
          const idxDownRight = idx + width * 4 + 4;
          tempData[idxDownRight] = Math.min(255, Math.max(0, tempData[idxDownRight] + errR * 5 / divisor));
          tempData[idxDownRight + 1] = Math.min(255, Math.max(0, tempData[idxDownRight + 1] + errG * 5 / divisor));
          tempData[idxDownRight + 2] = Math.min(255, Math.max(0, tempData[idxDownRight + 2] + errB * 5 / divisor));
        }
        if (x + 2 < width) {
          const idxDownRight2 = idx + width * 4 + 8;
          tempData[idxDownRight2] = Math.min(255, Math.max(0, tempData[idxDownRight2] + errR * 3 / divisor));
          tempData[idxDownRight2 + 1] = Math.min(255, Math.max(0, tempData[idxDownRight2 + 1] + errG * 3 / divisor));
          tempData[idxDownRight2 + 2] = Math.min(255, Math.max(0, tempData[idxDownRight2 + 2] + errB * 3 / divisor));
        }
      }
      if (y + 2 < height) {
        if (x > 1) {
          const idxDown2Left2 = idx + width * 8 - 8;
          tempData[idxDown2Left2] = Math.min(255, Math.max(0, tempData[idxDown2Left2] + errR * 1 / divisor));
          tempData[idxDown2Left2 + 1] = Math.min(255, Math.max(0, tempData[idxDown2Left2 + 1] + errG * 1 / divisor));
          tempData[idxDown2Left2 + 2] = Math.min(255, Math.max(0, tempData[idxDown2Left2 + 2] + errB * 1 / divisor));
        }
        if (x > 0) {
          const idxDown2Left = idx + width * 8 - 4;
          tempData[idxDown2Left] = Math.min(255, Math.max(0, tempData[idxDown2Left] + errR * 3 / divisor));
          tempData[idxDown2Left + 1] = Math.min(255, Math.max(0, tempData[idxDown2Left + 1] + errG * 3 / divisor));
          tempData[idxDown2Left + 2] = Math.min(255, Math.max(0, tempData[idxDown2Left + 2] + errB * 3 / divisor));
        }
        const idxDown2 = idx + width * 8;
        tempData[idxDown2] = Math.min(255, Math.max(0, tempData[idxDown2] + errR * 5 / divisor));
        tempData[idxDown2 + 1] = Math.min(255, Math.max(0, tempData[idxDown2 + 1] + errG * 5 / divisor));
        tempData[idxDown2 + 2] = Math.min(255, Math.max(0, tempData[idxDown2 + 2] + errB * 5 / divisor));
        if (x + 1 < width) {
          const idxDown2Right = idx + width * 8 + 4;
          tempData[idxDown2Right] = Math.min(255, Math.max(0, tempData[idxDown2Right] + errR * 3 / divisor));
          tempData[idxDown2Right + 1] = Math.min(255, Math.max(0, tempData[idxDown2Right + 1] + errG * 3 / divisor));
          tempData[idxDown2Right + 2] = Math.min(255, Math.max(0, tempData[idxDown2Right + 2] + errB * 3 / divisor));
        }
        if (x + 2 < width) {
          const idxDown2Right2 = idx + width * 8 + 8;
          tempData[idxDown2Right2] = Math.min(255, Math.max(0, tempData[idxDown2Right2] + errR * 1 / divisor));
          tempData[idxDown2Right2 + 1] = Math.min(255, Math.max(0, tempData[idxDown2Right2 + 1] + errG * 1 / divisor));
          tempData[idxDown2Right2 + 2] = Math.min(255, Math.max(0, tempData[idxDown2Right2 + 2] + errB * 1 / divisor));
        }
      }
    }
  }

  return imageData;
}

function bayerDither(imageData, strength, mode) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;

  // 8x8 Bayer matrix (normalized to 0-1 range)
  const bayerMatrix = [
    [0, 32, 8, 40, 2, 34, 10, 42],
    [48, 16, 56, 24, 50, 18, 58, 26],
    [12, 44, 4, 36, 14, 46, 6, 38],
    [60, 28, 52, 20, 62, 30, 54, 22],
    [3, 35, 11, 43, 1, 33, 9, 41],
    [51, 19, 59, 27, 49, 17, 57, 25],
    [15, 47, 7, 39, 13, 45, 5, 37],
    [63, 31, 55, 23, 61, 29, 53, 21]
  ];

  const matrixSize = 8;
  const maxThreshold = 64;

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * 4;
      const r = data[idx];
      const g = data[idx + 1];
      const b = data[idx + 2];

      // Get threshold from Bayer matrix
      const matrixX = x % matrixSize;
      const matrixY = y % matrixSize;
      const threshold = (bayerMatrix[matrixY][matrixX] / maxThreshold) * 255;

      // Apply dithering with strength factor
      const adjustedR = r + (threshold - 127.5) * strength;
      const adjustedG = g + (threshold - 127.5) * strength;
      const adjustedB = b + (threshold - 127.5) * strength;

      // Clamp values
      const clampedR = Math.min(255, Math.max(0, adjustedR));
      const clampedG = Math.min(255, Math.max(0, adjustedG));
      const clampedB = Math.min(255, Math.max(0, adjustedB));

      // Find closest color in palette
      const closest = findClosestColor(clampedR, clampedG, clampedB, mode);

      data[idx] = closest.r;
      data[idx + 1] = closest.g;
      data[idx + 2] = closest.b;
    }
  }

  return imageData;
}

function ditherImage(imageData, alg, strength, mode) {
  switch (alg) {
    case 'floydSteinberg':
      return floydSteinbergDither(imageData, strength, mode);
    case 'atkinson':
      return atkinsonDither(imageData, strength, mode);
    case 'stucki':
      return stuckiDither(imageData, strength, mode);
    case 'jarvis':
      return jarvisDither(imageData, strength, mode);
    case 'bayer':
      return bayerDither(imageData, strength, mode);
    case 'none':
    default:
      return imageData;
  }
}

function decodeProcessedData(processedData, width, height, mode) {
  const imageData = new ImageData(width, height);
  const data = imageData.data;

  if (mode === 'sixColor') {
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const newIndex = (y * width + x) >> 1;
        const colorValue = (x % 2 === 0)
          ? (processedData[newIndex] >> 4) & 0x0F
          : processedData[newIndex] & 0x0F;
        const color = rgbPalette.find(c => c.value === colorValue) || rgbPalette[1]; // 默认白色
        const index = (y * width + x) * 4;
        data[index] = color.r;
        data[index + 1] = color.g;
        data[index + 2] = color.b;
        data[index + 3] = 255;
      }
    }
  } else if (mode === 'fourColor') {
    const fourColorValues = [
      { value: 0x00, r: 0, g: 0, b: 0 },      // 黑色
      { value: 0x01, r: 255, g: 255, b: 255 }, // 白色
      { value: 0x03, r: 255, g: 0, b: 0 },     // 红色
      { value: 0x02, r: 255, g: 255, b: 0 }    // 黄色
    ];
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const newIndex = (y * width + x) / 4 | 0;
        const shift = 6 - ((x % 4) * 2);
        const value = (processedData[newIndex] >> shift) & 0x03;
        const color = fourColorValues.find(c => c.value === value) || fourColorValues[1]; // 默认白色
        const index = (y * width + x) * 4;
        data[index] = color.r;
        data[index + 1] = color.g;
        data[index + 2] = color.b;
        data[index + 3] = 255;
      }
    }
  } else if (mode === 'blackWhiteColor') {
    const byteWidth = Math.ceil(width / 8);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const byteIndex = y * byteWidth + Math.floor(x / 8);
        const bitIndex = 7 - (x % 8);
        const bit = (processedData[byteIndex] >> bitIndex) & 1;
        const index = (y * width + x) * 4;
        data[index] = bit ? 255 : 0; // 白或黑
        data[index + 1] = bit ? 255 : 0;
        data[index + 2] = bit ? 255 : 0;
        data[index + 3] = 255;
      }
    }
  } else if (mode === 'threeColor') {
    const byteWidth = Math.ceil(width / 8);
    const blackWhiteData = processedData.slice(0, byteWidth * height);
    const redWhiteData = processedData.slice(byteWidth * height);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const byteIndex = y * byteWidth + Math.floor(x / 8);
        const bitIndex = 7 - (x % 8);
        const blackWhiteBit = (blackWhiteData[byteIndex] >> bitIndex) & 1;
        const redWhiteBit = (redWhiteData[byteIndex] >> bitIndex) & 1;
        const index = (y * width + x) * 4;
        if (!redWhiteBit) {
          // 红色
          data[index] = 255;
          data[index + 1] = 0;
          data[index + 2] = 0;
        } else {
          // 黑或白
          data[index] = blackWhiteBit ? 255 : 0;
          data[index + 1] = blackWhiteBit ? 255 : 0;
          data[index + 2] = blackWhiteBit ? 255 : 0;
        }
        data[index + 3] = 255;
      }
    }
  }

  return imageData;
}

function processImageData(imageData, mode) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;

  let processedData;

  if (mode === 'sixColor') {
    processedData = new Uint8Array(Math.ceil((width * height) / 2)); // 4bpp
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const index = (y * width + x) * 4;
        const r = data[index];
        const g = data[index + 1];
        const b = data[index + 2];
        const closest = findClosestColor(r, g, b, mode);
        const colorValue = closest.value; // 0-5
        const newIndex = (y * width + x) >> 1;
        if (x % 2 === 0) {
          processedData[newIndex] |= (colorValue << 4);
        } else {
          processedData[newIndex] |= colorValue;
        }
      }
    }
  } else if (mode === 'fourColor') {
    processedData = new Uint8Array(Math.ceil((width * height) / 4));
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const index = (y * width + x) * 4;
        const r = data[index];
        const g = data[index + 1];
        const b = data[index + 2];
        const closest = findClosestColor(r, g, b, mode); // 使用 fourColorPalette
        const colorValue = closest.value; // 0x00 (黑), 0x01 (白), 0x02 (红), 0x03 (黄)
        const newIndex = (y * width + x) / 4 | 0;
        const shift = 6 - ((x % 4) * 2);
        processedData[newIndex] |= (colorValue << shift);
      }
    }
  } else if (mode === 'blackWhiteColor') {
    const byteWidth = Math.ceil(width / 8);
    processedData = new Uint8Array(byteWidth * height);
    const threshold = 140;

    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const index = (y * width + x) * 4;
        const r = data[index];
        const g = data[index + 1];
        const b = data[index + 2];
        const grayscale = Math.round(0.299 * r + 0.587 * g + 0.114 * b);
        const bit = grayscale >= threshold ? 1 : 0;
        const byteIndex = y * byteWidth + Math.floor(x / 8);
        const bitIndex = 7 - (x % 8);
        processedData[byteIndex] |= (bit << bitIndex);
      }
    }
  } else if (mode === 'threeColor') {
    const byteWidth = Math.ceil(width / 8);
    const blackWhiteThreshold = 140;
    const redThreshold = 160;

    const blackWhiteData = new Uint8Array(height * byteWidth);
    const redWhiteData = new Uint8Array(height * byteWidth);

    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const index = (y * width + x) * 4;
        const r = data[index];
        const g = data[index + 1];
        const b = data[index + 2];
        const grayscale = Math.round(0.299 * r + 0.587 * g + 0.114 * b);

        const blackWhiteBit = grayscale >= blackWhiteThreshold ? 1 : 0;
        const blackWhiteByteIndex = y * byteWidth + Math.floor(x / 8);
        const blackWhiteBitIndex = 7 - (x % 8);
        if (blackWhiteBit) {
          blackWhiteData[blackWhiteByteIndex] |= (0x01 << blackWhiteBitIndex);
        } else {
          blackWhiteData[blackWhiteByteIndex] &= ~(0x01 << blackWhiteBitIndex);
        }

        const redWhiteBit = (r > redThreshold && r > g && r > b) ? 0 : 1;
        const redWhiteByteIndex = y * byteWidth + Math.floor(x / 8);
        const redWhiteBitIndex = 7 - (x % 8);
        if (redWhiteBit) {
          redWhiteData[redWhiteByteIndex] |= (0x01 << redWhiteBitIndex);
        } else {
          redWhiteData[redWhiteByteIndex] &= ~(0x01 << redWhiteBitIndex);
        }
      }
    }

    processedData = new Uint8Array(blackWhiteData.length + redWhiteData.length);
    processedData.set(blackWhiteData, 0);
    processedData.set(redWhiteData, blackWhiteData.length);
  }

  return processedData;
}
