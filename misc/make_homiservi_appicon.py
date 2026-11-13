#!/usr/bin/env python3
# Copyright 2006-2026 The QElectroTech Team
# This file is part of QElectroTech.
#
# QElectroTech is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# QElectroTech is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
"""Build the Homiservi-SCH program icon from the Homiservi symbol.

Run from the repository root (needs Pillow):

    python3 misc/make_homiservi_appicon.py

It reads ico/homiservi/homiservi-symbol.png (the chip and gear of the logo,
blue on transparent) and writes:

    ico/homiservi/homiservi-<size>.png   16, 24, 32, 48, 64, 128 and 256 px
    ico/homiservi/homiservi.ico          the same sizes, for the Windows .exe

Every size is a solid Homiservi blue tile with the drawing in white, so the
icon reads on light and dark title bars, panels and taskbars alike. The
symbol's thin lines melt into a blur below 48 px: 16, 24 and 32 px get a
plain chip drawn on whole pixels instead, the rest gets the symbol itself.
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
ICO_DIR = ROOT / "ico" / "homiservi"
SYMBOL = ICO_DIR / "homiservi-symbol.png"

BLUE = (0, 74, 159, 255)
WHITE = (255, 255, 255, 255)
SUPERSAMPLE = 8

# Pixel layout of the chip for the small sizes, in whole pixels:
# body = outer square of the chip (x0, y0, x1, y1), drawn as an outline
# of `line` px; die = filled square in its middle; pins = offsets of the
# pins along each side, `line` px wide, from `pin_from` to the body.
SMALL = {
    16: dict(radius=3, body=(4, 4, 12, 12), line=1, die=(6, 6, 10, 10),
             pins=(6, 9), pin_from=2),
    24: dict(radius=4, body=(6, 6, 18, 18), line=2, die=(10, 10, 14, 14),
             pins=(8, 14), pin_from=3),
    32: dict(radius=6, body=(8, 8, 24, 24), line=2, die=(12, 12, 20, 20),
             pins=(11, 15, 19), pin_from=4),
}
LARGE = (48, 64, 128, 256)


def tile(size):
	"""Blue rounded tile, supersampled so that only the corners are soft."""
	k = SUPERSAMPLE
	img = Image.new("RGBA", (size * k, size * k), (0, 0, 0, 0))
	radius = SMALL[size]["radius"] if size in SMALL else round(size * 0.19)
	ImageDraw.Draw(img).rounded_rectangle(
		(0, 0, size * k - 1, size * k - 1), radius=radius * k, fill=BLUE)
	return img


def small_icon(size):
	"""Chip drawn on whole pixels: crisp at 16, 24 and 32 px."""
	spec = SMALL[size]
	k = SUPERSAMPLE
	img = tile(size)
	draw = ImageDraw.Draw(img)

	def rect(x0, y0, x1, y1):
		draw.rectangle((x0 * k, y0 * k, x1 * k - 1, y1 * k - 1), fill=WHITE)

	x0, y0, x1, y1 = spec["body"]
	t = spec["line"]
	rect(x0, y0, x1, y0 + t)
	rect(x0, y1 - t, x1, y1)
	rect(x0, y0, x0 + t, y1)
	rect(x1 - t, y0, x1, y1)
	rect(*spec["die"])

	a, b = spec["pin_from"], size - spec["pin_from"]
	for p in spec["pins"]:
		rect(a, p, x0, p + t)   # left
		rect(x1, p, b, p + t)   # right
		rect(p, a, p + t, y0)   # top
		rect(p, y1, p + t, b)   # bottom
	return img.resize((size, size), Image.BOX)


def large_icon(size, symbol):
	"""The Homiservi symbol in white on the tile."""
	k = SUPERSAMPLE
	img = tile(size)
	alpha = symbol.getchannel("A")
	box = alpha.getbbox()
	mask = alpha.crop(box)
	inner = round(size * k * 0.78)
	scale = inner / max(mask.size)
	mask = mask.resize((round(mask.width * scale), round(mask.height * scale)),
	                   Image.LANCZOS)
	white = Image.new("RGBA", mask.size, WHITE)
	white.putalpha(mask)
	img.alpha_composite(white, ((img.width - mask.width) // 2,
	                            (img.height - mask.height) // 2))
	return img.resize((size, size), Image.LANCZOS)


def main():
	symbol = Image.open(SYMBOL).convert("RGBA")
	images = {}
	for size in SMALL:
		images[size] = small_icon(size)
	for size in LARGE:
		images[size] = large_icon(size, symbol)
	for size, img in sorted(images.items()):
		img.save(ICO_DIR / f"homiservi-{size}.png", optimize=True)
	sizes = sorted(images)
	images[256].save(ICO_DIR / "homiservi.ico",
	                 sizes=[(s, s) for s in sizes],
	                 append_images=[images[s] for s in sizes[:-1]])
	print("wrote", ", ".join(f"homiservi-{s}.png" for s in sizes),
	      "and homiservi.ico")


if __name__ == "__main__":
	main()
