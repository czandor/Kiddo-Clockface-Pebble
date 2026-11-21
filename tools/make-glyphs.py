"""Builds the Kiddo glyph sprite sheets and the matching C glyph tables.

Input: the original Fitbit Kiddo artwork in original/source/resources/images
(each glyph is a grayscale fill mask `x.png` plus an RGBA outline/shadow layer
`xb.png`). New icons (sleep, bell, muted bell) are drawn here in the same style.

Every glyph is scaled to the platform size and quantized to four colors:
transparent, black (outline and shadow), white (the fill, recolored at runtime
by changing the palette entry) and light gray (the battery terminal).

Output:
  pebble/resources/images/kiddo_<platform>.png  (one sprite sheet per platform)
  pebble/src/c/glyphs.h                         (sheet rectangles and metrics)

Usage: python -P tools/make-glyphs.py [--preview DIR]
"""
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SRC = os.path.join(ROOT, 'original', 'source', 'resources', 'images')
OUT_IMG = os.path.join(ROOT, 'pebble', 'resources', 'images')
OUT_H = os.path.join(ROOT, 'pebble', 'src', 'c', 'glyphs.h')

# (x offset, visible width) of each glyph in its box, from the Fitbit const.js SIZES table
BIG = {'0': (8, 65), '1': (20, 40), '2': (14, 52), '3': (15, 49), '4': (10, 60), '5': (18, 44), '6': (16, 48),
       '7': (17, 46), '8': (13, 53), '9': (15, 50), ':': (28, 22)}
BIG_BOX, BIG_SPACER = 80, 4
SMALL = {'0': (2, 21), '1': (6, 13), '2': (4, 16), '3': (5, 15), '4': (3, 19), '5': (5, 14), '6': (5, 15),
         '7': (5, 15), '8': (4, 17), '9': (5, 15), 'a': (3, 20), 'b': (5, 15), 'c': (5, 15), 'd': (5, 15),
         'e': (6, 13), 'f': (6, 13), 'g': (5, 15), 'h': (4, 16), 'i': (8, 9), 'j': (4, 17), 'l': (6, 13),
         'm': (1, 23), 'n': (3, 18), 'o': (2, 21), 'p': (4, 17), 'r': (4, 17), 's': (4, 16), 't': (4, 17),
         'u': (3, 19), 'v': (3, 19), 'w': (1, 24), 'y': (2, 22), '.': (9, 7)}
SMALL_BOX, SMALL_SPACER, SMALL_SPACE = 25, 1, 8
BIG_CHARS = '0123456789:'
SMALL_CHARS = '0123456789abcdefghijlmnoprstuvwy.'
FILE_NAME = {':': 'ddot', '.': 'dot'}
ICONS = ['hr', 'steps', 'active', 'distance', 'calories', 'sleep', 'bell', 'belloff']
ICON_FILES = {'hr': 'health/hr', 'steps': 'health/steps', 'active': 'health/activeZoneMinutes',
              'distance': 'health/distance', 'calories': 'health/calories'}
ICON_BOX = 35
BATT_SIZE = (50, 30)
NAIL_BOX = 20

# Per platform: big digit scale, small text/icon scale, health board size (Fitbit: 300x100).
PLATFORMS = {
    'basalt': dict(big=0.48, small=0.56, board=(140, 50), sheet_w=256),
    'chalk': dict(big=0.50, small=0.56, board=(136, 48), sheet_w=256),
    'emery': dict(big=0.66, small=0.72, board=(194, 68), sheet_w=256),
    'gabbro': dict(big=0.74, small=0.78, board=(202, 70), sheet_w=320),
}

MASK_T = 110   # fill where the scaled mask is at least this bright
LAYER_T = 110  # outline layer counts where its alpha is at least this
DARK_T = 150   # outline layer pixels darker than this are black

T, K, W, G = 0, 1, 2, 3  # quantized classes: transparent, black, white (fill), gray
CLASS_RGBA = {T: (0, 0, 0, 0), K: (0, 0, 0, 255), W: (255, 255, 255, 255), G: (170, 170, 170, 255)}


def load_pair(name):
    mask = Image.open(os.path.join(SRC, name + '.png')).convert('L')
    layer = Image.open(os.path.join(SRC, name + 'b.png')).convert('RGBA')
    return mask, layer


def styled_pair(fill_mask, outline=4, shadow=6):
    """Fitbit Kiddo icon style for new artwork: black outline plus a bottom-right shadow."""
    grown = fill_mask.filter(ImageFilter.MaxFilter(2 * outline + 1))
    shifted = ImageChops.offset(grown, shadow, shadow)
    dark = ImageChops.lighter(grown, shifted)
    layer = Image.new('RGBA', fill_mask.size, (0, 0, 0, 0))
    layer.paste((0, 0, 0, 255), (0, 0), dark)
    layer.paste((255, 255, 255, 255), (0, 0), fill_mask)
    return fill_mask, layer


def draw_bell(d, s):
    """Bell silhouette on a 140x140 canvas (s = 1.0)."""
    def p(x, y):
        return (x * s, y * s)
    def half(t):
        return 31 + 15 * (t ** 2.2)
    steps = 40
    left = [p(70 - half(i / steps), 52 + 44 * i / steps) for i in range(steps + 1)]
    right = [p(70 + half(i / steps), 52 + 44 * i / steps) for i in range(steps, -1, -1)]
    d.polygon(left + right, fill=255)
    d.ellipse([p(39, 20), p(101, 84)], fill=255)          # round dome
    d.rounded_rectangle([p(18, 90), p(122, 106)], radius=8 * s, fill=255)  # rim
    d.ellipse([p(62, 10), p(78, 26)], fill=255)          # knob
    d.ellipse([p(57, 102), p(83, 126)], fill=255)        # clapper


def new_icon(name):
    size = 140
    m = Image.new('L', (size, size), 0)
    d = ImageDraw.Draw(m)
    if name == 'sleep':
        d.ellipse([18, 22, 118, 122], fill=255)
        d.ellipse([46, 4, 138, 96], fill=0)              # crescent opening to the top right
    elif name == 'bell':
        draw_bell(d, 1.0)
    elif name == 'belloff':
        draw_bell(d, 1.0)
        d.line([(14, 14), (126, 126)], fill=0, width=30)   # gap around the slash
        d.line([(20, 20), (120, 120)], fill=255, width=13)  # the slash itself
    else:
        raise ValueError(name)
    return styled_pair(m.resize((ICON_BOX * 4, ICON_BOX * 4), Image.LANCZOS))


def quantize(mask, layer, size):
    """Scale a (mask, outline layer) pair to size and classify every pixel."""
    w, h = size
    ms = mask.resize((w, h), Image.LANCZOS).load()
    ls = layer.resize((w, h), Image.LANCZOS).load()
    out = [[T] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = ls[x, y]
            if ms[x, y] >= MASK_T:
                out[y][x] = W
            elif a >= LAYER_T:
                if (r + g + b) / 3 < DARK_T:
                    out[y][x] = K
                elif ms[x, y] >= 20:
                    out[y][x] = W
                else:
                    out[y][x] = G
    return out


def crop(cells):
    h, w = len(cells), len(cells[0])
    ys = [y for y in range(h) if any(cells[y])]
    xs = [x for x in range(w) if any(cells[y][x] for y in range(h))]
    if not ys:
        return 0, 0, [[T]]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    return x0, y0, [row[x0:x1 + 1] for row in cells[y0:y1 + 1]]


def build(platform, cfg):
    sb, ss = cfg['big'], cfg['small']
    tiles = []  # (group, key, cells, dx, dy, adv16)

    def text_tile(group, ch, box, scale, metrics, spacer):
        off, width = metrics[ch]
        px = round(box * scale)
        prefix = 'big/' if group == 'big' else 'small/'
        mask, layer = load_pair(prefix + FILE_NAME.get(ch, ch))
        x0, y0, cells = crop(quantize(mask, layer, (px, px)))
        tiles.append((group, ch, cells, x0 - round(off * scale), y0, round((width + spacer) * scale * 16)))

    for ch in BIG_CHARS:
        text_tile('big', ch, BIG_BOX, sb, BIG, BIG_SPACER)
    for ch in SMALL_CHARS:
        text_tile('small', ch, SMALL_BOX, ss, SMALL, SMALL_SPACER)
    icon_px = round(ICON_BOX * ss)
    for name in ICONS:
        pair = load_pair(ICON_FILES[name]) if name in ICON_FILES else new_icon(name)
        x0, y0, cells = crop(quantize(pair[0], pair[1], (icon_px, icon_px)))
        tiles.append(('icon', name, cells, x0, y0, 0))
    batt = (round(BATT_SIZE[0] * ss), round(BATT_SIZE[1] * ss))
    x0, y0, cells = crop(quantize(*load_pair('batt'), batt))
    tiles.append(('misc', 'batt', cells, x0, y0, 0))
    nail_px = max(5, round(NAIL_BOX * ss))
    x0, y0, cells = crop(quantize(*load_pair('small/dot'), (nail_px, nail_px)))
    tiles.append(('misc', 'nail', cells, x0, y0, 0))
    x0, y0, cells = crop(quantize(*load_pair('table'), cfg['board']))
    tiles.append(('misc', 'board', cells, x0, y0, 0))

    # Shelf packing, tallest first, 1 px gap.
    sheet_w = cfg['sheet_w']
    order = sorted(range(len(tiles)), key=lambda i: (-len(tiles[i][2]), -len(tiles[i][2][0])))
    pos = {}
    x = y = shelf_h = 0
    for i in order:
        cells = tiles[i][2]
        w, h = len(cells[0]), len(cells)
        if x + w > sheet_w:
            x, y, shelf_h = 0, y + shelf_h + 1, 0
        pos[i] = (x, y)
        x += w + 1
        shelf_h = max(shelf_h, h)
    sheet_h = y + shelf_h
    img = Image.new('RGBA', (sheet_w, sheet_h), CLASS_RGBA[T])
    px = img.load()
    for i, (_, _, cells, _, _, _) in enumerate(tiles):
        ox, oy = pos[i]
        for yy, row in enumerate(cells):
            for xx, c in enumerate(row):
                px[ox + xx, oy + yy] = CLASS_RGBA[c]
    meta = dict(big_box=round(BIG_BOX * sb), small_box=round(SMALL_BOX * ss), icon_box=icon_px,
                batt=batt, nail_box=nail_px, board=cfg['board'],
                big_sp16=round(BIG_SPACER * sb * 16), small_sp16=round(SMALL_SPACER * ss * 16),
                small_space16=round((SMALL_SPACE + SMALL_SPACER) * ss * 16))
    return img, tiles, pos, meta


def c_glyph(tile, xy):
    _, key, cells, dx, dy, adv16 = tile
    return '  {%3d, %3d, %3d, %3d, %3d, %3d, %4d},  // %s' % (
        xy[0], xy[1], len(cells[0]), len(cells), dx, dy, adv16, key)


def main():
    preview = None
    if '--preview' in sys.argv:
        preview = sys.argv[sys.argv.index('--preview') + 1]
    os.makedirs(OUT_IMG, exist_ok=True)
    lines = [
        '// Generated by tools/make-glyphs.py from the original Fitbit Kiddo artwork. Do not edit.',
        '#pragma once',
        '#include <pebble.h>',
        '',
        'typedef struct {',
        '  uint16_t x, y;  // rectangle in the sprite sheet',
        '  uint8_t w, h;',
        '  int8_t dx, dy;  // offset of the cropped glyph from the pen position / box origin',
        '  uint16_t adv16; // pen advance in 1/16 px (text glyphs)',
        '} KGlyph;',
        '',
        '#define BIG_CHARS "%s"' % BIG_CHARS,
        '#define SMALL_CHARS "%s"' % SMALL_CHARS,
        'enum { %s, ICON_COUNT };' % ', '.join('ICON_' + n.upper() for n in ICONS),
        'enum { MISC_BATT, MISC_NAIL, MISC_BOARD, MISC_COUNT };',
        '',
    ]
    first = True
    for platform, cfg in PLATFORMS.items():
        img, tiles, pos, meta = build(platform, cfg)
        path = os.path.join(OUT_IMG, 'kiddo_%s.png' % platform)
        img.save(path)
        if preview:
            os.makedirs(preview, exist_ok=True)
            bg = Image.new('RGBA', img.size, (230, 160, 130, 255))
            bg.alpha_composite(img)
            bg.resize((img.size[0] * 3, img.size[1] * 3), Image.NEAREST).save(
                os.path.join(preview, 'sheet_%s.png' % platform))
        groups = {g: [(t, pos[i]) for i, t in enumerate(tiles) if t[0] == g] for g in ('big', 'small', 'icon', 'misc')}
        lines.append('#%s defined(PBL_PLATFORM_%s)' % ('if' if first else 'elif', platform.upper()))
        first = False
        lines.append('#define KIDDO_SHEET RESOURCE_ID_KIDDO_SHEET_%s  // %dx%d' % (platform.upper(), img.size[0], img.size[1]))
        lines.append('#define BIG_BOX %d' % meta['big_box'])
        lines.append('#define SMALL_BOX %d' % meta['small_box'])
        lines.append('#define ICON_BOX %d' % meta['icon_box'])
        lines.append('#define BATT_W %d' % meta['batt'][0])
        lines.append('#define BATT_H %d' % meta['batt'][1])
        lines.append('#define NAIL_BOX %d' % meta['nail_box'])
        lines.append('#define BOARD_W %d' % meta['board'][0])
        lines.append('#define BOARD_H %d' % meta['board'][1])
        lines.append('#define BIG_SP16 %d' % meta['big_sp16'])
        lines.append('#define SMALL_SP16 %d' % meta['small_sp16'])
        lines.append('#define SMALL_SPACE16 %d' % meta['small_space16'])
        for g, cname in (('big', 'GL_BIG'), ('small', 'GL_SMALL'), ('icon', 'GL_ICON'), ('misc', 'GL_MISC')):
            lines.append('static const KGlyph %s[%d] = {' % (cname, len(groups[g])))
            lines.extend(c_glyph(t, xy) for t, xy in groups[g])
            lines.append('};')
        print('%-7s sheet %dx%d, big box %d, small box %d, icon %d, board %dx%d' % (
            platform, img.size[0], img.size[1], meta['big_box'], meta['small_box'], meta['icon_box'], *meta['board']))
    lines.append('#endif')
    with open(OUT_H, 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
