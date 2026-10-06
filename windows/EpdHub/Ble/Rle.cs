namespace EpdHub.Ble;

/// <summary>
/// RLE format understood by the EPD-nRF5 firmware (see EPD_service.c rle_decompress_from):
///   control &amp; 0x80  -> repeat run: count = (control &amp; 0x7F) + 3, followed by one value byte
///   otherwise        -> literal run: count = control + 1, followed by count bytes
/// This is a port of html/js/rle.js so the output matches the web client byte for byte.
/// </summary>
public static class Rle
{
    public static byte[] Compress(ReadOnlySpan<byte> input, int maxLiteralSize = 128)
    {
        var result = new List<byte>(input.Length / 2 + 16);
        int i = 0;
        while (i < input.Length)
        {
            int runLen = 1;
            while (i + runLen < input.Length && runLen < 130 && input[i + runLen] == input[i]) runLen++;

            if (runLen >= 3)
            {
                result.Add((byte)(0x80 | (runLen - 3)));
                result.Add(input[i]);
                i += runLen;
            }
            else
            {
                int literalStart = i;
                int literalLen = 0;
                while (i < input.Length && literalLen < maxLiteralSize)
                {
                    if (i + 2 < input.Length && input[i] == input[i + 1] && input[i] == input[i + 2]) break;
                    literalLen++;
                    i++;
                }
                if (literalLen == 0)
                {
                    result.Add(0x00);
                    result.Add(input[i++]);
                }
                else
                {
                    result.Add((byte)(literalLen - 1));
                    for (int j = literalStart; j < literalStart + literalLen; j++) result.Add(input[j]);
                }
            }
        }
        return result.ToArray();
    }

    /// <summary>
    /// Compress the whole buffer, then split the stream at code boundaries so every chunk
    /// is a complete RLE stream no longer than <paramref name="maxChunkSize"/>.
    /// </summary>
    public static List<byte[]> CompressChunked(ReadOnlySpan<byte> data, int maxChunkSize)
    {
        int maxLit = Math.Min(128, maxChunkSize - 1);
        byte[] input = Compress(data, maxLit);
        var chunks = new List<byte[]>();
        int start = 0, i = 0;
        while (i < input.Length)
        {
            int codeLen = (input[i] & 0x80) != 0 ? 2 : input[i] + 2;
            if (i + codeLen - start > maxChunkSize && i > start)
            {
                chunks.Add(input[start..i]);
                start = i;
            }
            i += codeLen;
        }
        if (i > start) chunks.Add(input[start..i]);
        return chunks;
    }
}
