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
#ifndef CONDUCTORENDLABELING_H
#define CONDUCTORENDLABELING_H

#include "potentialrules.h"

#include <QString>

class Conductor;
class Terminal;

/**
	@brief The ConductorEndLabeling namespace
	Computes and applies the wire end (ferrule) labels. What a label says
	is a rule of the project (PotentialRules::Settings::end_labels): by
	default the terminal of that end itself (the end plugged into -D1's
	pin L2 is tagged "-D1:L2"), the convention of the ferrules printed for
	the workshop: whoever holds a loose end knows where it goes. The other
	rules: the terminal at the other end (EPLAN "Ziel"), the number of the
	wire, or the number and the terminal.

	The labels drawn on the folios, the printed tag list and the label
	printer file all come from here (see WiringList), so they say the
	same thing.
*/
namespace ConductorEndLabeling
{
	QString terminalCrossRefId(Terminal *terminal);

		///The rule of the project of conductor
	PotentialRules::EndLabels rule(Conductor *conductor);
		///The label of the end of conductor at terminal1 (end1) or at
		///terminal2, by rule
	QString endLabel(Conductor *conductor, bool end1, PotentialRules::EndLabels rule);
		///The label of that end as drawn on the folio, else as rule gives it
	QString shownLabel(Conductor *conductor, bool end1);

	/**
		Compute and apply both end labels of @p conductor, pushing one
		ChangeDiagramTextCommand per changed end onto the conductor's
		diagram undo stack. Callers doing this for several conductors at
		once should wrap the calls in undoStack().beginMacro()/endMacro()
		so the whole batch becomes a single undo step.
	*/
	void applyToConductor(Conductor *conductor);
		///true if applyToConductor() would change a label of conductor
	bool needsLabels(Conductor *conductor);
}

#endif
