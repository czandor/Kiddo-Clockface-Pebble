"""Generates the Pebble / Rebble Appstore icons (80x80, 144x144) and the
720x320 banner for Kiddo, in the Kiddo cartoon style (colored letters with a
black outline and a bottom-right shadow).

Needs the original Agent Orange font in original/font/AgentOrange.ttf (local
only, not distributed) and the store screenshots from tools/store-shots.sh.

Usage: python -P tools/make-store.py
"""
from collections import deque
from pathlib import Path

import numpy as np

from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
store = root / 'store'
font_path = root / 'original' / 'font' / 'AgentOrange.ttf'
hero_path = store / 'emery-01-main.png'

# Vivid Pebble colors (Kiddo random pool)
YELLOW = (255, 255, 0)
ORANGE = (255, 85, 0)
PINK = (255, 85, 170)
GREEN = (85, 255, 85)
CYAN = (0, 255, 255)
VIOLET = (170, 85, 255)
SKY = (85, 170, 255)
NAVY = (18, 28, 64)


def label(mask):
    """4-connected component labels of a boolean numpy mask (0 = not in mask)."""
    h, w = mask.shape
    labels = np.zeros((h, w), np.int32)
    count = 0
    for y0, x0 in zip(*np.nonzero(mask)):
        if labels[y0, x0]:
            continue
        count += 1
        labels[y0, x0] = count
        queue = deque([(y0, x0)])
        while queue:
            y, x = queue.popleft()
            for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not labels[ny, nx]:
                    labels[ny, nx] = count
                    queue.append((ny, nx))
    return labels, count


def grow(mask):
    out = mask.copy()
    out[1:, :] |= mask[:-1, :]
    out[:-1, :] |= mask[1:, :]
    out[:, 1:] |= mask[:, :-1]
    out[:, :-1] |= mask[:, 1:]
    return out


def kiddo_letter(ch, font, color):
    """Agent Orange draws hollow letters (outline + 3D shadow). As in the Fitbit
    Kiddo glyphs, the letter body inside the outer outline gets the color, the
    counters (the hole of O, D...) stay transparent. Returns an RGBA image."""
    pad = 4
    l, t, r, b = font.getbbox(ch)
    w, h = r - l + 2 * pad, b - t + 2 * pad
    m = Image.new('L', (w, h), 0)
    ImageDraw.Draw(m).text((pad - l, pad - t), ch, font=font, fill=255)
    stroke = np.array(m) >= 128
    regions, n_regions = label(~stroke)
    outside = regions == regions[0, 0]
    strokes, _ = label(stroke)
    outer_strokes = set(np.unique(strokes[grow(outside) & stroke])) - {0}
    body = np.zeros_like(stroke)
    for i in range(1, n_regions + 1):
        region = regions == i
        if region[0, 0]:
            continue
        touching = set(np.unique(strokes[grow(region) & stroke])) - {0}
        if touching & outer_strokes:
            body |= region
    rgba = np.zeros((h, w, 4), np.uint8)
    rgba[body] = (*color, 255)
    rgba[stroke] = (0, 0, 0, 255)
    return Image.fromarray(rgba, 'RGBA'), (l - pad, t - pad)


def kiddo_text(img, text, font_path_, size, xy, colors, scale=4, spacing=0):
    """Draws Kiddo-style text at xy, rendered at `scale`x and downsampled."""
    font = ImageFont.truetype(str(font_path_), size * scale)
    letters = []
    x = 0
    for i, ch in enumerate(text):
        im, (ox, oy) = kiddo_letter(ch, font, colors[i % len(colors)])
        letters.append((im, x + ox, oy))
        x += font.getlength(ch) + spacing * scale
    min_x = min(lx for _, lx, _ in letters)
    min_y = min(ly for _, _, ly in letters)
    W = int(max(lx + im.width for im, lx, _ in letters) - min_x)
    H = int(max(ly + im.height for im, _, ly in letters) - min_y)
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    for im, lx, ly in letters:
        canvas.alpha_composite(im, (int(lx - min_x), int(ly - min_y)))
    small = canvas.resize((max(1, W // scale), max(1, H // scale)), Image.LANCZOS)
    img.paste(small, xy, small)
    return small.size


def make_icon(size, out_path):
    S = size * 4
    img = Image.new('RGB', (S, S), SKY)
    probe = Image.new('RGBA', (S, S))
    w, h = kiddo_text(probe, 'K', font_path, int(size * 0.86), (0, 0), [YELLOW], scale=4)
    kiddo_text(img, 'K', font_path, int(size * 0.86) * 4, ((S - w * 4) // 2, (S - h * 4) // 2), [YELLOW], scale=1)
    mask = Image.new('L', (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.2), fill=255)
    out = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    out.paste(img, (0, 0), mask)
    out.resize((size, size), Image.LANCZOS).save(out_path)
    print(f'Saved {out_path.name} ({size}x{size})')


def make_banner(out_path):
    W, H = 720, 320
    banner = Image.new('RGB', (W, H), SKY)
    px = banner.load()
    for y in range(H):  # soft top-to-bottom shade of the Kiddo sky blue
        t = y / (H - 1)
        row = (int(85 + 40 * t), int(170 + 35 * t), 255)
        for x in range(W):
            px[x, y] = row

    kiddo_text(banner, 'KIDDO', font_path, 92, (36, 14), [YELLOW, PINK, GREEN, ORANGE, CYAN], spacing=1)

    draw = ImageDraw.Draw(banner)
    try:
        font_sub = ImageFont.truetype(r'C:\Windows\Fonts\segoeuib.ttf', 17)
        font_bul = ImageFont.truetype(r'C:\Windows\Fonts\segoeui.ttf', 16)
        font_tag = ImageFont.truetype(r'C:\Windows\Fonts\segoeuib.ttf', 12)
    except OSError:
        font_sub = ImageFont.load_default()
        font_bul = font_tag = font_sub

    draw.text((40, 128), 'For Pebble Time, Time Round, Time 2 and Round 2', font=font_sub, fill=NAVY)
    bullets = [
        '• Rainbow cartoon digits from the original Fitbit Kiddo',
        '• Vivid random colors every minute, hour, day or flick',
        '• Health board: steps, heart rate, sleep, distance, kcal',
        '• Quiet Time bell, wrist-flick board, on-demand HR',
    ]
    y = 164
    for b in bullets:
        draw.text((40, y), b, font=font_bul, fill=NAVY)
        y += 25
    sx = 40
    for col in (YELLOW, ORANGE, PINK, VIOLET, SKY, CYAN, GREEN):
        draw.rounded_rectangle([sx, 270, sx + 22, 280], radius=3, fill=col, outline=(0, 0, 0))
        sx += 28
    draw.text((40, 292), 'DESIGN & CODE: ANDOR CZAFIK', font=font_tag, fill=NAVY)

    if hero_path.exists():
        im = Image.open(hero_path).convert('RGB')
        cx, cy, pad = 592, 160, 12
        bx0, by0 = cx - im.width // 2 - pad, cy - im.height // 2 - pad
        bx1, by1 = cx + im.width // 2 + pad, cy + im.height // 2 + pad
        draw.rounded_rectangle([bx0 + 7, by0 + 9, bx1 + 7, by1 + 9], radius=24, fill=(0, 0, 0))
        draw.rounded_rectangle([bx0, by0, bx1, by1], radius=22, fill=(24, 26, 30), outline=(0, 0, 0), width=3)
        banner.paste(im, (cx - im.width // 2, cy - im.height // 2))
    banner.save(out_path)
    print(f'Saved {out_path.name}')


make_icon(80, store / 'icon-80x80.png')
make_icon(144, store / 'icon-144x144.png')
make_banner(store / 'banner-720x320.png')
