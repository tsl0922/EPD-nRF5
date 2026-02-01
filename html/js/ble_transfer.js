/**
 * BLE Transfer Module with CRC Verification and Resume Capability
 * 
 * Features:
 * - CRC16-CCITT verification for data integrity
 * - Batch confirmation mode (no per-block ACK wait)
 * - Resume capability via bitmap tracking
 * - Multi-layer support (black/white and color layers)
 */

const BleTransfer = {
  // Configuration
  MAX_RETRIES: 3,           // Maximum retry rounds
  BATCH_SIZE: 20,           // Blocks per batch before status check
  BATCH_DELAY_MS: 200,      // Delay after batch for MCU processing

  // Commands
  CMD_WRITE_BLOCK: 0x31,
  CMD_QUERY_STATUS: 0x32,
  CMD_RESET_TRANSFER: 0x33,

  // Response types
  RSP_BLOCK_ACK: 0xA0,
  RSP_STATUS: 0xA1,

  // Current state
  currentLayer: 0x0F,       // Current layer: 0x0F=black/white, 0x00=color
  pendingStatus: null,      // Pending status response
  statusResolver: null,     // Promise resolver for status
  statusRequestId: 0,       // Request ID to prevent race conditions
  firstBlockSent: false,    // Track if first block of current layer was sent

  /**
   * CRC16-CCITT calculation (polynomial 0x8408, init 0xFFFF)
   * @param {Uint8Array} data - Data to calculate CRC for
   * @returns {number} 16-bit CRC value
   */
  crc16(data) {
    let crc = 0xFFFF;
    for (let i = 0; i < data.length; i++) {
      crc ^= data[i];
      for (let j = 0; j < 8; j++) {
        crc = (crc & 1) ? (crc >>> 1) ^ 0x8408 : crc >>> 1;
      }
    }
    return crc & 0xFFFF;
  },

  /**
   * Handle notification from MCU
   * @param {DataView} value - Received notification data
   */
  handleNotification(value) {
    const data = new Uint8Array(value.buffer, value.byteOffset, value.byteLength);

    if (data.length >= 1) {
      if (data[0] === this.RSP_STATUS && this.statusResolver) {
        // Minimum STATUS response is 7 bytes (header + metadata, no bitmap)
        if (data.length < 7) {
          console.warn('STATUS response too short:', data.length);
          return;
        }
        // Parse status response
        // Format: [0xA1, total_L, total_H, received_L, received_H, session, active, bitmap...]
        const status = {
          total: data[1] | (data[2] << 8),
          received: data[3] | (data[4] << 8),
          sessionId: data[5],
          active: data[6] === 1,
          bitmap: data.slice(7)
        };
        const resolver = this.statusResolver;
        this.statusResolver = null;  // Clear before calling to prevent race
        resolver(status);
      }
      // ACK/NACK responses are not waited for in batch mode
    }
  },

  /**
   * Send a single block (fast mode, no ACK wait)
   * @param {number} blockId - Block ID (0-indexed)
   * @param {number} totalBlocks - Total number of blocks
   * @param {Uint8Array} payload - Block payload data
   * @param {boolean} withResponse - Whether to wait for BLE response
   */
  async sendBlockFast(blockId, totalBlocks, payload, withResponse = false) {
    const crc = this.crc16(payload);
    // cfg = (first block flag) | (layer)
    // First block of layer needs RAM command (0x00), continuation uses 0xF0
    const isFirstBlock = !this.firstBlockSent;
    const cfg = (isFirstBlock ? 0x00 : 0xF0) | (this.currentLayer & 0x0F);

    if (isFirstBlock) {
      this.firstBlockSent = true;
    }

    // Packet format: [cmd][block_id:2][total:2][cfg:1][payload][crc:2]
    const packet = new Uint8Array(8 + payload.length);
    packet[0] = this.CMD_WRITE_BLOCK;
    packet[1] = blockId & 0xFF;
    packet[2] = blockId >> 8;
    packet[3] = totalBlocks & 0xFF;
    packet[4] = totalBlocks >> 8;
    packet[5] = cfg;
    packet.set(payload, 6);
    packet[6 + payload.length] = crc & 0xFF;
    packet[7 + payload.length] = crc >> 8;

    try {
      if (withResponse) {
        await epdCharacteristic.writeValueWithResponse(packet);
      } else {
        await epdCharacteristic.writeValueWithoutResponse(packet);
      }
    } catch (e) {
      console.error('Block send failed:', e);
      throw e;
    }
  },

  /**
   * Reset transfer state on MCU
   * @param {number} sessionId - Optional session ID
   */
  async resetTransfer(sessionId = 0) {
    this.firstBlockSent = false;  // Reset first block tracking
    const packet = new Uint8Array([this.CMD_RESET_TRANSFER, sessionId]);
    await epdCharacteristic.writeValueWithResponse(packet);
  },

  /**
   * Query transfer status from MCU
   * @returns {Promise<object>} Status object with total, received, bitmap
   */
  async queryStatus() {
    const requestId = ++this.statusRequestId;
    let timeoutId = null;

    return new Promise((resolve, reject) => {
      this.statusResolver = (status) => {
        if (timeoutId) clearTimeout(timeoutId);
        resolve(status);
      };

      const packet = new Uint8Array([this.CMD_QUERY_STATUS]);
      epdCharacteristic.writeValueWithResponse(packet).catch((e) => {
        if (timeoutId) clearTimeout(timeoutId);
        if (this.statusResolver) {
          this.statusResolver = null;
          reject(e);
        }
      });

      // Timeout after 3 seconds
      timeoutId = setTimeout(() => {
        // Only reject if this is still the current request
        if (this.statusResolver && this.statusRequestId === requestId) {
          this.statusResolver = null;
          reject(new Error('Status query timeout'));
        }
      }, 3000);
    });
  },

  /**
   * Get list of missing blocks from status bitmap
   * @param {object} status - Status object from queryStatus
   * @param {number} totalBlocks - Expected total blocks
   * @returns {number[]} Array of missing block IDs
   */
  getMissingBlocks(status, totalBlocks) {
    const missing = [];
    for (let i = 0; i < totalBlocks; i++) {
      const byteIdx = Math.floor(i / 8);
      const bitIdx = i % 8;
      if (byteIdx < status.bitmap.length) {
        if (!(status.bitmap[byteIdx] & (1 << bitIdx))) {
          missing.push(i);
        }
      } else {
        missing.push(i);
      }
    }
    return missing;
  },

  /**
   * Send image with CRC verification and resume capability
   * @param {Uint8Array} data - Image data to send
   * @param {string} step - 'bw' for black/white, 'red' for color layer
   * @param {function} onProgress - Progress callback (blocksSent, totalBlocks)
   * @returns {Promise<boolean>} True if successful
   */
  async sendImageWithResume(data, step = 'bw', onProgress = null) {
    let mtu = parseInt(document.getElementById('mtusize').value);
    if (isNaN(mtu) || mtu < 20) {
      console.warn('Invalid MTU value, using default 20');
      mtu = 20;
    }
    const chunkSize = Math.max(mtu - 8, 20); // Account for header/CRC overhead
    const totalBlocks = Math.ceil(data.length / chunkSize);

    // Statistics tracking
    const stats = {
      totalBlocks: totalBlocks,
      sentBlocks: 0,
      retries: 0,
      startTime: Date.now()
    };

    // Set current layer based on step
    this.currentLayer = (step === 'bw') ? 0x0F : 0x00;

    // Reset transfer state (also resets firstBlockSent)
    await this.resetTransfer(Date.now() & 0xFF);

    for (let retryRound = 0; retryRound < this.MAX_RETRIES; retryRound++) {
      let missingBlocks;

      // Optimization: Skip status query on first round (bitmap is empty after reset)
      if (retryRound === 0) {
        // First round: send all blocks
        missingBlocks = Array.from({ length: totalBlocks }, (_, i) => i);
      } else {
        // Retry rounds: query status to find missing blocks
        let status;
        try {
          status = await this.queryStatus();
        } catch (e) {
          console.warn('Status query failed, assuming all missing:', e);
          status = { total: 0, received: 0, bitmap: new Uint8Array(0) };
        }

        missingBlocks = this.getMissingBlocks(status, totalBlocks);

        if (missingBlocks.length === 0) {
          // Transfer complete
          const elapsed = ((Date.now() - stats.startTime) / 1000).toFixed(1);
          console.log(`Transfer complete: ${stats.totalBlocks} blocks, ${stats.sentBlocks} sent, ${stats.retries} retries, ${elapsed}s`);
          return true;
        }

        stats.retries++;
      }

      console.log(`Round ${retryRound + 1}: ${missingBlocks.length} blocks to send`);

      // Send missing blocks in batches
      for (let i = 0; i < missingBlocks.length; i++) {
        const blockId = missingBlocks[i];
        const offset = blockId * chunkSize;
        const payload = data.slice(offset, Math.min(offset + chunkSize, data.length));

        // Skip empty payloads (can occur at exact data boundaries)
        if (payload.length === 0) {
          continue;
        }

        // Use response for last block in batch or overall
        const isLastInBatch = ((i + 1) % this.BATCH_SIZE === 0);
        const isLastBlock = (i === missingBlocks.length - 1);
        const useResponse = isLastInBatch || isLastBlock;

        await this.sendBlockFast(blockId, totalBlocks, payload, useResponse);
        stats.sentBlocks++;

        if (onProgress) {
          onProgress(i + 1, missingBlocks.length);
        }
      }

      // Wait for MCU to process, then check status
      await new Promise(r => setTimeout(r, this.BATCH_DELAY_MS));

      // Check if all blocks received after first round
      if (retryRound === 0) {
        let status;
        try {
          status = await this.queryStatus();
          const stillMissing = this.getMissingBlocks(status, totalBlocks);
          if (stillMissing.length === 0) {
            const elapsed = ((Date.now() - stats.startTime) / 1000).toFixed(1);
            console.log(`Transfer complete: ${stats.totalBlocks} blocks, ${stats.sentBlocks} sent, 0 retries, ${elapsed}s`);
            return true;
          }
        } catch (e) {
          console.warn('Post-transfer status query failed:', e);
        }
      }
    }

    const elapsed = ((Date.now() - stats.startTime) / 1000).toFixed(1);
    console.error(`Transfer failed: ${stats.totalBlocks} blocks, ${stats.sentBlocks} sent, ${stats.retries} retries, ${elapsed}s`);
    throw new Error('Transfer failed after max retries');
  },

  /**
   * Initialize the transfer module (call on connect)
   */
  init() {
    this.pendingStatus = null;
    this.statusResolver = null;
    this.statusRequestId = 0;
    this.firstBlockSent = false;
  }
};

// Export for use in main.js
if (typeof window !== 'undefined') {
  window.BleTransfer = BleTransfer;
}
