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
"""Build the Homiservi icon themes from the line icons in ico/.

Run from the repository root:

    python3 misc/make_homiservi_icons.py

It writes:

    ico/themes/homiservi/index.theme             the light theme, inherits "qet"
    ico/themes/homiservi/scalable/*.svg          the icons in the light colors
    ico/themes/homiservi-dark/index.theme        the dark theme, inherits "qet-dark"
    ico/themes/homiservi-dark/scalable/*.svg     the icons in the dark colors
    ico/homiservi-icons.qrc                      resource file listing both themes

The .qrc also serves the names of CLASSIC_ALIASES in QET's "qet" and
"qet-dark" themes, from the files ico/icon-themes.qrc gives the icon they
copy.

The themes hold only the toolbar icons, listed in ICONS. Every other
name is inherited from QET's own themes, so an icon missing here still
shows. The sources are line icons drawn in currentColor on a 24 pixel
grid: ico/lucide/ (Lucide, ISC license) and ico/homiservi/icons/ (drawn
for Homiservi-SCH). Qt draws an SVG with currentColor in black, so each
theme gets a copy in its own colors: a neutral ink, and the Homiservi
blue for the icons in BRAND.

Idempotent: running it twice changes nothing.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ICO = ROOT / "ico"
THEMES = ICO / "themes"
QRC = ICO / "homiservi-icons.qrc"

# Theme name to source SVG, relative to ico/.
ICONS = {
    # File
    "project-new": "lucide/folder-plus.svg",
    "document-new": "lucide/file-plus.svg",
    "document-open": "lucide/folder-open.svg",
    "folder-open": "lucide/folder-open.svg",
    "document-save": "lucide/save.svg",
    "document-save-as": "lucide/save-all.svg",
    "project-close": "lucide/folder-x.svg",
    "document-print": "lucide/printer.svg",
    "application-pdf": "lucide/file-down.svg",
    "view-refresh": "lucide/refresh-cw.svg",
    # Edit
    "edit-undo": "lucide/undo-2.svg",
    "edit-redo": "lucide/redo-2.svg",
    "edit-cut": "lucide/scissors.svg",
    "edit-copy": "lucide/copy.svg",
    "edit-paste": "lucide/clipboard-paste.svg",
    "format-painter": "lucide/paintbrush.svg",
    "edit-delete": "lucide/trash-2.svg",
    "transform-rotate": "lucide/rotate-cw.svg",
    # View
    "select": "lucide/mouse-pointer-2.svg",
    "move": "lucide/hand.svg",
    "grid": "lucide/grid-3x3.svg",
    "guides": "homiservi/icons/guides.svg",
    "zoom-draw": "lucide/scan.svg",
    "zoom-fit-best": "lucide/expand.svg",
    "zoom-original": "lucide/search.svg",
    "zoom-in": "lucide/zoom-in.svg",
    "zoom-out": "lucide/zoom-out.svg",
    "document-print-frame": "lucide/frame.svg",
    # Diagram and conductors
    "folio-properties": "lucide/file-cog.svg",
    "conductor": "homiservi/icons/conductor.svg",
    "autoconnect": "homiservi/icons/autoconnect.svg",
    "conductor-reset": "homiservi/icons/conductor-reset.svg",
    "conductor-numerate-ends": "homiservi/icons/conductor-numerate-ends.svg",
    "conductor-end-labels": "homiservi/icons/conductor-end-labels.svg",
    # Drawing
    "textfield": "lucide/text-cursor-input.svg",
    "text": "lucide/type.svg",
    "insert-image": "lucide/image.svg",
    "pdf-import": "lucide/file-input.svg",
    "line": "homiservi/icons/line.svg",
    "rectangle": "lucide/square.svg",
    "ellipse": "homiservi/icons/ellipse.svg",
    "arc": "homiservi/icons/arc.svg",
    "polygon": "lucide/pentagon.svg",
    "draw-bezier-curves": "lucide/spline.svg",
    "draw-fillet": "homiservi/icons/draw-fillet.svg",
    "terminalstrip": "homiservi/icons/terminalstrip.svg",
    "terminal": "homiservi/icons/terminal.svg",
    "snap-points": "homiservi/icons/snap-points.svg",
    # Depth
    "bring_forward": "lucide/bring-to-front.svg",
    "raise": "lucide/arrow-up.svg",
    "lower": "lucide/arrow-down.svg",
    "send_backward": "lucide/send-to-back.svg",
    # Element editor
    "names": "lucide/languages.svg",
    "element-edit": "lucide/sliders-horizontal.svg",
    "document-import": "lucide/file-search.svg",
}

# Icons drawn in the Homiservi blue: saving, the PDF export and the
# electrical tools. The others take the neutral ink.
BRAND = {
    "document-save", "application-pdf",
    "conductor", "autoconnect", "conductor-reset",
    "conductor-numerate-ends", "conductor-end-labels",
    "terminalstrip", "terminal",
}

# Names new to the toolbar actions, served in QET's own themes by the files
# of an existing icon, so the classic icons look as before. Name to the
# QET icon it copies.
CLASSIC_ALIASES = {
    "conductor-numerate-ends": "conductor-reset",
    "conductor-end-labels": "conductor-reset",
}

# Theme name, the QET theme it inherits, neutral ink, brand ink. The dark
# inks are the dark palette's text color and the blue lightened to read
# on it.
THEME_COLORS = [
    ("homiservi", "qet", "#2e3338", "#004a9e"),
    ("homiservi-dark", "qet-dark", "#dcdcdc", "#5b9be6"),
]

SVG_TAG = re.compile(r"<svg\b([^>]*)>(.*)</svg>", re.S)
COMMENT = re.compile(r"<!--.*?-->", re.S)
# Presentation attributes the sources set on the root element. QtSvg
# applies them more reliably on a group, so they move there.
ROOT_ATTRS = ("fill", "stroke", "stroke-width", "stroke-linecap", "stroke-linejoin")


def colored(source, ink):
    text = COMMENT.sub("", source.read_text(encoding="utf-8"))
    match = SVG_TAG.search(text)
    if not match:
        raise SystemExit(f"{source}: no <svg> element")
    attrs, body = match.groups()
    group = []
    for name in ROOT_ATTRS:
        value = re.search(rf'\s{name}="([^"]*)"', attrs)
        if value:
            group.append(f'{name}="{value.group(1)}"')
    body = "\n".join(line.strip() for line in body.strip().splitlines())
    svg = (
        '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">\n'
        f'<g {" ".join(group)}>\n{body}\n</g>\n</svg>\n'
    )
    return svg.replace("currentColor", ink)


def index_theme(name, inherits):
    return (
        "[Icon Theme]\n"
        f"Name={name}\n"
        "Comment=Homiservi-SCH line icons\n"
        f"Inherits={inherits}\n"
        "Directories=scalable\n"
        "\n"
        "[scalable]\n"
        "Size=24\n"
        "MinSize=8\n"
        "MaxSize=512\n"
        "Type=Scalable\n"
    )


def write_if_changed(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == data:
        return
    path.write_text(data, encoding="utf-8", newline="\n")


def classic_aliases():
    """The .qrc lines that serve CLASSIC_ALIASES in QET's themes."""
    qet = (ICO / "icon-themes.qrc").read_text(encoding="utf-8")
    lines = []
    for name, copied in sorted(CLASSIC_ALIASES.items()):
        found = re.findall(
            rf'<file alias="(themes/qet(?:-dark)?/[^/"]+/){re.escape(copied)}\.svg">([^<]+)</file>', qet)
        if not found:
            raise SystemExit(f"{copied} is not in ico/icon-themes.qrc")
        lines += [f'        <file alias="{folder}{name}.svg">{source}</file>' for folder, source in found]
    return lines


def main():
    files = []
    for theme, inherits, neutral, brand in THEME_COLORS:
        folder = THEMES / theme
        write_if_changed(folder / "index.theme", index_theme(theme, inherits))
        files.append(f"themes/{theme}/index.theme")
        wanted = set()
        for name, source in sorted(ICONS.items()):
            ink = brand if name in BRAND else neutral
            write_if_changed(folder / "scalable" / f"{name}.svg", colored(ICO / source, ink))
            wanted.add(f"{name}.svg")
            files.append(f"themes/{theme}/scalable/{name}.svg")
        for old in (folder / "scalable").glob("*.svg"):
            if old.name not in wanted:
                old.unlink()

    lines = ["<!DOCTYPE RCC>", "<!-- Generated by misc/make_homiservi_icons.py. Do not edit. -->",
             '<RCC version="1.0">', '    <qresource prefix="/ico">']
    lines += [f"        <file>{f}</file>" for f in files]
    lines += classic_aliases()
    lines += ["    </qresource>", "</RCC>", ""]
    write_if_changed(QRC, "\n".join(lines))
    print(f"{len(ICONS)} icons in {len(THEME_COLORS)} themes")


if __name__ == "__main__":
    main()
