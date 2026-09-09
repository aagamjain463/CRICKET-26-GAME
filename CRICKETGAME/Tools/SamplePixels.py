"""Reads sRGB values out of a capture PNG with nothing but the standard library.

Reasoning about a render is not a substitute for measuring it. Two of the worst bugs in this
project's history -- an outfield lit only by the sky's lower hemisphere, and a pitch drawing with
the engine default grey material -- both looked plausible in a screenshot and were only pinned down
by sampling actual pixels. Use this before and after any lighting, material or grade change.

    python3 Tools/SamplePixels.py <capture.png> X,Y[,label] ...
    python3 Tools/SamplePixels.py <capture.png> --box X,Y,W,H[,label] ...

--box averages a region, which is what you want for anything soft-edged such as a contact shadow.
"""
import struct
import sys
import zlib


def load(path):
    data = open(path, 'rb').read()
    i, idat, w, h, bit, colour = 8, b'', 0, 0, 0, 0
    while i < len(data):
        length = struct.unpack('>I', data[i:i + 4])[0]
        kind = data[i + 4:i + 8]
        chunk = data[i + 8:i + 8 + length]
        i += 12 + length
        if kind == b'IHDR':
            w, h, bit, colour = struct.unpack('>IIBB', chunk[:10])
        elif kind == b'IDAT':
            idat += chunk
        elif kind == b'IEND':
            break
    raw = zlib.decompress(idat)
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colour]
    bpp = channels * (bit // 8)
    stride = w * bpp
    out, prev, p = bytearray(), bytearray(stride), 0
    for _ in range(h):
        filt = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if filt == 1:
                line[x] = (line[x] + a) & 255
            elif filt == 2:
                line[x] = (line[x] + b) & 255
            elif filt == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif filt == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pred) & 255
        out += line
        prev = line
    return w, h, channels, bytes(out)


def pixel(img, x, y):
    w, _, ch, d = img
    o = (y * w + x) * ch
    return d[o], d[o + 1], d[o + 2]


def average(img, x0, y0, bw, bh):
    w, h, _, _ = img
    n = 0
    acc = [0, 0, 0]
    for y in range(max(0, y0), min(h, y0 + bh)):
        for x in range(max(0, x0), min(w, x0 + bw)):
            r, g, b = pixel(img, x, y)
            acc[0] += r
            acc[1] += g
            acc[2] += b
            n += 1
    return tuple(round(v / max(1, n), 1) for v in acc)


def main():
    img = load(sys.argv[1])
    print('image %s  %dx%d' % (sys.argv[1], img[0], img[1]))
    args = sys.argv[2:]
    box = False
    for arg in args:
        if arg == '--box':
            box = True
            continue
        parts = arg.split(',')
        if box:
            x, y, bw, bh = (int(v) for v in parts[:4])
            label = parts[4] if len(parts) > 4 else ''
            value = average(img, x, y, bw, bh)
            print('%-26s box(%d,%d %dx%d) = %s  luma=%.1f'
                  % (label, x, y, bw, bh, value, .2126 * value[0] + .7152 * value[1] + .0722 * value[2]))
        else:
            x, y = int(parts[0]), int(parts[1])
            label = parts[2] if len(parts) > 2 else ''
            value = pixel(img, x, y)
            print('%-26s (%d,%d) = %s  luma=%.1f'
                  % (label, x, y, value, .2126 * value[0] + .7152 * value[1] + .0722 * value[2]))


if __name__ == '__main__':
    main()
