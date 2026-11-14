#!/usr/bin/env python3
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ICON_PATH = os.path.join(ROOT, "src", "data", "icon.png")


def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
    pos = 8
    idat = []
    palette = None
    trns = None
    width = height = depth = ctype = 0
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        pos += 4
        ctag = data[pos:pos + 4]
        pos += 4
        chunk = data[pos:pos + length]
        pos += length + 4
        if ctag == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
            assert depth == 8 and interlace == 0
        elif ctag == b"PLTE":
            palette = chunk
        elif ctag == b"tRNS":
            trns = chunk
        elif ctag == b"IDAT":
            idat.append(chunk)
        elif ctag == b"IEND":
            break

    raw = zlib.decompress(b"".join(idat))
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    stride = width * channels
    out = bytearray()
    prev = bytearray(stride)
    p = 0
    for _ in range(height):
        filt = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if filt == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif filt == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif filt == 3:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif filt == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                est = a + b - c
                pa, pb, pc = abs(est - a), abs(est - b), abs(est - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out += line
        prev = line

    pixels = []
    for i in range(0, len(out), channels):
        if ctype == 6:
            pixels.append((out[i], out[i + 1], out[i + 2], out[i + 3]))
        elif ctype == 2:
            pixels.append((out[i], out[i + 1], out[i + 2], 255))
        elif ctype == 0:
            v = out[i]
            pixels.append((v, v, v, 255))
        elif ctype == 4:
            v = out[i]
            pixels.append((v, v, v, out[i + 1]))
        else:
            idx = out[i]
            r, g, b = palette[idx * 3:idx * 3 + 3]
            a = trns[idx] if trns is not None and idx < len(trns) else 255
            pixels.append((r, g, b, a))
    return width, height, pixels


FONT = {
    " ": ["00000"] * 7,
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01111", "10000", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "J": ["00111", "00010", "00010", "00010", "00010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "Q": ["01110", "10001", "10001", "10001", "10101", "10010", "01101"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "11011", "10001"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11111", "00010", "00100", "00010", "00001", "10001", "01110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
    "-": ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    ".": ["00000", "00000", "00000", "00000", "00000", "01100", "01100"],
}


def save_png(path, width, height, pixels):
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += pixels[y * width * 3:(y + 1) * width * 3]

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    blob = b"\x89PNG\r\n\x1a\n"
    blob += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    blob += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    blob += chunk(b"IEND", b"")
    open(path, "wb").write(blob)


def background(width, height, glow_x, glow_y):
    px = bytearray(width * height * 3)
    glow_r = width * 0.70
    glow_ry = height * 0.85
    for y in range(height):
        for x in range(width):
            t = y / max(1, height - 1)
            r = 7 + 18 * t
            g = 9 + 12 * t
            b = 22 + 26 * t
            dx = (x - glow_x) / glow_r
            dy = (y - glow_y) / glow_ry
            d = dx * dx + dy * dy
            glow = max(0.0, 1.0 - d)
            glow = glow * glow
            r += 68 * glow
            g += 45 * glow
            b += 16 * glow
            vx = (x - width * 0.5) / (width * 0.5)
            vy = (y - height * 0.5) / (height * 0.5)
            vig = 1.0 - 0.30 * min(1.0, (vx * vx + vy * vy) * 0.85)
            i = (y * width + x) * 3
            px[i] = max(0, min(255, int(r * vig)))
            px[i + 1] = max(0, min(255, int(g * vig)))
            px[i + 2] = max(0, min(255, int(b * vig)))
    return px


def blend(px, width, height, x, y, color, alpha):
    if x < 0 or y < 0 or x >= width or y >= height or alpha <= 0:
        return
    i = (y * width + x) * 3
    a = min(1.0, alpha)
    for k in range(3):
        px[i + k] = int(px[i + k] * (1.0 - a) + color[k] * a)


def draw_icon(px, width, height, icon, icon_w, icon_h, ox, oy, scale):
    for y in range(icon_h * scale):
        for x in range(icon_w * scale):
            r, g, b, a = icon[(y // scale) * icon_w + (x // scale)]
            if a < 8:
                continue
            blend(px, width, height, ox + x, oy + y, (r, g, b), a / 255.0)


def text_width(text, scale, spacing=1):
    return len(text) * 6 * scale - spacing * scale


def draw_text(px, width, height, text, x, y, scale, color, spacing=1, shadow=None):
    if shadow is not None:
        draw_text(px, width, height, text, x + scale * 2, y + scale * 2, scale, shadow, spacing)
    cursor = x
    for ch in text.upper():
        glyph = FONT.get(ch, FONT[" "])
        for row in range(7):
            for col in range(5):
                if glyph[row][col] == "1":
                    for sy in range(scale):
                        for sx in range(scale):
                            blend(px, width, height, cursor + col * scale + sx,
                                  y + row * scale + sy, color, 1.0)
        cursor += 6 * scale + spacing * scale


def gold_frame(px, width, height, inset, thickness, color, alpha):
    for t in range(thickness):
        for x in range(inset + t, width - inset - t):
            blend(px, width, height, x, inset + t, color, alpha)
            blend(px, width, height, x, height - 1 - inset - t, color, alpha)
        for y in range(inset + t, height - inset - t):
            blend(px, width, height, inset + t, y, color, alpha)
            blend(px, width, height, width - 1 - inset - t, y, color, alpha)


def main():
    icon_w, icon_h, icon = read_png(ICON_PATH)

    icon0_w, icon0_h = 320, 176
    px = background(icon0_w, icon0_h, 108, 88)
    gold_frame(px, icon0_w, icon0_h, 5, 2, (196, 158, 82), 0.55)
    draw_icon(px, icon0_w, icon0_h, icon, icon_w, icon_h, 28, 24, 4)

    tx = 176
    gold = (226, 190, 112)
    cream = (238, 226, 196)
    draw_text(px, icon0_w, icon0_h, "SDLPOP", tx, 58, 2, gold, shadow=(0, 0, 0))
    draw_text(px, icon0_w, icon0_h, "PS3", tx, 88, 2, cream, shadow=(0, 0, 0))
    for x in range(tx, tx + text_width("SDLPOP", 2)):
        blend(px, icon0_w, icon0_h, x, 120, (180, 146, 78), 0.8)
    save_png(os.path.join(HERE, "ICON0.PNG"), icon0_w, icon0_h, px)

    pic_w, pic_h = 1920, 1080
    px = background(pic_w, pic_h, 960, 420)
    draw_icon(px, pic_w, pic_h, icon, icon_w, icon_h, 832, 120, 8)

    title = "SDLPOP PS3"
    scale = 12
    tx = (pic_w - text_width(title, scale)) // 2
    draw_text(px, pic_w, pic_h, title, tx, 452, scale, (232, 198, 124), shadow=(0, 0, 0))
    for x in range(430, 1490):
        blend(px, pic_w, pic_h, x, 576, (188, 152, 82), 0.9)
        blend(px, pic_w, pic_h, x, 578, (188, 152, 82), 0.35)
    subtitle = "PRINCE OF PERSIA"
    ss = 4
    sx = (pic_w - text_width(subtitle, ss)) // 2
    draw_text(px, pic_w, pic_h, subtitle, sx, 624, ss, (222, 212, 190), shadow=(0, 0, 0))

    foot = "BASED ON THE ORIGINAL BY JORDAN MECHNER"
    fs = 2
    fx = (pic_w - text_width(foot, fs)) // 2
    draw_text(px, pic_w, pic_h, foot, fx, 982, fs, (120, 112, 100))
    save_png(os.path.join(HERE, "PIC1.PNG"), pic_w, pic_h, px)

    print("assets: ICON0.PNG 320x176, PIC1.PNG 1920x1080")


if __name__ == "__main__":
    main()
