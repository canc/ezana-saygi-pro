#!/usr/bin/env python3
"""Generate a simple muted-speaker multi-size ICO (16/32/48)."""
from __future__ import print_function
import os
import struct
import math

SIZES = (16, 32, 48)


def clamp(v):
    return 0 if v < 0 else 255 if v > 255 else int(v)


def draw(size):
    # RGBA top-down
    px = [(0, 0, 0, 0)] * (size * size)

    def setp(x, y, r, g, b, a):
        if 0 <= x < size and 0 <= y < size:
            i = y * size + x
            if a == 0:
                return
            or_, og, ob, oa = px[i]
            if oa == 0:
                px[i] = (r, g, b, a)
            else:
                out_a = a + oa * (255 - a) // 255
                px[i] = (
                    (r * a + or_ * oa * (255 - a) // 255) // max(out_a, 1),
                    (g * a + og * oa * (255 - a) // 255) // max(out_a, 1),
                    (b * a + ob * oa * (255 - a) // 255) // max(out_a, 1),
                    out_a,
                )

    def fill_round_rect(x0, y0, x1, y1, rad, color):
        r, g, b, a = color
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                dx = 0
                dy = 0
                if x < x0 + rad:
                    dx = x0 + rad - x
                elif x > x1 - rad:
                    dx = x - (x1 - rad)
                if y < y0 + rad:
                    dy = y0 + rad - y
                elif y > y1 - rad:
                    dy = y - (y1 - rad)
                if dx * dx + dy * dy <= rad * rad + rad:
                    setp(x, y, r, g, b, a)

    def fill_poly(points, color):
        r, g, b, a = color
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        minx, maxx = int(min(xs)), int(max(xs))
        miny, maxy = int(min(ys)), int(max(ys))
        for y in range(miny, maxy + 1):
            for x in range(minx, maxx + 1):
                # winding / ray cast
                inside = False
                j = len(points) - 1
                for i, (xi, yi) in enumerate(points):
                    xj, yj = points[j]
                    if ((yi > y) != (yj > y)) and (
                        x < (xj - xi) * (y - yi) / float(yj - yi + 1e-9) + xi
                    ):
                        inside = not inside
                    j = i
                if inside:
                    setp(x, y, r, g, b, a)

    def draw_line(x0, y0, x1, y1, w, color):
        r, g, b, a = color
        steps = max(abs(x1 - x0), abs(y1 - y0), 1) * 2
        for i in range(steps + 1):
            t = i / float(steps)
            cx = x0 + (x1 - x0) * t
            cy = y0 + (y1 - y0) * t
            rad = w / 2.0
            for oy in range(-int(rad) - 1, int(rad) + 2):
                for ox in range(-int(rad) - 1, int(rad) + 2):
                    if ox * ox + oy * oy <= rad * rad + 0.5:
                        setp(int(round(cx + ox)), int(round(cy + oy)), r, g, b, a)

    s = float(size)
    # speaker body (left rectangle) + cone
    body = (36, 40, 48, 255)
    outline = (18, 18, 20, 255)
    mute = (200, 48, 48, 255)
    mute_dark = (140, 24, 24, 255)

    bx0 = int(s * 0.12)
    by0 = int(s * 0.32)
    bx1 = int(s * 0.38)
    by1 = int(s * 0.68)
    fill_round_rect(bx0, by0, bx1, by1, max(1, size // 16), body)

    cone = [
        (s * 0.36, s * 0.34),
        (s * 0.72, s * 0.14),
        (s * 0.72, s * 0.86),
        (s * 0.36, s * 0.66),
    ]
    fill_poly(cone, body)

    # mute slash
    w = max(2.0, s * 0.10)
    draw_line(int(s * 0.18), int(s * 0.82), int(s * 0.82), int(s * 0.18), w + 1.5, mute_dark)
    draw_line(int(s * 0.18), int(s * 0.82), int(s * 0.82), int(s * 0.18), w, mute)

    return px


def rgba_to_dib(px, size):
    # 32-bit XOR bitmap bottom-up + AND mask
    xor = bytearray()
    for y in range(size - 1, -1, -1):
        for x in range(size):
            r, g, b, a = px[y * size + x]
            xor += struct.pack("BBBB", b, g, r, a)
    row = ((size + 31) // 32) * 4
    mask = bytearray(row * size)
    # AND mask 0 where pixel is visible
    for y in range(size):
        src_y = size - 1 - y
        for x in range(size):
            r, g, b, a = px[src_y * size + x]
            if a < 16:
                byte_i = y * row + (x // 8)
                mask[byte_i] |= 0x80 >> (x % 8)
    hdr = struct.pack(
        "<IiiHHIIiiII",
        40,
        size,
        size * 2,
        1,
        32,
        0,
        len(xor),
        0,
        0,
        0,
        0,
    )
    return hdr + xor + mask


def write_ico(path):
    images = []
    for size in SIZES:
        images.append(rgba_to_dib(draw(size), size))
    offset = 6 + 16 * len(images)
    buf = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    for size, data in zip(SIZES, images):
        buf += struct.pack(
            "<BBBBHHII",
            size if size < 256 else 0,
            size if size < 256 else 0,
            0,
            0,
            1,
            32,
            len(data),
            offset,
        )
        offset += len(data)
    for data in images:
        buf += data
    with open(path, "wb") as f:
        f.write(buf)
    print("wrote", path, "(%d bytes)" % len(buf))


if __name__ == "__main__":
    out = os.path.join(os.path.dirname(__file__), "app.ico")
    write_ico(out)
