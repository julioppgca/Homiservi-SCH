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

#include <QString>

class Conductor;
class Terminal;

/**
	@brief The ConductorEndLabeling namespace
	Computes and applies wire end (ferrule) marking: each end of a conductor
	is labeled with the identifier ("-<element label><pin name>") of *that
	same end's own terminal* - e.g. the wire end plugged into -D1's pin L2 is
	tagged "-D1L2", and the other end, plugged into -K1's pin A1, is tagged
	"-K1A1". This is the standard convention for physically printed
	ferrules/wire-end labels: it tells whoever is holding one loose end
	where that end belongs, once the two ends are no longer next to each
	other on a diagram.
*/
namespace ConductorEndLabeling
{
	QString terminalCrossRefId(Terminal *terminal);

	/**
		Compute and apply both end labels of @p conductor, pushing one
		ChangeDiagramTextCommand per changed end onto the conductor's
		diagram undo stack. Callers doing this for several conductors at
		once should wrap the calls in undoStack().beginMacro()/endMacro()
		so the whole batch becomes a single undo step.
	*/
	void applyToConductor(Conductor *conductor);
}

#endif
