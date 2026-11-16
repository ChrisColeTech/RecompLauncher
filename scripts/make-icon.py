#!/usr/bin/env python3
# RecompLauncher - native game library and build launcher.
"""Render the application icon: a dark controller silhouette on the lime accent square.
Writes src/launcher/icons/app.ico (16-256 px, embedded in the Windows exe) and app.png (256 px, window icon on
every platform). Developer tool; needs Pillow (pip install pillow). Run after changing the design and commit both files."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'src/launcher/icons'
S = 1024                      # draw large, downsample per size
LIME = (0xb5, 0xf3, 0x6b, 255)
INK = (0x14, 0x18, 0x16, 255)


def draw():
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, S - 1, S - 1), radius=int(S * 0.22), fill=LIME)
    # controller body: a wide rounded bar plus two grips
    body = (int(S * .11), int(S * .30), int(S * .89), int(S * .64))
    d.rounded_rectangle(body, radius=int(S * .17), fill=INK)
    for cx in (S * .255, S * .745):
        r = S * .16
        d.ellipse((cx - r, S * .54 - r, cx + r, S * .54 + r), fill=INK)
    # d-pad (left) and two buttons (right), cut out in lime
    cx, cy, arm, w = S * .295, S * .465, S * .105, S * .044
    d.rounded_rectangle((cx - arm, cy - w, cx + arm, cy + w), radius=int(w * .5), fill=LIME)
    d.rounded_rectangle((cx - w, cy - arm, cx + w, cy + arm), radius=int(w * .5), fill=LIME)
    for bx, by in ((S * .665, S * .51), (S * .75, S * .425)):
        r = S * .052
        d.ellipse((bx - r, by - r, bx + r, by + r), fill=LIME)
    return img


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    big = draw()
    sizes = [16, 24, 32, 48, 64, 128, 256]
    big.resize((256, 256), Image.LANCZOS).save(OUT / 'app.png')
    big.save(OUT / 'app.ico', sizes=[(s, s) for s in sizes])
    # side-by-side preview at real sizes for review (not committed)
    preview = Image.new('RGBA', (sum(sizes) + 20 * len(sizes), 276), (30, 33, 31, 255))
    x = 10
    for s in sizes:
        preview.paste(big.resize((s, s), Image.LANCZOS), (x, 266 - s)); x += s + 20
    preview.save(ROOT / 'build/icon-preview.png')
    print('wrote', OUT / 'app.ico', OUT / 'app.png')


if __name__ == '__main__': main()
