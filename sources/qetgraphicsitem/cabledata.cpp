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
#include "cabledata.h"

#include <QLineF>
#include <QtMath>

namespace
{
	qreal dot(const QPointF &a, const QPointF &b) {return a.x() * b.x() + a.y() * b.y();}

		///The length of the 45 degrees entry, one grid step
	const qreal entry = 10;
}

CableGeometry::Projection CableGeometry::project(const QPolygonF &line, const QPointF &p)
{
	Projection best;
	qreal best_distance = 0, along = 0;
	for (int i = 0; i + 1 < line.size(); ++i)
	{
		const QPointF a = line.at(i), b = line.at(i + 1);
		const QPointF ab = b - a;
		const qreal length = QLineF(a, b).length();
		if (length <= 0)
			continue;
		const qreal t = qBound(qreal(0), dot(p - a, ab) / (length * length), qreal(1));
		const QPointF q = a + ab * t;
		const qreal distance = QLineF(p, q).length();
		if (!best.valid || distance < best_distance)
		{
			best.valid = true;
			best.point = q;
			best.direction = ab / length;
			best.along = along + t * length;
			best_distance = distance;
		}
		along += length;
	}
	return best;
}

QPainterPath CableGeometry::stub(const QPolygonF &line, const QPointF &terminal, const QPointF &out,
				 qreal toward_along, QPointF *text_pos, bool *vertical)
{
	QPainterPath path(terminal);
	const Projection projection = project(line, terminal);
	if (!projection.valid)
		return path;

	const qreal distance = dot(projection.point - terminal, out);
	const bool across = qAbs(dot(out, projection.direction)) < 0.2;
	QPointF corner;
	if (across && distance > entry)
	{
			//Straight out of the terminal until one step before the cable,
			//then 45 degrees into it, leaning to where the wire goes
		const qreal side = toward_along >= projection.along ? 1 : -1;
		corner = terminal + out * (distance - entry);
		path.lineTo(corner);
		path.lineTo(corner + out * entry + projection.direction * side * entry);
	}
	else
	{
			//The terminal faces along the cable or behind it: straight in
		corner = projection.point;
		path.lineTo(corner);
	}

	if (text_pos)
		*text_pos = (terminal + corner) / 2;
	if (vertical)
		*vertical = qAbs(out.y()) > qAbs(out.x());
	return path;
}
