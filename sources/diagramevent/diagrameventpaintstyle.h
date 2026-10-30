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
#ifndef DIAGRAMEVENTPAINTSTYLE_H
#define DIAGRAMEVENTPAINTSTYLE_H

#include "diagrameventinterface.h"
#include "../stylepainter.h"

class QGraphicsItem;
class QPointF;

/**
	@brief The DiagramEventPaintStyle class
	The format painter tool: a click on an item of the same kind as the
	captured one gives it that look (StylePainter). The tool ends after one
	click, or keeps going while Ctrl is held; Escape or a right click ends
	it.
*/
class DiagramEventPaintStyle : public DiagramEventInterface
{
	Q_OBJECT

	public:
		DiagramEventPaintStyle(Diagram *diagram, const StylePainter::Source &source);
		~DiagramEventPaintStyle() override;

		void mousePressEvent       (QGraphicsSceneMouseEvent *event) override;
		void mouseMoveEvent        (QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent     (QGraphicsSceneMouseEvent *event) override;
		void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event) override;

	private:
		QGraphicsItem *targetAt(const QPointF &pos) const;
		void setCursor(Qt::CursorShape shape) const;
		void showHint() const;

		StylePainter::Source m_source;
};

#endif // DIAGRAMEVENTPAINTSTYLE_H
