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
#include "elementalign.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "elementscene.h"
#include "graphicspart/partdynamictextfield.h"
#include "graphicspart/parttext.h"

#include <QCoreApplication>
#include <QGraphicsObject>
#include <QUndoCommand>

#include <algorithm>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("ElementAlign", text);
	}
}

QString ElementAlign::modeName(Mode mode)
{
	switch (mode) {
		case Left:        return tr("Alinhar à esquerda");
		case HCenter:     return tr("Centralizar na horizontal");
		case Right:       return tr("Alinhar à direita");
		case Top:         return tr("Alinhar em cima");
		case VCenter:     return tr("Centralizar na vertical");
		case Bottom:      return tr("Alinhar embaixo");
		case DistributeH: return tr("Distribuir na horizontal");
		case DistributeV: return tr("Distribuir na vertical");
	}
	return QString();
}

/**
	@return the rectangle item is aligned by, in scene coordinates: the
	box of its key points (ElementScene::keyPoints), the box of a text
*/
QRectF ElementAlign::geometry(const ElementScene *scene, const QGraphicsItem *item)
{
	if (item->type() == PartText::Type || item->type() == PartDynamicTextField::Type)
		return item->sceneBoundingRect();
	const QList<QPointF> points = scene->keyPoints(item);
	if (points.isEmpty())
		return item->sceneBoundingRect();
	QPointF top_left = points.first(), bottom_right = points.first();
	for (const QPointF &p : points) {
		top_left = QPointF(qMin(top_left.x(), p.x()), qMin(top_left.y(), p.y()));
		bottom_right = QPointF(qMax(bottom_right.x(), p.x()), qMax(bottom_right.y(), p.y()));
	}
	return QRectF(top_left, bottom_right);
}

QRectF ElementAlign::geometry(const ElementScene *scene, const QList<QGraphicsItem *> &items)
{
	QRectF rect;
	bool first = true;
	for (QGraphicsItem *item : items) {
		const QRectF r = geometry(scene, item);
			//Not united(), which drops an empty rectangle: a terminal is a point
		rect = first ? r : QRectF(QPointF(qMin(rect.left(), r.left()), qMin(rect.top(), r.top())),
					  QPointF(qMax(rect.right(), r.right()), qMax(rect.bottom(), r.bottom())));
		first = false;
	}
	return rect;
}

/**
	@brief ElementAlign::translate
	@return the undo moving items by movement, nullptr when it moves nothing
*/
QUndoCommand *ElementAlign::translate(const QList<QGraphicsItem *> &items, const QPointF &movement,
				      const QString &text)
{
	auto undo = new QUndoCommand(text);
	for (QGraphicsItem *item : items) {
		QGraphicsObject *object = item->toGraphicsObject();
		if (object && !movement.isNull())
			new QPropertyUndoCommand(object, "pos", object->pos(), object->pos() + movement, undo);
	}
	if (undo->childCount())
		return undo;
	delete undo;
	return nullptr;
}

/**
	@brief ElementAlign::arrange
	Align items on the side or centre of the box of them all, or spread
	them (by their centres, the first and last staying) at equal steps.
	@return the undo, nullptr when nothing moves
*/
QUndoCommand *ElementAlign::arrange(ElementScene *scene, const QList<QGraphicsItem *> &items, Mode mode)
{
	if (items.size() < 2 || ((mode == DistributeH || mode == DistributeV) && items.size() < 3))
		return nullptr;

	const QRectF all = geometry(scene, items);
	auto undo = new QUndoCommand(modeName(mode));
	auto move = [undo](QGraphicsItem *item, const QPointF &movement) {
		QGraphicsObject *object = item->toGraphicsObject();
		if (object && (qAbs(movement.x()) > 1e-6 || qAbs(movement.y()) > 1e-6))
			new QPropertyUndoCommand(object, "pos", object->pos(), object->pos() + movement, undo);
	};

	if (mode == DistributeH || mode == DistributeV) {
		const bool horizontal = mode == DistributeH;
		QList<QGraphicsItem *> sorted = items;
		auto centre = [scene, horizontal](QGraphicsItem *item) {
			const QPointF c = geometry(scene, item).center();
			return horizontal ? c.x() : c.y();
		};
		std::stable_sort(sorted.begin(), sorted.end(), [&centre](QGraphicsItem *a, QGraphicsItem *b) {
			return centre(a) < centre(b);
		});
		const qreal first = centre(sorted.first()), last = centre(sorted.last());
		const qreal step = (last - first) / (sorted.size() - 1);
		for (int i = 1; i < sorted.size() - 1; ++i) {
			const qreal shift = first + step * i - centre(sorted.at(i));
			move(sorted.at(i), horizontal ? QPointF(shift, 0) : QPointF(0, shift));
		}
	} else {
		for (QGraphicsItem *item : items) {
			const QRectF r = geometry(scene, item);
			switch (mode) {
				case Left:    move(item, QPointF(all.left() - r.left(), 0)); break;
				case HCenter: move(item, QPointF(all.center().x() - r.center().x(), 0)); break;
				case Right:   move(item, QPointF(all.right() - r.right(), 0)); break;
				case Top:     move(item, QPointF(0, all.top() - r.top())); break;
				case VCenter: move(item, QPointF(0, all.center().y() - r.center().y())); break;
				case Bottom:  move(item, QPointF(0, all.bottom() - r.bottom())); break;
				default: break;
			}
		}
	}
	if (undo->childCount())
		return undo;
	delete undo;
	return nullptr;
}
