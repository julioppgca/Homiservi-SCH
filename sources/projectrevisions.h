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
#ifndef PROJECTREVISIONS_H
#define PROJECTREVISIONS_H

#include <QList>
#include <QString>

class QETProject;

/**
	@brief The ProjectRevisions namespace
	The revisions of a project (edition, modification, who modified, who
	approved, date), shown by the revision table of a title block.
	They are kept as project properties, the newest first: rev1_ed,
	rev1_desc, rev1_by, rev1_appr, rev1_date, then rev2_... The table of
	the title block shows the first TableRows of them; the empty rows are
	written too, else the title block would show their variable names.
*/
namespace ProjectRevisions
{
	constexpr int TableRows = 10;

	struct Revision
	{
		QString edition;	///< A, B, C...
		QString description;
		QString modified_by;
		QString approved_by;
		QString date;		///< as the user writes it

		bool isEmpty() const;
	};

	QList<Revision> read(QETProject *project);
	void write(QETProject *project, const QList<Revision> &revisions);
	void ensureTable(QETProject *project);
	QString nextEdition(const QString &edition);
}

#endif // PROJECTREVISIONS_H
