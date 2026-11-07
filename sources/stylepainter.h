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
#ifndef STYLEPAINTER_H
#define STYLEPAINTER_H

#include "conductorproperties.h"
#include "ElementsCollection/elementslocation.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QPen>
#include <QPointF>
#include <QString>

class QGraphicsItem;
class QUndoCommand;
class QETProject;

/**
	@brief The StylePainter namespace
	The format painter of the folios: capture() takes the look of one item,
	paint() gives it to another of the same kind, as one undo step.
	- text: font, colour, text style and frame;
	- element: the look of each of its texts, matched by what they show
	  (the same information, the same composite or fixed text), and their
	  place when both elements come from the same definition;
	- conductor: colour, section, line, size and texts, on the whole
	  potential, but not its number, function or voltage;
	- shape: line and filling.
*/
namespace StylePainter
{
	enum Kind { Nothing, Text, Element, Conductor, Shape };

	struct TextLook
	{
			///What the text shows, to match it with a text of another element
		QString key;
		QString style;
		QFont font;
		QColor color;
		bool has_frame = false;
		bool frame = false;
		bool has_place = false;
		QPointF pos;
		qreal rotation = 0;
	};

	struct Source
	{
		Kind kind = Nothing;
		TextLook text;
		QList<TextLook> element_texts;
		ElementsLocation location;
		ConductorProperties conductor;
		QPen pen;
		QBrush brush;
	};

	Source capture(const QList<QGraphicsItem *> &selection);
	QString kindName(Kind kind);
	QGraphicsItem *target(const Source &source, QGraphicsItem *item);
	QUndoCommand *paint(const Source &source, QGraphicsItem *target, QETProject *project);
}

#endif // STYLEPAINTER_H
