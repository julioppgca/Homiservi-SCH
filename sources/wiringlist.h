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
#ifndef WIRINGLIST_H
#define WIRINGLIST_H

#include <QList>
#include <QString>

class Conductor;
class Terminal;
class Diagram;
class QETProject;
class QWidget;

/**
	@brief The WiringList namespace
	Homiservi-SCH: the wires of a project as one list, read from the
	drawing as it is now: folio, number, the two ends, their labels and
	the wire properties. The wiring editor, the wiring list CSV, the
	printed tag list and the label printer file all read it, so they show
	the same wires with the same columns.
*/
namespace WiringList
{
	struct Wire
	{
		Conductor *conductor = nullptr;
			///Number of the folio as the title block shows it: 02.04
		QString folio;
		int folio_index = 0;
		QString number;
			///-K1:A1
		QString from;
		QString to;
			///The labels of the ends at from and at to (ConductorEndLabeling)
		QString from_label;
		QString to_label;
		QString function;
		QString color;
		QString section;
		QString cable;
		QString voltage;
		QString bus;
	};

	QString folioLabel(Diagram *diagram);
	QString terminalText(Conductor *conductor, bool end1);
	QString terminalText(Terminal *terminal);
	Wire wire(Conductor *conductor);
	QList<Wire> wires(QETProject *project);

	QStringList csvHeader();
	QString toCsv(const QList<Wire> &wires);
	QString tagListHtml(const QString &title, const QList<Wire> &wires);
	QString tagListCsv(const QList<Wire> &wires);

		//With a dialog
	bool saveCsv(QWidget *parent, QETProject *project, const QList<Wire> &wires);
	void printTagList(QWidget *parent, QETProject *project);
	bool saveTagListCsv(QWidget *parent, QETProject *project);
}

#endif // WIRINGLIST_H
