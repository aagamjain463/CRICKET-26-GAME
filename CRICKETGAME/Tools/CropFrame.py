"""Crops and magnifies a region of a capture PNG, using nothing but the standard library.

A 1600x900 broadcast frame is the wrong tool for judging a glove, a pad edge or a shoe: the
striker's kit is a few dozen pixels tall in it. Sampling single pixels (SamplePixels.py) answers
"what colour is this"; this answers "what shape is this". Crop the striker out, magnify, and look
at the silhouette you actually shipped.

    python3 Tools/CropFrame.py <capture.png> <out.png> X,Y,W,H [scale]

X,Y is the top-left of the crop in source pixels. Scale is an integer nearest-neighbour zoom
(default 4) -- nearest neighbour on purpose, so magnification never invents an edge that the
renderer did not draw.
"""
import struct
import sys
import zlib

sys.path.insert(0, __file__.rsplit('/', 1)[0])
from SamplePixels import load


def write(path, width, height, channels, pixels):
    raw = bytearray()
    stride = width * channels
    for y in range(height):
        raw.append(0)
        raw += pixels[y * stride:(y + 1) * stride]

    def chunk(kind, body):
        return (struct.pack('>I', len(body)) + kind + body
                + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff))

    colour = {1: 0, 3: 2, 4: 6}[channels]
    open(path, 'wb').write(
        b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, colour, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(bytes(raw), 6))
        + chunk(b'IEND', b''))


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    source, destination = sys.argv[1], sys.argv[2]
    x, y, w, h = (int(v) for v in sys.argv[3].split(','))
    scale = int(sys.argv[4]) if len(sys.argv) > 4 else 4
    width, height, channels, pixels = load(source)
    x, y = max(0, min(x, width - 1)), max(0, min(y, height - 1))
    w, h = min(w, width - x), min(h, height - y)
    out = bytearray()
    for row in range(h * scale):
        line = bytearray()
        base = (y + row // scale) * width * channels
        for column in range(w * scale):
            start = base + (x + column // scale) * channels
            line += pixels[start:start + channels]
        out += line
    write(destination, w * scale, h * scale, channels, bytes(out))
    print(f'{destination} {w * scale}x{h * scale} from {source} at {x},{y} {w}x{h} scale {scale}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
