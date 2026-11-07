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
#ifndef IOMAP_H
#define IOMAP_H

#include <QList>
#include <QString>

class Diagram;
class QETProject;

/**
	@brief The IoMap namespace
	The input/output maps of a project: for each PLC (a master element of
	kind PLC), the table of its inputs beside the table of its outputs,
	one row per input or output of its IO table with the pins, the
	address, where the contact linked to it is drawn and its function.
	The inputs or outputs no contact uses are listed too, as spare.
	The maps are drawn on folios of their own (SheetTable), that
	generating them again replaces.
*/
namespace IoMap
{
	struct Io
	{
		QString pins;
		QString address;
		QString location;	///< folio.rowcolumn of the linked contacts
		QString description;
	};

	struct Module
	{
		QString label;
		QString description;
		QList<Io> inputs;
		QList<Io> outputs;
	};

	QList<Module> collect(QETProject *project);
	QList<Diagram *> generatedFolios(QETProject *project);
	int createFolios(QETProject *project, const QList<Module> &modules);
}

#endif // IOMAP_H
