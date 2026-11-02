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
#ifndef ELEMENTALIGN_H
#define ELEMENTALIGN_H

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

class ElementScene;
class QGraphicsItem;
class QUndoCommand;

/**
	@brief The ElementAlign namespace
	Align and distribute the parts of the element editor, and move the
	selection as a whole. A part counts by its drawing (the ends of a
	line, the box of a rectangle...), not by the width of its pen; a
	terminal by its connection point; a text by its box.
*/
namespace ElementAlign
{
	enum Mode {
		Left, HCenter, Right,
		Top, VCenter, Bottom,
		DistributeH, DistributeV
	};

	QString modeName(Mode mode);
	QRectF geometry(const ElementScene *scene, const QGraphicsItem *item);
	QRectF geometry(const ElementScene *scene, const QList<QGraphicsItem *> &items);
	QUndoCommand *arrange(ElementScene *scene, const QList<QGraphicsItem *> &items, Mode mode);
	QUndoCommand *translate(const QList<QGraphicsItem *> &items, const QPointF &movement,
				const QString &text);
}

#endif // ELEMENTALIGN_H
