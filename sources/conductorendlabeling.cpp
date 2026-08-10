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
#include "conductorendlabeling.h"

#include "diagram.h"
#include "diagramcommands.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"

/**
	@param terminal a terminal belonging to some element on the diagram
	@return the wire-end identifier for @p terminal, e.g. a terminal named
	"A1" on element labeled "K1" returns "-K1A1". If @p terminal belongs to
	a report/off-page connector ("renvoi") element, its own label is
	meaningless for physical marking (it's not a real device) - and the
	renvoi symbol already shows its own page cross-reference right there,
	so nothing is added here beyond what applyToConductor() appends
	(the conductor's tension, if any). Returns an empty string if the
	terminal has no parent element (shouldn't happen for a terminal that
	is actually wired).
*/
QString ConductorEndLabeling::terminalCrossRefId(Terminal *terminal)
{
	if (!terminal || !terminal->parentElement())
		return QString();

	Element *element = terminal->parentElement();

	if (element->linkType() & Element::AllReport)
		return QString();

	return "-" + element->actualLabel() + terminal->name();
}

/**
	@param conductor the conductor to (re)label
	Sets textItemEnd1() to terminal1's own cross-reference id and
	textItemEnd2() to terminal2's own id — each physical wire end is marked
	with *where that same end plugs in*, e.g. the free end that plugs into
	-K1/A1 is tagged "-K1A1". This is the standard ferrule-marking
	convention: it tells whoever holds that loose wire end where it belongs,
	which is the whole point once the two ends are printed on separate
	ferrules/labels and no longer visually next to each other on a diagram.
	Only pushes an undo command for an end whose text actually changes.
*/
void ConductorEndLabeling::applyToConductor(Conductor *conductor)
{
	if (!conductor || !conductor->isValid())
		return;

	Diagram *diagram = conductor->diagram();
	if (!diagram)
		return;

	QString label_end1 = terminalCrossRefId(conductor->terminal1);
	QString label_end2 = terminalCrossRefId(conductor->terminal2);

		// A wire crossing folios (through a report/renvoi element) loses its
		// local context on the far page, so surface the conductor's tension
		// there when the user filled it in - the renvoi symbol itself
		// already shows the page cross-reference, so this is the only thing
		// added on that end (e.g. a 24V supply feed shows just "24V").
	const QString tension = conductor->properties().m_tension_protocol.trimmed();
	if (!tension.isEmpty())
	{
		if (conductor->terminal1 && conductor->terminal1->parentElement() &&
			(conductor->terminal1->parentElement()->linkType() & Element::AllReport))
			label_end1 = tension;
		if (conductor->terminal2 && conductor->terminal2->parentElement() &&
			(conductor->terminal2->parentElement()->linkType() & Element::AllReport))
			label_end2 = tension;
	}

	if (ConductorTextItem *item = conductor->textItemEnd1())
	{
		const QString before = item->toPlainText();
		if (before != label_end1)
			diagram->undoStack().push(new ChangeDiagramTextCommand(item, before, label_end1));
	}
	if (ConductorTextItem *item = conductor->textItemEnd2())
	{
		const QString before = item->toPlainText();
		if (before != label_end2)
			diagram->undoStack().push(new ChangeDiagramTextCommand(item, before, label_end2));
	}

		// The labels were originally positioned (in Conductor::updatePath(),
		// back when the diagram was drawn) based on their *empty* bounding
		// rect, since no text existed yet at that point. Now that real text
		// was just applied above, recompute the anchor so the label doesn't
		// stay collapsed on the terminal's dock point.
	conductor->calculateEndLabelsPosition();
}
