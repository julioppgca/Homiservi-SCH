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
#include "elementpreview.h"

#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>

void ElementPreview::draw(QGraphicsScene *scene, const QDomDocument &document, bool terminal_names)
{
	const QDomElement description = document.documentElement().firstChildElement(QStringLiteral("description"));
	for (QDomElement e = description.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
		auto number = [&e](const char *name) { return e.attribute(QString::fromLatin1(name)).toDouble(); };
		const QString style = e.attribute(QStringLiteral("style"));
		QPen pen(Qt::black, 1);
		pen.setCosmetic(true);
		if (style.contains(QLatin1String("dashed")))
			pen.setStyle(Qt::DashLine);
		const QBrush brush = style.contains(QLatin1String("filling:black")) ? QBrush(Qt::black) : QBrush();
		const QString tag = e.tagName();
		if (tag == QLatin1String("line")) {
			scene->addLine(number("x1"), number("y1"), number("x2"), number("y2"), pen);
		} else if (tag == QLatin1String("rect")) {
			QPainterPath path;
			path.addRoundedRect(QRectF(number("x"), number("y"), number("width"), number("height")),
					    number("rx"), number("ry"));
			scene->addPath(path, pen, brush);
		} else if (tag == QLatin1String("circle")) {
			scene->addEllipse(number("x"), number("y"), number("diameter"), number("diameter"), pen, brush);
		} else if (tag == QLatin1String("arc")) {
			QPainterPath path;
			const QRectF rect(number("x"), number("y"), number("width"), number("height"));
			path.arcMoveTo(rect, number("start"));
			path.arcTo(rect, number("start"), number("angle"));
			scene->addPath(path, pen);
		} else if (tag == QLatin1String("polygon")) {
			QPolygonF points;
			for (int i = 1; e.hasAttribute(QStringLiteral("x%1").arg(i)); ++i)
				points << QPointF(e.attribute(QStringLiteral("x%1").arg(i)).toDouble(),
						  e.attribute(QStringLiteral("y%1").arg(i)).toDouble());
			QPainterPath path;
			path.addPolygon(points);
			if (e.attribute(QStringLiteral("closed")) != QLatin1String("false"))
				path.closeSubpath();
			scene->addPath(path, pen, brush);
		} else if (tag == QLatin1String("terminal")) {
			scene->addEllipse(number("x") - 1.5, number("y") - 1.5, 3, 3, QPen(Qt::red, 0.5), QBrush(Qt::red));
			if (terminal_names) {
				auto *name = scene->addSimpleText(e.attribute(QStringLiteral("name")), QFont(QStringLiteral("Sans Serif"), 4));
				name->setBrush(Qt::darkRed);
				name->setPos(number("x") + 2, number("y") - 7);
			}
		} else if (tag == QLatin1String("text")) {
			QFont font;
			font.fromString(e.attribute(QStringLiteral("font")));
			auto *item = scene->addSimpleText(e.attribute(QStringLiteral("text")), font);
				//The part's position is the left end of its baseline
			item->setPos(number("x"), number("y") - QFontMetricsF(font).ascent());
		} else if (tag == QLatin1String("dynamic_text")) {
				//The label field, shown as a sample label
			QFont font(QStringLiteral("Sans Serif"));
			font.setPointSizeF(e.attribute(QStringLiteral("font_size"), QStringLiteral("9")).toDouble());
			auto *item = scene->addSimpleText(QStringLiteral("-X1"), font);
			item->setBrush(Qt::darkBlue);
			item->setPos(number("x"), number("y"));
		}
	}
}
