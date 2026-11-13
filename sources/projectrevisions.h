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

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class Diagram;
class QDomElement;
class QETProject;

/**
	@brief The ProjectRevisions namespace
	The revisions of a project (edition, modification, who modified, who
	approved, date) and the folios each one changed. Homiservi-SCH keeps
	them in the project (<project_revisions>); a project saved before has
	them as project properties (rev1_ed...), read as revisions of every
	folio.

	Every folio shows its own revisions: the revision table of its title
	block (rev1_ed, rev1_desc... in the title block data of the folio, the
	newest first, TableRows of them) lists the revisions of the whole
	project and the ones that changed it, and its revision index is the
	edition of the newest of them. The project properties rev1_ed... are
	left empty, so they show through nowhere.

	A revision keeps the content of every folio when it is made (a hash,
	see folioHash()): the folios whose content differs at the next revision
	are the ones it changes.
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
			///Uuids of the folios it changes; none: every folio
		QStringList folios;
			///Uuid of every folio -> hash of its content, when it was made
		QHash<QString, QString> snapshot;

		bool isEmpty() const;
		bool isGeneral() const {return folios.isEmpty();}
		bool affects(const QString &folio_uuid) const;
		bool operator==(const Revision &other) const;
	};

		//Without a project (projectrevisionlist.cpp)
	QString nextEdition(const QString &edition);
	QList<Revision> forFolio(const QList<Revision> &revisions, const QString &folio_uuid);
	QString folioEdition(const QList<Revision> &revisions, const QString &folio_uuid);
	QStringList changedFolios(const QList<Revision> &revisions, const QHash<QString, QString> &hashes);
	QList<Revision> fromXml(const QDomElement &project_root, bool *found = nullptr);
	void toXml(const QList<Revision> &revisions, QDomElement &project_root);

		//With a project (projectrevisions.cpp)
	QList<Revision> read(QETProject *project);
	QList<Revision> readLegacy(QETProject *project);
	void write(QETProject *project, const QList<Revision> &revisions);
	void ensureTable(QETProject *project);
	QString folioHash(Diagram *diagram);
	QHash<QString, QString> folioHashes(QETProject *project);
}

#endif // PROJECTREVISIONS_H
