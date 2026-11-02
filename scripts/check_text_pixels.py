#!/usr/bin/env python3
"""check_text_pixels.py: prüft, dass im Fensterbild der Laufschrift gelbe Schriftpunkte zu sehen sind.

Warum es das Skript gibt: Issue #13 meldet, dass in der Oberfläche kein Text erscheint. Die GPU-Tests prüfen die
Session, dieses Skript prüft das fertige Fensterbild (Player, Vorschau, Zeichnen). Das Beispiel
examples/effekte/laufschrift.whfx malt gelbe Schrift (1, 0.9, 0.3) über sehr dunkles Plasma; gelbe Bildpunkte
kann es sonst nirgends im Fenster geben. Aufruf: check_text_pixels.py bild.png
"""
import sys

from PIL import Image


def main() -> int:
    image = Image.open(sys.argv[1]).convert("RGB")
    yellow = sum(1 for r, g, b in image.getdata() if r > 200 and g > 150 and b < 140)
    print(f"{sys.argv[1]}: {image.width}x{image.height}, gelbe Bildpunkte: {yellow}")
    if yellow < 20:
        print(f"::error title=Kein Text im Fensterbild::{sys.argv[1]} enthält nur {yellow} gelbe Bildpunkte (Issue 13)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
