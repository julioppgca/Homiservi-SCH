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
#ifndef WIREJUNCTION_H
#define WIREJUNCTION_H

#include <QList>
#include <QPointF>

class Conductor;
class Diagram;
class Element;
class ElementsLocation;
class QETProject;
class QUndoCommand;
class Terminal;

/**
	@brief The WireJunction namespace
	Homiservi-SCH: the junction point of wires, the dot where a wire
	branches. It is a tiny element of the project (a thumbnail, so it is
	no part of the parts list) with one terminal that takes any number of
	wires.

	The junction is a drawing convention, not a device: in the panel a
	wire goes from terminal to terminal. So the wiring list replaces the
	wires of a junction by the wires between the terminals it joins
	(ends()), in order; the end labels and the wiring check see through
	it the same way (partner()).
*/
namespace WireJunction
{
	bool isJunction(const Element *element);
	bool isJunction(const Terminal *terminal);

		///The junction definition, embedded in project when missing
	ElementsLocation location(QETProject *project);
		///A new junction whose terminal is at point, not on the folio yet
	Element *create(Diagram *diagram, const QPointF &point);

	struct End
	{
		Terminal *terminal = nullptr;
			///The wire from the junction(s) to terminal
		Conductor *conductor = nullptr;
	};
		///The terminals joined through the junction of junction_terminal
		///(and the junctions wired to it), by folio then top to bottom,
		///left to right. The wires of the panel join them one after the
		///other.
	QList<End> ends(Terminal *junction_terminal, QList<Element *> *junctions = nullptr);
		///The terminal the end of conductor away from end1's end goes to,
		///through a junction: the next of the ends() of that junction
	Terminal *partner(Conductor *conductor, bool end1);

		///point projected on the path of conductor, snapped to the grid
		///along its segment; false if point is farther than tolerance
	bool pointOnConductor(Conductor *conductor, const QPointF &point,
			      QPointF *on, qreal tolerance = 3.0);
		///Replace conductor by two wires through a new junction at point
		///(on its path), as children of parent. The two wires keep the
		///properties, the path and the end labels of conductor.
		///@return the terminal of the junction
	Terminal *split(Conductor *conductor, const QPointF &point, QUndoCommand *parent);
}

#endif // WIREJUNCTION_H
