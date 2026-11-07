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
#include "eseventmeasure.h"

#include "../elementscene.h"
#include "../ui/qetelementeditor.h"

#include <QCoreApplication>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QPen>
#include <QStatusBar>
#include <QtMath>

ESEventMeasure::ESEventMeasure(ElementScene *scene) :
	ESEventInterface(scene)
{
	m_running = true;
	m_editor->statusBar()->showMessage(QCoreApplication::translate("ESEventMeasure",
		"Medir: clique no primeiro ponto e depois no segundo. Botão direito ou Esc termina."));
}

ESEventMeasure::~ESEventMeasure()
{
	delete m_line;
	delete m_label;
	delete m_label_back;
	m_scene->clearSnapMarker();
	m_editor->UncheckAddPrimitive();
}

/**
	@return "L = 42.43   Δx = 30   Δy = -30   45°": the distance from
	from to to, the gaps (y down, as in the editor) and the angle
	counterclockwise from the right
*/
QString ESEventMeasure::describe(const QPointF &from, const QPointF &to)
{
	const QPointF d = to - from;
	const qreal angle = qRadiansToDegrees(qAtan2(-d.y(), d.x()));
	auto value = [](qreal v) { return QString::number(qRound(v * 100) / 100.0); };
	return QCoreApplication::translate("ESEventMeasure", "L = %1   Δx = %2   Δy = %3   %4°")
			.arg(value(qSqrt(d.x() * d.x() + d.y() * d.y())), value(d.x()), value(d.y()), value(angle));
}

bool ESEventMeasure::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::RightButton) {
		m_running = false;
		return true;
	}
	if (event->button() != Qt::LeftButton)
		return false;

	const QPointF pos = snappedPos(event);
	if (!m_measuring) {
		m_from = pos;
		m_measuring = true;
		show(pos);
	} else {
			//The measure stays shown until the next click
		show(pos);
		m_measuring = false;
	}
	return true;
}

bool ESEventMeasure::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	const QPointF pos = snappedPos(event);
	updateHelpCross(pos);
	if (m_measuring)
		show(pos);
	return true;
}

bool ESEventMeasure::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	return event->button() == Qt::LeftButton || event->button() == Qt::RightButton;
}

/**
	@brief ESEventMeasure::show
	The line from the first point to to, its measure next to it
*/
void ESEventMeasure::show(const QPointF &to)
{
	if (!m_line) {
		QPen pen(QColor(0, 74, 158));
		pen.setCosmetic(true);
		pen.setWidthF(1.5);
		pen.setStyle(Qt::DashLine);
		m_line = new QGraphicsLineItem;
		m_line->setPen(pen);
		m_line->setZValue(1000000);
		m_scene->addItem(m_line);

		m_label_back = new QGraphicsRectItem;
		m_label_back->setBrush(QColor(255, 255, 255, 220));
		m_label_back->setPen(Qt::NoPen);
		m_label_back->setZValue(1000001);
		m_label_back->setFlag(QGraphicsItem::ItemIgnoresTransformations);
		m_scene->addItem(m_label_back);

		m_label = new QGraphicsSimpleTextItem(m_label_back);
		m_label->setBrush(QColor(0, 74, 158));
	}
	m_line->setLine(QLineF(m_from, to));
	const QString text = describe(m_from, to);
	m_label->setText(text);
	const QRectF r = m_label->boundingRect().adjusted(-3, -2, 3, 2);
	m_label_back->setRect(r);
	m_label_back->setPos((m_from + to) / 2 + QPointF(4, 4));
	m_editor->statusBar()->showMessage(text);
}
