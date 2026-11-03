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
#ifndef CABLEDATA_H
#define CABLEDATA_H

#include <QDomElement>
#include <QPainterPath>
#include <QPolygonF>
#include <QString>
#include <QVariantMap>

/**
	@brief The CableData struct
	What makes a polyline shape a cable: the thick line the wires of a
	multicore cable run into, drawn the way machine builders draw them,
	each wire entering at 45 degrees. A wire belongs to the cable whose
	name is the one written in its "cable" property, so the wiring list
	and the exports, which already read that property, group the wires
	of a cable together without knowing about this item at all.
*/
struct CableData
{
		///Where the shield of the cable is grounded
	enum ShieldGround {GroundNone = 0, GroundStart = 1, GroundEnd = 2, GroundBoth = 3};

	bool enabled = false;
	QString name;
	QString code;		///< Manufacturer reference, A02B-0124-K820 for instance
	QString formation;	///< 10 x 2 x 0,18 mm² for instance
	QString length;
	int cores = 0;		///< 0: not given, no check of the wires against it
	bool shielded = false;
	int shield_ground = GroundStart;

	bool operator==(const CableData &other) const {
		return enabled == other.enabled && name == other.name && code == other.code
			&& formation == other.formation && length == other.length
			&& cores == other.cores && shielded == other.shielded
			&& shield_ground == other.shield_ground;
	}
	bool operator!=(const CableData &other) const {return !(*this == other);}

		///The text written on the cable
	QString label() const {
		return formation.isEmpty() ? name : name + QStringLiteral(" – ") + formation;
	}

	QVariantMap toMap() const {
		QVariantMap map;
		map.insert(QStringLiteral("enabled"), enabled);
		map.insert(QStringLiteral("name"), name);
		map.insert(QStringLiteral("code"), code);
		map.insert(QStringLiteral("formation"), formation);
		map.insert(QStringLiteral("length"), length);
		map.insert(QStringLiteral("cores"), cores);
		map.insert(QStringLiteral("shielded"), shielded);
		map.insert(QStringLiteral("shield_ground"), shield_ground);
		return map;
	}

	static CableData fromMap(const QVariantMap &map) {
		CableData data;
		data.enabled = map.value(QStringLiteral("enabled")).toBool();
		data.name = map.value(QStringLiteral("name")).toString();
		data.code = map.value(QStringLiteral("code")).toString();
		data.formation = map.value(QStringLiteral("formation")).toString();
		data.length = map.value(QStringLiteral("length")).toString();
		data.cores = map.value(QStringLiteral("cores")).toInt();
		data.shielded = map.value(QStringLiteral("shielded")).toBool();
		data.shield_ground = map.value(QStringLiteral("shield_ground"), int(GroundStart)).toInt();
		return data;
	}

	QDomElement toXml(QDomDocument &document) const {
		QDomElement e = document.createElement(QStringLiteral("cable"));
		e.setAttribute(QStringLiteral("name"), name);
		e.setAttribute(QStringLiteral("code"), code);
		e.setAttribute(QStringLiteral("formation"), formation);
		e.setAttribute(QStringLiteral("length"), length);
		e.setAttribute(QStringLiteral("cores"), cores);
		e.setAttribute(QStringLiteral("shielded"), shielded ? 1 : 0);
		e.setAttribute(QStringLiteral("shield_ground"), shield_ground);
		return e;
	}

		///A shape without a <cable> child is not a cable
	static CableData fromXml(const QDomElement &shape) {
		CableData data;
		const QDomElement e = shape.firstChildElement(QStringLiteral("cable"));
		if (e.isNull())
			return data;
		data.enabled = true;
		data.name = e.attribute(QStringLiteral("name"));
		data.code = e.attribute(QStringLiteral("code"));
		data.formation = e.attribute(QStringLiteral("formation"));
		data.length = e.attribute(QStringLiteral("length"));
		data.cores = e.attribute(QStringLiteral("cores"), QStringLiteral("0")).toInt();
		data.shielded = e.attribute(QStringLiteral("shielded"), QStringLiteral("0")).toInt();
		data.shield_ground = e.attribute(QStringLiteral("shield_ground"), QStringLiteral("1")).toInt();
		return data;
	}
};

namespace CableGeometry
{
		///Where a wire meets the cable: the nearest point of the cable line,
		///the direction of the cable there, and how far along the cable it is
	struct Projection {
		QPointF point;
		QPointF direction;
		qreal along = 0;
		bool valid = false;
	};

	Projection project(const QPolygonF &line, const QPointF &p);

		///The path of a wire from its terminal to the cable: straight out of
		///the terminal, then 45 degrees into the cable, turned towards the
		///other end of the wire. \a out is the direction the terminal faces,
		///\a toward_along where along the cable the wire goes next.
		///\a text_pos and \a vertical give where the wire number fits.
	QPainterPath stub(const QPolygonF &line, const QPointF &terminal, const QPointF &out,
			  qreal toward_along, QPointF *text_pos = nullptr, bool *vertical = nullptr);
}

#endif // CABLEDATA_H
