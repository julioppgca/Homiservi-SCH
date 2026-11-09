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
#ifndef PROJECTINDEX_H
#define PROJECTINDEX_H

#include <QList>
#include <QString>
#include <QStringList>

class Diagram;
class QETProject;

/**
	@brief The ProjectIndex namespace
	Homiservi-SCH: the two indexes of a project, tables read from its
	database, which follow the folios and the sections as they change:
	- the folio index: folio, title, location..., grouped by section under
	  a heading row ("02 - DISTRIBUIÇÃO DE FORÇA") or with a section column;
	- the section index: code, name, first to last folio, folio count
	  (project_sections_view).
*/
namespace ProjectIndex
{
	enum Kind { Folios, Sections };

	struct Column
	{
		QString key;
		QString title;
	};

	QList<Column> columns(Kind kind);
	QStringList defaultColumns(Kind kind, bool revisions = false);
	QString columnTitle(Kind kind, const QString &key);
	QString defaultName(Kind kind);
	QString query(Kind kind, const QStringList &keys, bool grouped = true);
	bool recognize(const QString &query, Kind *kind = nullptr, QStringList *keys = nullptr,
		       bool *grouped = nullptr);

	bool hasSections(QETProject *project);
	Diagram *addIndexFolio(QETProject *project, Kind kind, Diagram *after = nullptr);
	void insert(Diagram *diagram, const QString &query, const QString &name,
		    bool adjust_to_folio = true, bool more_folios = true);
}

#endif
