#!/usr/bin/env python3
"""make_icons.py: erzeugt das Programmsymbol (Windows .ico und eingebettete PNGs) aus der Vorlage packaging/linux/whiteholefx.svg.

Warum es das gibt: Unter Linux zeigt das Paket das Symbol als SVG (packaging/linux/whiteholefx.svg). Windows braucht eine
.ico-Datei in der .exe, und Qt braucht für das Fenstersymbol und den Startbildschirm Rastergrafiken. Das Symbol ist einfache
Geometrie (Rechteck, Leuchtring, Scheibe, sechs Punkte); es wird hier mit denselben Maßen und Farben wie im SVG gezeichnet,
damit keine SVG-Bibliothek nötig ist. Wer das SVG ändert, ändert die Zahlen unten mit und führt das Skript neu aus:

  python3 scripts/make_icons.py          schreibt packaging/windows/whiteholefx.ico und apps/whitehole_studio/icons/app-*.png

Benötigt Pillow und NumPy (nur zum Erzeugen, nicht zum Bauen; die Ergebnisse liegen im Repository).
"""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
BASE = 1024  # Zeichengröße, danach verkleinert (glatte Kanten)
SCALE = BASE / 256.0


def lerp(a, b, t):
    return a + (b - a) * t


def hex_rgb(value):
    return np.array([int(value[i:i + 2], 16) for i in (1, 3, 5)], dtype=np.float64)


def render():
    y, x = np.mgrid[0:BASE, 0:BASE].astype(np.float64)
    x = (x + 0.5) / SCALE
    y = (y + 0.5) / SCALE
    img = np.zeros((BASE, BASE, 4), dtype=np.float64)

    # Hintergrund: abgerundetes Quadrat mit Verlauf (diagonal, #1b1d27 -> #0b0c11), rx = 44
    t = (x + y) / 512.0
    bg = np.stack([lerp(hex_rgb("#1b1d27")[i], hex_rgb("#0b0c11")[i], t) for i in range(3)], axis=-1)
    r = 44.0
    cx = np.clip(x, r, 256 - r)
    cy = np.clip(y, r, 256 - r)
    inside = (x - cx) ** 2 + (y - cy) ** 2 <= r * r
    img[..., :3] = bg
    img[..., 3] = inside

    def over(color_rgb, alpha):
        a = alpha[..., None]
        img[..., :3] = img[..., :3] * (1 - a) + color_rgb * a
        img[..., 3] = np.maximum(img[..., 3], alpha)

    # Leuchtring: Kreis r = 104, radialer Verlauf (Stopps bei 55 %, 78 %, 100 % des Radius)
    d = np.hypot(x - 128, y - 128) / 104.0
    stops = [(0.55, "#ffb347", 0.0), (0.78, "#ff8a1f", 0.9), (1.0, "#ff5a00", 0.0)]
    ring_alpha = np.zeros_like(d)
    ring_rgb = np.zeros(d.shape + (3,))
    for (d0, c0, a0), (d1, c1, a1) in zip(stops, stops[1:]):
        m = (d >= d0) & (d <= d1)
        u = np.clip((d - d0) / (d1 - d0), 0, 1)
        ring_alpha = np.where(m, lerp(a0, a1, u), ring_alpha)
        for i in range(3):
            ring_rgb[..., i] = np.where(m, lerp(hex_rgb(c0)[i], hex_rgb(c1)[i], u), ring_rgb[..., i])
    ring_alpha = np.where(d <= 1.0, ring_alpha, 0.0)
    a = (ring_alpha * inside)[..., None]
    img[..., :3] = img[..., :3] * (1 - a) + ring_rgb * a

    # dunkle Scheibe in der Mitte
    over(hex_rgb("#05060a"), (np.hypot(x - 128, y - 128) <= 58.0).astype(np.float64))
    # sechs blaue Punkte
    for px, py in ((40, 40), (68, 40), (96, 40), (160, 216), (188, 216), (216, 216)):
        over(hex_rgb("#6cb4ff"), (np.hypot(x - px, y - py) <= 5.0).astype(np.float64))

    out = np.clip(img, 0, 255)
    out[..., 3] *= 255
    return Image.fromarray(out.astype(np.uint8), "RGBA")


def main():
    base = render()
    icons = ROOT / "apps" / "whitehole_studio" / "icons"
    icons.mkdir(parents=True, exist_ok=True)
    for size in (256, 64, 32):
        base.resize((size, size), Image.LANCZOS).save(icons / f"app-{size}.png", optimize=True)
    ico = ROOT / "packaging" / "windows" / "whiteholefx.ico"
    base.resize((256, 256), Image.LANCZOS).save(ico, format="ICO", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    print("geschrieben:", ico.relative_to(ROOT), "und", ", ".join(f"app-{s}.png" for s in (256, 64, 32)))


if __name__ == "__main__":
    main()
