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
#include "projectnumbering.h"
#include "qetproject.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"

/**
	@param terminal a terminal belonging to some element on the diagram
	@return the "target designation" identifier for @p terminal, EPLAN
	"Ziel" style, e.g. a terminal named "A1" on element labeled "K1"
	returns "-K1:A1" (IEC 81346). If @p terminal belongs to a report/off-page connector
	("renvoi") element, its own label is meaningless as a target (it's not
	a real device), so the folio its linked counterpart lives on is used
	instead, e.g. "→F02.04": where the wire goes on.
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

		//IEC 81346: the device, then its terminal after a colon (-K1:A1).
		//A label already starting with its prefix (-, +, =) keeps it.
	QString label = element->actualLabel().trimmed();
	if (!label.isEmpty() && !label.startsWith(QLatin1Char('-'))
	    && !label.startsWith(QLatin1Char('+')) && !label.startsWith(QLatin1Char('=')))
		label.prepend(QLatin1Char('-'));
	QString name = terminal->name().trimmed();
	if (name == QLatin1String("_")) name.clear();
	if (label.isEmpty()) return name;
	return name.isEmpty() ? label : label + QLatin1Char(':') + name;
}

PotentialRules::EndLabels ConductorEndLabeling::rule(Conductor *conductor)
{
	Diagram *diagram = conductor ? conductor->diagram() : nullptr;
	QETProject *project = diagram ? diagram->project() : nullptr;
	return project ? project->potentialRules().end_labels : PotentialRules::OwnTerminal;
}

/**
	@brief ConductorEndLabeling::endLabel
	@return the label of the end of conductor at terminal1 if end1, else at
	terminal2, as rule says. At the end plugged into a folio report, the
	voltage of the wire when it has one: the report already shows where
	the wire goes on.
*/
QString ConductorEndLabeling::endLabel(Conductor *conductor, bool end1, PotentialRules::EndLabels rule)
{
	if (!conductor) return QString();
	Terminal *own = end1 ? conductor->terminal1 : conductor->terminal2;
	Terminal *other = end1 ? conductor->terminal2 : conductor->terminal1;

	const QString tension = conductor->properties().m_tension_protocol.trimmed();
	if (!tension.isEmpty() && own && own->parentElement()
	    && (own->parentElement()->linkType() & Element::AllReport))
		return tension;

	QString number = conductor->properties().text.trimmed();
	if (ProjectNumbering::isNoWireNumber(number)) number.clear();

	switch (rule) {
		case PotentialRules::Destination:
			return terminalCrossRefId(other);
		case PotentialRules::WireNumber:
			return number;
		case PotentialRules::NumberAndTerminal: {
			const QString terminal = terminalCrossRefId(own);
			if (number.isEmpty()) return terminal;
			return terminal.isEmpty() ? number : number + QLatin1Char(' ') + terminal;
		}
		case PotentialRules::OwnTerminal:
		default:
			return terminalCrossRefId(own);
	}
}

/**
	@return the label of the end of conductor at terminal1 (end1) or
	terminal2 as drawn on its folio, edited or not; when the folio shows
	none, as the rule of the project gives it
*/
QString ConductorEndLabeling::shownLabel(Conductor *conductor, bool end1)
{
	if (!conductor) return QString();
	ConductorTextItem *item = end1 ? conductor->textItemEnd1() : conductor->textItemEnd2();
	const QString shown = item ? item->toPlainText().trimmed() : QString();
	return shown.isEmpty() ? endLabel(conductor, end1, rule(conductor)) : shown;
}

bool ConductorEndLabeling::needsLabels(Conductor *conductor)
{
	if (!conductor || !conductor->isValid() || !conductor->diagram()) return false;
	const PotentialRules::EndLabels end_rule = rule(conductor);
	ConductorTextItem *end1 = conductor->textItemEnd1();
	ConductorTextItem *end2 = conductor->textItemEnd2();
	return (end1 && end1->toPlainText() != endLabel(conductor, true, end_rule))
		|| (end2 && end2->toPlainText() != endLabel(conductor, false, end_rule));
}

/**
	@param conductor the conductor to (re)label
	Gives both ends of conductor their label by the rule of its project
	(endLabel()). Only pushes an undo command for an end whose text
	actually changes.
*/
void ConductorEndLabeling::applyToConductor(Conductor *conductor)
{
	if (!conductor || !conductor->isValid())
		return;

	Diagram *diagram = conductor->diagram();
	if (!diagram)
		return;

	const PotentialRules::EndLabels end_rule = rule(conductor);
	const QString label_end1 = endLabel(conductor, true, end_rule);
	const QString label_end2 = endLabel(conductor, false, end_rule);

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
