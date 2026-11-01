#!/usr/bin/env python3
"""Turn captured U8g2 framebuffers into a PNG contact sheet (stdlib only)."""
import pathlib
import struct
import sys
import zlib


def chunk(kind, data):
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))


directory = pathlib.Path(sys.argv[1])
names = ['offline', 'syncing', 'idle', 'generating', 'synthesizing', 'speaking', 'error', 'clock',
         'sensor-offline', 'sensor-present', 'sensor-no-distance', 'sensor-stale', 'sensor-clear',
         'arrival', 'sound', 'mcp-happy', 'mcp-sad']
scale, margin = 4, 16
width, height = 2 * (128 * scale + margin) + margin, ((len(names) + 1) // 2) * (64 * scale + margin) + margin
image = [bytearray([28] * width) for _ in range(height)]
for index, name in enumerate(names):
    values = (directory / (name + '.pbm')).read_text().split()
    assert values[:3] == ['P1', '128', '64']
    pixels = [int(value) for value in values[3:]]
    assert len(pixels) == 8192
    left = margin + (index % 2) * (128 * scale + margin)
    top = margin + (index // 2) * (64 * scale + margin)
    for y in range(64 * scale):
        for x in range(128 * scale):
            image[top + y][left + x] = 0 if pixels[(y // scale) * 128 + x // scale] else 255
raw = b''.join(b'\0' + row for row in image)
png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!IIBBBBB', width, height, 8, 0, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
target = directory / 'oled-preview.png'
target.write_bytes(png)
print(target)
