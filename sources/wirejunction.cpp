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
#include "wirejunction.h"

#include "ElementsCollection/elementslocation.h"
#include "ElementsCollection/xmlelementcollection.h"
#include "NameList/nameslist.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"
#include "undocommand/addgraphicsobjectcommand.h"
#include "undocommand/deleteqgraphicsitemcommand.h"
#include "diagramcontent.h"
#include "conductorsegment.h"

#include <QDomDocument>
#include <QLineF>
#include <QSet>

#include <algorithm>

namespace {
	const QString file_name = QStringLiteral("homiservi_juncao.elmt");
	const QString dir_name = QStringLiteral("homiservi");

		///A dot on a terminal at 0,0
	const char *definition =
		"<definition version=\"0.100.0\" type=\"element\" link_type=\"thumbnail\""
		" width=\"10\" height=\"10\" hotspot_x=\"5\" hotspot_y=\"5\">"
		"<uuid uuid=\"{6b1f3e2a-5c7d-4e8f-9a0b-1c2d3e4f5a6b}\"/>"
		"<names>"
		"<name lang=\"pt_BR\">Junção de fios</name>"
		"<name lang=\"pt\">Junção de fios</name>"
		"<name lang=\"en\">Wire junction</name>"
		"<name lang=\"fr\">Jonction de conducteurs</name>"
		"<name lang=\"es\">Unión de cables</name>"
		"</names>"
		"<informations>Homiservi-SCH: ponto de junção de fios</informations>"
		"<description>"
		"<ellipse x=\"-2\" y=\"-2\" width=\"4\" height=\"4\""
		" style=\"line-style:normal;line-weight:normal;filling:black;color:black\" antialias=\"true\"/>"
		"<terminal uuid=\"{0d3c5b7a-9e1f-4a2b-8c4d-6e8f0a1b2c3d}\" name=\"\" x=\"0\" y=\"0\""
		" orientation=\"n\" type=\"Generic\"/>"
		"</description>"
		"</definition>";

	QList<QPointF> scenePoints(Conductor *conductor)
	{
		QList<QPointF> points;
		const QList<ConductorSegment *> segments = conductor->segmentsList();
		for (ConductorSegment *segment : segments) {
			if (points.isEmpty()) points << conductor->mapToScene(segment->firstPoint());
			points << conductor->mapToScene(segment->secondPoint());
		}
		return points;
	}

	bool near(const QPointF &a, const QPointF &b)
	{
		return qAbs(a.x() - b.x()) < 0.01 && qAbs(a.y() - b.y()) < 0.01;
	}

		///points without repeated ones nor points inside a straight run
	QList<QPointF> simplified(const QList<QPointF> &points)
	{
		QList<QPointF> result;
		for (const QPointF &point : points) {
			if (!result.isEmpty() && near(result.last(), point)) continue;
			if (result.size() >= 2) {
				const QPointF a = result.at(result.size() - 2);
				const QPointF b = result.last();
				if ((qAbs(a.x() - b.x()) < 0.01 && qAbs(b.x() - point.x()) < 0.01)
				    || (qAbs(a.y() - b.y()) < 0.01 && qAbs(b.y() - point.y()) < 0.01))
					result.removeLast();
			}
			result << point;
		}
		return result;
	}

		///A wire like conductor between a and b, along path
	Conductor *copyOf(Conductor *conductor, Terminal *a, Terminal *b,
			  const QList<QPointF> &path, const QString &label_a,
			  const QString &label_b)
	{
		auto wire = new Conductor(a, b);
		const QList<QPointF> points = simplified(path);
		if (points.size() > 2) wire->setPathPoints(points);
		wire->rSequenceNum() = conductor->sequenceNum();
		wire->setProperties(conductor->properties());
		wire->setFreezeLabel(conductor->isFreezeLabel());
		if (wire->textItemEnd1()) wire->textItemEnd1()->setPlainText(label_a);
		if (wire->textItemEnd2()) wire->textItemEnd2()->setPlainText(label_b);
		return wire;
	}
}

bool WireJunction::isJunction(const Element *element)
{
	return element && element->location().fileName() == file_name;
}

bool WireJunction::isJunction(const Terminal *terminal)
{
	return terminal && isJunction(terminal->parentElement());
}

ElementsLocation WireJunction::location(QETProject *project)
{
	const QString path = QStringLiteral("import/") + dir_name + QLatin1Char('/') + file_name;
	ElementsLocation location(QStringLiteral("embed://") + path, project);
	if (!project || location.exist()) return location;

	XmlElementCollection *collection = project->embeddedElementCollection();
	NamesList names;
	names.addName(QStringLiteral("pt_BR"), QStringLiteral("Homiservi-SCH"));
	names.addName(QStringLiteral("en"), QStringLiteral("Homiservi-SCH"));
	collection->createDir(QStringLiteral("import"), dir_name, names);
	QDomDocument document;
	document.setContent(QByteArray(definition));
	collection->addElementDefinition(QStringLiteral("import/") + dir_name, file_name,
					 document.documentElement());
	return ElementsLocation(QStringLiteral("embed://") + path, project);
}

Element *WireJunction::create(Diagram *diagram, const QPointF &point)
{
	if (!diagram || !diagram->project()) return nullptr;
	const ElementsLocation junction = location(diagram->project());
	int state = 0;
	Element *element = ElementFactory::Instance()->createElement(junction, nullptr, &state);
	if (state || !element) {
		delete element;
		return nullptr;
	}
	element->setPos(point);
	return element;
}

QList<WireJunction::End> WireJunction::ends(Terminal *junction_terminal, QList<Element *> *junctions)
{
	QList<End> result;
	if (!isJunction(junction_terminal)) return result;
	QList<Element *> queue {junction_terminal->parentElement()};
	QSet<Element *> seen {junction_terminal->parentElement()};
	QSet<Conductor *> walked;
	while (!queue.isEmpty()) {
		Element *junction = queue.takeFirst();
		if (junctions) *junctions << junction;
		for (Terminal *terminal : junction->terminals()) {
			for (Conductor *conductor : terminal->conductors()) {
				if (walked.contains(conductor)) continue;
				walked << conductor;
				Terminal *other = conductor->terminal1 == terminal ? conductor->terminal2
										   : conductor->terminal1;
				if (!other) continue;
				if (isJunction(other)) {
					Element *next = other->parentElement();
					if (!seen.contains(next)) {
						seen << next;
						queue << next;
					}
				} else {
					End end;
					end.terminal = other;
					end.conductor = conductor;
					result << end;
				}
			}
		}
	}
	std::sort(result.begin(), result.end(), [](const End &a, const End &b) {
		Diagram *da = a.terminal->diagram();
		Diagram *db = b.terminal->diagram();
		const int fa = da ? da->folioIndex() : 0;
		const int fb = db ? db->folioIndex() : 0;
		if (fa != fb) return fa < fb;
		const QPointF pa = a.terminal->dockConductor();
		const QPointF pb = b.terminal->dockConductor();
		if (qAbs(pa.y() - pb.y()) > 0.01) return pa.y() < pb.y();
		return pa.x() < pb.x();
	});
	return result;
}

Terminal *WireJunction::partner(Conductor *conductor, bool end1)
{
	if (!conductor) return nullptr;
	Terminal *own = end1 ? conductor->terminal1 : conductor->terminal2;
	Terminal *other = end1 ? conductor->terminal2 : conductor->terminal1;
	if (!isJunction(other)) return other;
	const QList<End> list = ends(other);
	for (int i = 0 ; i < list.size() ; ++i) {
		if (list.at(i).terminal != own || list.at(i).conductor != conductor) continue;
		if (i + 1 < list.size()) return list.at(i + 1).terminal;
		if (i > 0) return list.at(i - 1).terminal;
	}
	return nullptr;
}

bool WireJunction::pointOnConductor(Conductor *conductor, const QPointF &point,
				    QPointF *on, qreal tolerance)
{
	if (!conductor) return false;
	const QList<QPointF> points = scenePoints(conductor);
	qreal best = tolerance + 0.01;
	bool found = false;
	for (int i = 1 ; i < points.size() ; ++i) {
		const QPointF a = points.at(i - 1), b = points.at(i);
		const bool horizontal = qAbs(a.y() - b.y()) < 0.01;
		const qreal lo = horizontal ? qMin(a.x(), b.x()) : qMin(a.y(), b.y());
		const qreal hi = horizontal ? qMax(a.x(), b.x()) : qMax(a.y(), b.y());
		if (hi - lo < 0.01) continue;
		const qreal along = horizontal ? point.x() : point.y();
		if (along < lo - tolerance || along > hi + tolerance) continue;
		const qreal d_line = horizontal ? qAbs(point.y() - a.y()) : qAbs(point.x() - a.x());
		if (d_line >= best) continue;
			//On the grid along the segment, and inside it
		const qreal grid = horizontal ? Diagram::xGrid : Diagram::yGrid;
		qreal snapped = qRound(along / grid) * grid;
		if (snapped < lo || snapped > hi) snapped = qBound(lo, along, hi);
		best = d_line;
		if (on) *on = horizontal ? QPointF(snapped, a.y()) : QPointF(a.x(), snapped);
		found = true;
	}
	return found;
}

Terminal *WireJunction::split(Conductor *conductor, const QPointF &point, QUndoCommand *parent)
{
	Diagram *diagram = conductor ? conductor->diagram() : nullptr;
	if (!diagram || !conductor->terminal1 || !conductor->terminal2) return nullptr;
	const QList<QPointF> points = scenePoints(conductor);
	if (points.size() < 2) return nullptr;

		//The segment of point
	int segment = -1;
	for (int i = 1 ; i < points.size() && segment < 0 ; ++i) {
		const QPointF a = points.at(i - 1), b = points.at(i);
		const QRectF box = QRectF(a, b).normalized().adjusted(-0.01, -0.01, 0.01, 0.01);
		if (box.contains(point)) segment = i;
	}
	if (segment < 0) return nullptr;
		//Not on an end: there it would be a terminal of its own
	if (near(point, points.first()) || near(point, points.last())) return nullptr;

	Element *junction = create(diagram, point);
	if (!junction || junction->terminals().isEmpty()) {
		delete junction;
		return nullptr;
	}
	Terminal *joint = junction->terminals().first();

	QList<QPointF> first = points.mid(0, segment);
	first << point;
	QList<QPointF> second {point};
	second << points.mid(segment);

	const QString label1 = conductor->textItemEnd1() ? conductor->textItemEnd1()->toPlainText() : QString();
	const QString label2 = conductor->textItemEnd2() ? conductor->textItemEnd2()->toPlainText() : QString();

	new AddGraphicsObjectCommand(junction, diagram, point, parent);
	DiagramContent content;
	content.m_other_conductors << conductor;
	new DeleteQGraphicsItemCommand(diagram, content, parent);
	Conductor *wire1 = copyOf(conductor, conductor->terminal1, joint, first, label1, QString());
	new AddGraphicsObjectCommand(wire1, diagram, QPointF(), parent);
	Conductor *wire2 = copyOf(conductor, joint, conductor->terminal2, second, QString(), label2);
	new AddGraphicsObjectCommand(wire2, diagram, QPointF(), parent);
	return joint;
}
