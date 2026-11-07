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
#ifndef TERMINALSHEET_H
#define TERMINALSHEET_H

#include <QList>
#include <QString>

class Diagram;
class QETProject;

/**
	@brief The TerminalSheet namespace
	The terminal sheets of a project: one table per terminal strip with,
	for each terminal, its potential (function and voltage of its wires),
	its name, its jumpers, the devices wired to it and where it is drawn.

	The strips are the ones of the terminal strip editor, the terminal
	order and the bridges theirs; the terminal elements in no strip are
	grouped by their label, "XT31:1A" being the terminal 1A of XT31.
	The sheets are drawn on folios of their own, marked so that
	generating them again replaces them.
*/
namespace TerminalSheet
{
	struct Row
	{
		QString potential;
		QString terminal;
		int bridge = -1;	///< terminals with the same bridge are jumpered
		QString devices;
		QString location;	///< folio.rowcolumn
	};

	struct Strip
	{
		QString name;
		QList<Row> rows;
	};

	QList<Strip> collect(QETProject *project);
	QList<Diagram *> generatedFolios(QETProject *project);
	int createFolios(QETProject *project, const QList<Strip> &strips);
}

#endif // TERMINALSHEET_H
