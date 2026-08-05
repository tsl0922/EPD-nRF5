// RLE-compress a byte array.
function rleCompress(data, maxLiteralSize = 128) {
  const input = data instanceof Uint8Array ? data : new Uint8Array(data);
  const result = [];
  let i = 0;

  while (i < input.length) {
    let runLen = 1;
    // Count repeating bytes (max 130)
    while (i + runLen < input.length && runLen < 130 && input[i + runLen] === input[i]) {
      runLen++;
    }

    if (runLen >= 3) {
      // Repeat run: control = 0x80 | (runLen - 3)
      result.push(0x80 | (runLen - 3));
      result.push(input[i]);
      i += runLen;
    } else {
      // Literal run: collect up to maxLiteralSize bytes
      const literalStart = i;
      let literalLen = 0;
      while (i < input.length && literalLen < maxLiteralSize) {
        // Check if the next bytes form a long repeat run (>= 3)
        if (i + 2 < input.length && input[i] === input[i + 1] && input[i] === input[i + 2]) {
          break;
        }
        literalLen++;
        i++;
      }
      if (literalLen === 0) {
        // Should not happen, but fallback: single byte literal
        result.push(0x00);
        result.push(input[i++]);
      } else {
        result.push(literalLen - 1);
        for (let j = literalStart; j < literalStart + literalLen; j++) {
          result.push(input[j]);
        }
      }
    }
  }

  return new Uint8Array(result);
}

// RLE-compress ENTIRE data, then split the compressed stream at RLE-code
// boundaries so each chunk is a valid, complete RLE stream that fills the
// MTU as much as possible without truncating any code.
function rleCompressMTU(data, maxChunkSize) {
  // Constrain literal runs so a single code never exceeds maxChunkSize.
  // The control byte takes 1 byte, so data bytes ≤ maxChunkSize - 1.
  const maxLit = Math.min(maxChunkSize - 1, 128);
  const input = rleCompress(data, maxLit);     // compress with safe literal limit
  const chunks = [];
  let i = 0;
  let start = 0;

  while (i < input.length) {
    const control = input[i];
    // RLE code length: repeat = 2 bytes, literal = 1 + (control + 1) bytes
    const codeLen = (control & 0x80) ? 2 : (control + 2);

    // Flush if adding this code would exceed maxChunkSize AND we already
    // have data in the current chunk (don't split a code across chunks).
    // If a single code is larger than maxChunkSize it gets its own chunk.
    if (i - start + codeLen > maxChunkSize && i > start) {
      chunks.push(input.slice(start, i));
      start = i;
    }
    i += codeLen;
  }

  if (i > start) chunks.push(input.slice(start, i));
  return chunks;
}
