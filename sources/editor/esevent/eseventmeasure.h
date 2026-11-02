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
#ifndef ESEVENTMEASURE_H
#define ESEVENTMEASURE_H

#include "eseventinterface.h"

#include <QPointF>

class QGraphicsLineItem;
class QGraphicsSimpleTextItem;
class QGraphicsRectItem;

/**
	@brief The ESEventMeasure class
	Measure in the element editor: click a point, then another (both
	snapped to the key points of the parts and the grid), and the
	distance, the horizontal and vertical gaps and the angle show on the
	scene and in the status bar. Nothing is added to the element. A new
	click starts a new measure; right click or Escape ends the tool.
*/
class ESEventMeasure : public ESEventInterface
{
	public:
		ESEventMeasure(ElementScene *scene);
		~ESEventMeasure() override;

		bool mousePressEvent   (QGraphicsSceneMouseEvent *event) override;
		bool mouseMoveEvent    (QGraphicsSceneMouseEvent *event) override;
		bool mouseReleaseEvent (QGraphicsSceneMouseEvent *event) override;

		static QString describe(const QPointF &from, const QPointF &to);

	private:
		void show(const QPointF &to);

		QPointF m_from;
		bool m_measuring = false;
		QGraphicsLineItem *m_line = nullptr;
		QGraphicsRectItem *m_label_back = nullptr;
		QGraphicsSimpleTextItem *m_label = nullptr;
};

#endif // ESEVENTMEASURE_H
