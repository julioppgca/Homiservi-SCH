/*
	Copyright 2006-2026 The QElectroTech Team
	This file is part of QElectroTech.

	QElectroTech is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	QElectroTech is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with QElectroTech.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "modulegenerator.h"

#include "../qetapp.h"

#include <QFontMetricsF>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace {
		///Length of the lead between the box and a terminal
	constexpr int lead = 10;
		///Space between the box and the pin names
	constexpr qreal margin = 3;
	const char *line_style = "line-style:normal;line-weight:normal;filling:none;color:black";

	int ceilToGrid(qreal value)
	{
		return int(std::ceil(value / 10.0)) * 10;
	}

	qreal widest(const QStringList &names, const QFontMetricsF &metrics)
	{
		qreal width = 0;
		for (const QString &name : names)
			width = std::max(width, metrics.horizontalAdvance(name));
		return width;
	}

	void addLine(QDomElement &description, qreal x1, qreal y1, qreal x2, qreal y2)
	{
		QDomElement e = description.ownerDocument().createElement(QStringLiteral("line"));
		e.setAttribute(QStringLiteral("x1"), x1);
		e.setAttribute(QStringLiteral("y1"), y1);
		e.setAttribute(QStringLiteral("x2"), x2);
		e.setAttribute(QStringLiteral("y2"), y2);
		e.setAttribute(QStringLiteral("end1"), QStringLiteral("none"));
		e.setAttribute(QStringLiteral("end2"), QStringLiteral("none"));
		e.setAttribute(QStringLiteral("length1"), QStringLiteral("1.5"));
		e.setAttribute(QStringLiteral("length2"), QStringLiteral("1.5"));
		e.setAttribute(QStringLiteral("style"), QString::fromLatin1(line_style));
		e.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
		description.appendChild(e);
	}

	void addText(QDomElement &description, const QString &text, qreal x, qreal y,
		     int rotation, const QFont &font)
	{
		QDomElement e = description.ownerDocument().createElement(QStringLiteral("text"));
		e.setAttribute(QStringLiteral("text"), text);
		e.setAttribute(QStringLiteral("x"), QString::number(x, 'f', 1));
		e.setAttribute(QStringLiteral("y"), QString::number(y, 'f', 1));
		e.setAttribute(QStringLiteral("rotation"), rotation);
		e.setAttribute(QStringLiteral("font"), font.toString());
		e.setAttribute(QStringLiteral("color"), QStringLiteral("#000000"));
		description.appendChild(e);
	}

	void addTerminal(QDomElement &description, const QString &name, int x, int y,
			 const char *orientation)
	{
		QDomElement e = description.ownerDocument().createElement(QStringLiteral("terminal"));
		e.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
		e.setAttribute(QStringLiteral("name"), name);
		e.setAttribute(QStringLiteral("x"), x);
		e.setAttribute(QStringLiteral("y"), y);
		e.setAttribute(QStringLiteral("orientation"), QString::fromLatin1(orientation));
		e.setAttribute(QStringLiteral("type"), QStringLiteral("Generic"));
		description.appendChild(e);
	}
}

/**
	@return the names of the pins of one side, in order
*/
QStringList ModuleGenerator::pinNames(const SidePins &pins)
{
	QStringList names;
	for (int i = 0; i < pins.count; ++i) {
		if (pins.numbering == ByteBit) {
			const int index = pins.start * 8 + i;
			names << QStringLiteral("%1%2.%3").arg(pins.prefix).arg(index / 8).arg(index % 8);
		} else {
			names << pins.prefix + QString::number(pins.start + i * pins.step);
		}
	}
	return names;
}

/**
	@brief ModuleGenerator::build
	The box has its top-left corner at (0, 0); every terminal is on the
	10 px grid.
	@return an element definition holding the parts only
*/
QDomDocument ModuleGenerator::build(const Settings &settings)
{
	const int pitch = std::max(10, ceilToGrid(settings.pitch));
	QStringList names[SideCount];
	for (int side = 0; side < SideCount; ++side)
		names[side] = pinNames(settings.sides[side]);

	const QFont pin_font = QETApp::diagramTextsFont(7);
	const QFont body_font = QETApp::diagramTextsFont(9);
	const QFontMetricsF pin_metrics(pin_font);
	const QFontMetricsF body_metrics(body_font);

	auto names_size = [&](Side side) {
		return settings.show_names && !names[side].isEmpty()
				? widest(names[side], pin_metrics) + 2 * margin : 0.0;
	};
	const qreal body_width = settings.body_text.isEmpty()
			? 0 : body_metrics.horizontalAdvance(settings.body_text) + 10;
	const qreal body_height = settings.body_text.isEmpty() ? 0 : body_metrics.height() + 6;

	const int vertical_pins = int(std::max(names[Left].size(), names[Right].size()));
	const int horizontal_pins = int(std::max(names[Top].size(), names[Bottom].size()));
	const int width = std::max({(horizontal_pins + 1) * pitch,
				    ceilToGrid(names_size(Left) + names_size(Right) + body_width),
				    40});
	const int height = std::max({(vertical_pins + 1) * pitch,
				     ceilToGrid(names_size(Top) + names_size(Bottom) + body_height),
				     40});

	QDomDocument document;
	QDomElement definition = document.createElement(QStringLiteral("definition"));
	definition.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
	document.appendChild(definition);
	QDomElement description = document.createElement(QStringLiteral("description"));
	definition.appendChild(description);

	QDomElement rect = document.createElement(QStringLiteral("rect"));
	rect.setAttribute(QStringLiteral("x"), 0);
	rect.setAttribute(QStringLiteral("y"), 0);
	rect.setAttribute(QStringLiteral("width"), width);
	rect.setAttribute(QStringLiteral("height"), height);
	rect.setAttribute(QStringLiteral("rx"), 0);
	rect.setAttribute(QStringLiteral("ry"), 0);
	rect.setAttribute(QStringLiteral("style"), QString::fromLatin1(line_style));
	rect.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
	description.appendChild(rect);

		//Centre of a name across the pin line: its baseline sits half the
		//cap height past the pin
	const qreal half_cap = pin_metrics.capHeight() / 2;

	for (int i = 0; i < names[Left].size(); ++i) {
		const int y = (i + 1) * pitch;
		addLine(description, 0, y, -lead, y);
		addTerminal(description, names[Left].at(i), -lead, y, "w");
		if (settings.show_names)
			addText(description, names[Left].at(i), margin, y + half_cap, 0, pin_font);
	}
	for (int i = 0; i < names[Right].size(); ++i) {
		const int y = (i + 1) * pitch;
		addLine(description, width, y, width + lead, y);
		addTerminal(description, names[Right].at(i), width + lead, y, "e");
		if (settings.show_names)
			addText(description, names[Right].at(i),
				width - margin - pin_metrics.horizontalAdvance(names[Right].at(i)),
				y + half_cap, 0, pin_font);
	}
		//Top and bottom names read upwards (rotation 270), as in the
		//terminal strips of the collection
	for (int i = 0; i < names[Top].size(); ++i) {
		const int x = (i + 1) * pitch;
		addLine(description, x, 0, x, -lead);
		addTerminal(description, names[Top].at(i), x, -lead, "n");
		if (settings.show_names)
			addText(description, names[Top].at(i), x + half_cap,
				margin + pin_metrics.horizontalAdvance(names[Top].at(i)), 270, pin_font);
	}
	for (int i = 0; i < names[Bottom].size(); ++i) {
		const int x = (i + 1) * pitch;
		addLine(description, x, height, x, height + lead);
		addTerminal(description, names[Bottom].at(i), x, height + lead, "s");
		if (settings.show_names)
			addText(description, names[Bottom].at(i), x + half_cap, height - margin, 270, pin_font);
	}

	if (!settings.body_text.isEmpty()) {
		const qreal left = names_size(Left);
		const qreal right = width - names_size(Right);
		const qreal top = names_size(Top);
		const qreal bottom = height - names_size(Bottom);
		addText(description, settings.body_text,
			(left + right - body_metrics.horizontalAdvance(settings.body_text)) / 2,
			(top + bottom + body_metrics.capHeight()) / 2, 0, body_font);
	}

	if (settings.label_field) {
		QDomElement label = document.createElement(QStringLiteral("dynamic_text"));
		label.setAttribute(QStringLiteral("x"), 0);
		label.setAttribute(QStringLiteral("y"), (names[Top].isEmpty() ? 0 : -lead) - 18);
		label.setAttribute(QStringLiteral("z"), 100);
		label.setAttribute(QStringLiteral("text_width"), -1);
		label.setAttribute(QStringLiteral("Halignment"), QStringLiteral("AlignLeft"));
		label.setAttribute(QStringLiteral("Valignment"), QStringLiteral("AlignTop"));
		label.setAttribute(QStringLiteral("frame"), QStringLiteral("false"));
		label.setAttribute(QStringLiteral("rotation"), 0);
		label.setAttribute(QStringLiteral("text_from"), QStringLiteral("ElementInfo"));
		label.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
		label.setAttribute(QStringLiteral("font"), body_font.toString());
		label.appendChild(document.createElement(QStringLiteral("text")));
		QDomElement info = document.createElement(QStringLiteral("info_name"));
		info.appendChild(document.createTextNode(QStringLiteral("label")));
		label.appendChild(info);
		description.appendChild(label);
	}

	return document;
}
