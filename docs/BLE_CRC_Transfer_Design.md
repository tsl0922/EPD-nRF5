# BLE图像传输CRC校验与断点续传设计方案

> **版本**: v1.0  
> **日期**: 2026-02-01  
> **状态**: 已实现

## 1. 概述

本方案为BLE图像传输实现数据完整性保护，解决以下问题：
- **无校验机制**：数据包直接写入EPD RAM，无法检测丢包或损坏
- **不可恢复**：一旦数据丢失，无法重传，导致显示错乱
- **断连后丢失**：重连后需要从头开始传输

---

## 2. 协议设计

### 2.1 新增命令

| 命令 | ID | 方向 | 说明 |
|------|-----|------|------|
| WRITE_BLOCK | 0x31 | APP→MCU | 带CRC的数据块写入 |
| QUERY_STATUS | 0x32 | APP→MCU | 查询传输状态 |
| RESET_TRANSFER | 0x33 | APP→MCU | 重置传输上下文 |

### 2.2 响应类型

| 响应 | ID | 方向 | 说明 |
|------|-----|------|------|
| BLOCK_ACK | 0xA0 | MCU→APP | 块ACK/NACK响应 |
| STATUS | 0xA1 | MCU→APP | 传输状态响应 |

---

## 3. 数据包格式

### 3.1 WRITE_BLOCK (0x31)

```
┌─────────┬──────────┬───────────┬─────────┬────────────┬──────────┐
│ CMD     │ Block ID │ Total Blk │ CFG     │ Payload    │ CRC16    │
│ (1byte) │ (2bytes) │ (2bytes)  │ (1byte) │ (N bytes)  │ (2bytes) │
└─────────┴──────────┴───────────┴─────────┴────────────┴──────────┘
```

**CFG字节格式**:
- bit[3:0]: 图层 (`0x0F`=黑白层, `0x00`=颜色层)
- bit[7:4]: 首块标志 (`0x00`=首块需发送RAM命令, `0xF0`=续块)

### 3.2 BLOCK_ACK响应 (0xA0)

```
┌─────────┬──────────┬─────────┐
│ RSP     │ Block ID │ Status  │
│ (1byte) │ (2bytes) │ (1byte) │
└─────────┴──────────┴─────────┘
```

**Status值**:
| 值 | 含义 |
|----|------|
| 0x00 | ACK - 成功 |
| 0x01 | NACK - CRC错误 |
| 0x02 | NACK - block_id无效 |
| 0x03 | NACK - EPD未初始化 |

### 3.3 STATUS响应 (0xA1)

```
┌─────────┬───────┬──────────┬─────────┬────────┬────────────┐
│ RSP     │ Total │ Received │ Session │ Active │ Bitmap     │
│ (1byte) │ (2B)  │ (2bytes) │ (1byte) │ (1byte)│ (N bytes)  │
└─────────┴───────┴──────────┴─────────┴────────┴────────────┘
```

Bitmap长度根据total_blocks动态计算，最大64字节。

---

## 4. CRC16算法

使用**CRC16-CCITT**算法：
- 多项式: 0x8408 (反转形式)
- 初始值: 0xFFFF
- 只校验payload部分

```c
// MCU端实现
static uint16_t crc16_compute(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1;
        }
    }
    return crc;
}
```

---

## 5. 传输流程

### 5.1 首次传输

```
APP                                MCU
 │                                  │
 ├──RESET_TRANSFER(session_id)────>│  重置状态
 │                                  │
 ├──WRITE_BLOCK(0, total, data)───>│  发送首块
 ├──WRITE_BLOCK(1, total, data)───>│
 │  ... (批量发送，不等ACK)         │
 ├──WRITE_BLOCK(N, total, data)───>│  最后一块(withResponse)
 │                                  │
 ├──QUERY_STATUS─────────────────->│
 │<─STATUS(bitmap)─────────────────│  获取缺失块信息
 │                                  │
 │  [若有缺失块，重传]              │
```

### 5.2 断点续传

```
APP                                MCU
 │                                  │
 ├──QUERY_STATUS─────────────────->│  查询已接收状态
 │<─STATUS(bitmap)─────────────────│
 │                                  │
 │  [解析bitmap，只发送缺失块]      │
 ├──WRITE_BLOCK(missing_id, ...)──>│
```

---

## 6. 资源占用

| 资源 | 占用 | 说明 |
|------|------|------|
| MCU RAM | +72字节 | `image_transfer_ctx_t`结构体 |
| MCU Flash | ~800字节 | CRC函数和命令处理 |
| 传输开销 | +4.2% | 每块增加8字节头尾 |

---

## 7. 配置参数

### MCU端 (EPD_service.h)

```c
#define EPD_MAX_BLOCKS        512   // 最大块数 (96KB / 192B)
#define EPD_BLOCK_BITMAP_SIZE 64    // 位图大小 (512 bits)
```

### APP端 (ble_transfer.js)

```javascript
MAX_RETRIES: 3,        // 最大重试轮次
BATCH_SIZE: 20,        // 每批块数
BATCH_DELAY_MS: 200,   // 批次间延时
```

---

## 8. 向后兼容

- 保留原有 `EPD_CMD_WRITE_IMAGE (0x30)` 命令
- APP根据固件版本自动选择传输模式：
  - 版本 ≥ 0x19: 使用CRC传输
  - 版本 < 0x19: 使用传统传输

---

## 9. 修改文件清单

### MCU端
- `EPD/EPD_service.h` - 新增命令、结构体定义
- `EPD/EPD_service.c` - 实现CRC计算、命令处理

### APP端
- `html/js/ble_transfer.js` - 新建CRC传输模块
- `html/js/main.js` - 集成新模块
- `html/index.html` - 添加脚本引用
