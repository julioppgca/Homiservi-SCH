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
#include <QMap>
#include <QPointF>
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

	void addLine(QDomElement &description, qreal x1, qreal y1, qreal x2, qreal y2,
		     const char *style = line_style)
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
		e.setAttribute(QStringLiteral("style"), QString::fromLatin1(style));
		e.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
		description.appendChild(e);
	}

	void addRect(QDomElement &description, qreal x, qreal y, qreal width, qreal height)
	{
		QDomElement e = description.ownerDocument().createElement(QStringLiteral("rect"));
		e.setAttribute(QStringLiteral("x"), x);
		e.setAttribute(QStringLiteral("y"), y);
		e.setAttribute(QStringLiteral("width"), width);
		e.setAttribute(QStringLiteral("height"), height);
		e.setAttribute(QStringLiteral("rx"), 0);
		e.setAttribute(QStringLiteral("ry"), 0);
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
	if (!pins.names.isEmpty())
		return pins.names;
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
	10 px grid. The symbols are drawn between the two pins at their place;
	the body text goes in the widest stretch of the box without symbols.
	@return an element definition holding the parts only
*/
QDomDocument ModuleGenerator::build(const Settings &settings)
{
	const int pitch = std::max(10, ceilToGrid(settings.pitch));
	QStringList names[SideCount];
	QList<int> places[SideCount];
	for (int side = 0; side < SideCount; ++side) {
		names[side] = pinNames(settings.sides[side]);
		const QList<int> &given = settings.sides[side].places;
		for (int i = 0; i < names[side].size(); ++i)
			places[side] << (i < given.size() ? given.at(i) : i);
	}
	auto place_count = [&](Side a, Side b) {
		int count = 0;
		for (Side side : {a, b})
			for (int place : places[side])
				count = std::max(count, place + 1);
		return count;
	};

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

	bool vertical_symbols = false, horizontal_symbols = false;
	for (const Symbol &symbol : settings.symbols)
		(symbol.vertical ? vertical_symbols : horizontal_symbols) = true;
		//Room along a symbol: the symbol itself and a lead on each end
	constexpr int symbol_room = 36;

	const int vertical_pins = place_count(Left, Right);
	const int horizontal_pins = place_count(Top, Bottom);
	const int width = std::max({(horizontal_pins + 1) * pitch,
				    ceilToGrid(names_size(Left) + names_size(Right) + body_width),
				    horizontal_symbols ? ceilToGrid(names_size(Left) + names_size(Right) + symbol_room) : 0,
				    40});
	const int height = std::max({(vertical_pins + 1) * pitch,
				     ceilToGrid(names_size(Top) + names_size(Bottom) + body_height),
				     vertical_symbols ? ceilToGrid(names_size(Top) + names_size(Bottom) + symbol_room) : 0,
				     40});

	QDomDocument document;
	QDomElement definition = document.createElement(QStringLiteral("definition"));
	definition.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
	document.appendChild(definition);
	QDomElement description = document.createElement(QStringLiteral("description"));
	definition.appendChild(description);

	addRect(description, 0, 0, width, height);

		//Centre of a name across the pin line: its baseline sits half the
		//cap height past the pin
	const qreal half_cap = pin_metrics.capHeight() / 2;

	for (int i = 0; i < names[Left].size(); ++i) {
		const int y = (places[Left].at(i) + 1) * pitch;
		addLine(description, 0, y, -lead, y);
		addTerminal(description, names[Left].at(i), -lead, y, "w");
		if (settings.show_names)
			addText(description, names[Left].at(i), margin, y + half_cap, 0, pin_font);
	}
	for (int i = 0; i < names[Right].size(); ++i) {
		const int y = (places[Right].at(i) + 1) * pitch;
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
		const int x = (places[Top].at(i) + 1) * pitch;
		addLine(description, x, 0, x, -lead);
		addTerminal(description, names[Top].at(i), x, -lead, "n");
		if (settings.show_names)
			addText(description, names[Top].at(i), x + half_cap,
				margin + pin_metrics.horizontalAdvance(names[Top].at(i)), 270, pin_font);
	}
	for (int i = 0; i < names[Bottom].size(); ++i) {
		const int x = (places[Bottom].at(i) + 1) * pitch;
		addLine(description, x, height, x, height + lead);
		addTerminal(description, names[Bottom].at(i), x, height + lead, "s");
		if (settings.show_names)
			addText(description, names[Bottom].at(i), x + half_cap, height - margin, 270, pin_font);
	}

		//Symbols. A vertical one runs down its column from the top edge
		//to the bottom one, its middle half way between the names; a
		//horizontal one runs along its row, both clear of the pin names.
		//along is the position on that run, across the offset from it, to
		//the right of a vertical one and below a horizontal one.
	const qreal middle_y = std::round((names_size(Top) + height - names_size(Bottom)) / 2);
	const qreal middle_x = std::round((names_size(Left) + width - names_size(Right)) / 2);
		//Contacts of one device, linked by a dashed line: label to the
		//middles of their blades
	QMap<QString, QList<QPointF>> links;
	for (const Symbol &symbol : settings.symbols) {
		const qreal line = (symbol.place + 1) * pitch;
		const qreal mid = symbol.vertical ? middle_y : middle_x;
		const qreal start = symbol.vertical ? names_size(Top) : names_size(Left);
		const qreal end = symbol.vertical ? height - names_size(Bottom) : width - names_size(Right);
		auto point = [&](qreal along, qreal across) {
			return symbol.vertical ? QPointF(line + across, along) : QPointF(along, line + across);
		};
		auto stroke = [&](qreal along1, qreal across1, qreal along2, qreal across2) {
			const QPointF a = point(along1, across1), b = point(along2, across2);
			addLine(description, a.x(), a.y(), b.x(), b.y());
		};
		qreal label_across = 6;
		switch (symbol.kind) {
			case Coil: {
				stroke(start, 0, mid - 5, 0);
				stroke(mid + 5, 0, end, 0);
				const QPointF corner = point(mid - 5, -7);
				addRect(description, corner.x(), corner.y(),
					symbol.vertical ? 14 : 10, symbol.vertical ? 10 : 14);
				label_across = 10;
				break;
			}
			case NormallyClosed:
				stroke(start, 0, mid - 6, 0);
				stroke(mid - 6, 0, mid - 6, 6);
				stroke(mid + 6, 0, mid - 8, 8);
				stroke(mid + 6, 0, end, 0);
				links[symbol.label] << point(mid - 1, 4);
				label_across = -5;
				break;
			default:
				stroke(start, 0, mid - 6, 0);
				stroke(mid + 6, 0, mid - 5, -6);
				stroke(mid + 6, 0, end, 0);
				links[symbol.label] << point(mid, -3);
				break;
		}
		if (!symbol.label.isEmpty()) {
			const qreal text_width = pin_metrics.horizontalAdvance(symbol.label);
			if (symbol.vertical) {
				const qreal x = label_across > 0 ? line + label_across : line + label_across - text_width;
				addText(description, symbol.label, x, mid + half_cap + 5, 0, pin_font);
			} else {
				addText(description, symbol.label, mid - text_width / 2,
					line + (label_across > 0 ? label_across + 2 * half_cap : label_across), 0, pin_font);
			}
		}
	}
	for (auto it = links.cbegin(); it != links.cend(); ++it) {
		if (it.key().isEmpty() || it.value().size() < 2)
			continue;
		QPointF first = it.value().first(), last = first;
		for (const QPointF &p : it.value()) {
			if (p.x() + p.y() < first.x() + first.y()) first = p;
			if (p.x() + p.y() > last.x() + last.y()) last = p;
		}
		addLine(description, first.x(), first.y(), last.x(), last.y(),
			"line-style:dashed;line-weight:thin;filling:none;color:black");
	}

	if (!settings.body_text.isEmpty()) {
		qreal left = names_size(Left);
		qreal right = width - names_size(Right);
		const qreal top = names_size(Top);
		const qreal bottom = height - names_size(Bottom);
			//Keep the text clear of the vertical symbols: the widest run of
			//columns without one
		if (vertical_symbols) {
			QList<bool> used(horizontal_pins + 1, false);
			for (const Symbol &symbol : settings.symbols)
				if (symbol.vertical && symbol.place < used.size())
					used[symbol.place] = true;
			int best_start = 0, best_length = 0;
			for (int start = 0; start < used.size(); ) {
				int length = 0;
				while (start + length < used.size() && !used.at(start + length))
					++length;
				if (length > best_length) {
					best_start = start;
					best_length = length;
				}
				start += length + 1;
			}
			if (best_length > 0) {
				left = std::max(left, best_start * pitch + pitch / 2.0);
				right = std::min(right, (best_start + best_length) * pitch + pitch / 2.0);
			}
		}
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
