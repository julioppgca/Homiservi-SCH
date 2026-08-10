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
	@return the "target designation" identifier for @p terminal, EPLAN
	"Ziel" style, e.g. a terminal named "A1" on element labeled "K1"
	returns "-K1A1". If @p terminal belongs to a report/off-page connector
	("renvoi") element, its own label is meaningless as a target (it's not
	a real device), so the folio its linked counterpart lives on is used
	instead, e.g. "→F2" - genuinely useful here since this function is used
	to describe *the far end* at the other, real-device end of the wire.
	Returns an empty string if the terminal has no parent element
	(shouldn't happen for a terminal that is actually wired).
*/
QString ConductorEndLabeling::terminalCrossRefId(Terminal *terminal)
{
	if (!terminal || !terminal->parentElement())
		return QString();

	Element *element = terminal->parentElement();

	if (element->linkType() & Element::AllReport)
	{
		if (!element->linkedElements().isEmpty())
		{
			Element *counterpart = element->linkedElements().first();
			if (Diagram *target_diagram = qobject_cast<Diagram *>(counterpart->scene()))
			{
				const QString folio = target_diagram->border_and_titleblock.finalfolio();
				if (!folio.isEmpty())
					return QStringLiteral("→F") + folio;
			}
		}
		return QStringLiteral("→?");
	}

	return "-" + element->actualLabel() + terminal->name();
}

/**
	@param conductor the conductor to (re)label
	Sets textItemEnd1() to terminal2's cross-reference id (what's on the
	*other* end, seen from terminal1) and textItemEnd2() to terminal1's id
	(seen from terminal2) — EPLAN "Ziel" (target designation) convention:
	each end shows where the wire *goes*, e.g. the end plugged into -D1/2,
	whose far end is -K1/A1, is tagged "-K1A1". Only pushes an undo command
	for an end whose text actually changes.
*/
void ConductorEndLabeling::applyToConductor(Conductor *conductor)
{
	if (!conductor || !conductor->isValid())
		return;

	Diagram *diagram = conductor->diagram();
	if (!diagram)
		return;

	QString label_end1 = terminalCrossRefId(conductor->terminal2);
	QString label_end2 = terminalCrossRefId(conductor->terminal1);

		// At the renvoi's own end (not the real-device end), the far-side
		// folio pointer would just repeat what the renvoi symbol itself
		// already shows on screen - show the conductor's tension there
		// instead, when the user filled it in (e.g. a 24V supply feed).
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
