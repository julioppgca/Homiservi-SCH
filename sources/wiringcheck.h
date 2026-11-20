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
#ifndef WIRINGCHECK_H
#define WIRINGCHECK_H

#include <QList>
#include <QPointer>
#include <QString>

class Conductor;
class QETProject;

/**
	@brief The WiringCheck namespace
	Homiservi-SCH: what is left to do on the wires of a project, before
	they go to the workshop:
	- a wire without a number, when the wires are numbered;
	- a wire number (ProjectNumbering::isWireNumber()) given to more than
	  one potential, or a potential with more than one number;
	- a wire without colour or section, when most of the others have them;
	- a terminal with more wires than the wiring rules allow;
	- end labels that do not follow the rule of the project;
	- wires left out of the project database (an end on no element).
*/
namespace WiringCheck
{
	enum Severity { Error, Warning, Info };

	struct Issue
	{
		Severity severity = Info;
			///The wire to show, null for an issue of the whole project
		QPointer<Conductor> conductor;
		QString folio;
			///For the order: the folios in the order of the project
		int folio_index = -1;
		QString wire;
		QString text;
	};

	QList<Issue> check(QETProject *project);
	QString severityName(Severity severity);
}

#endif // WIRINGCHECK_H
