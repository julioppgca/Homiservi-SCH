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
#include "projectrevisions.h"

#include <QDomDocument>
#include <QDomElement>

#include <algorithm>

/*
	The functions of ProjectRevisions that need no project, so that they
	can be tested alone.
*/

bool ProjectRevisions::Revision::isEmpty() const
{
	return edition.isEmpty() && description.isEmpty() && modified_by.isEmpty()
			&& approved_by.isEmpty() && date.isEmpty();
}

/**
	@return true if this revision changes the folio of uuid folio_uuid: a
	revision of the whole project changes every folio
*/
bool ProjectRevisions::Revision::affects(const QString &folio_uuid) const
{
	return folios.isEmpty() || folios.contains(folio_uuid);
}

bool ProjectRevisions::Revision::operator==(const Revision &other) const
{
	return edition == other.edition && description == other.description
			&& modified_by == other.modified_by && approved_by == other.approved_by
			&& date == other.date && folios == other.folios && snapshot == other.snapshot;
}

/**
	@return the revisions of revisions (the newest first) that change the
	folio of uuid folio_uuid; with an empty uuid, the ones of the whole
	project
*/
QList<ProjectRevisions::Revision> ProjectRevisions::forFolio(const QList<Revision> &revisions,
							       const QString &folio_uuid)
{
	QList<Revision> list;
	for (const Revision &revision : revisions)
		if (folio_uuid.isEmpty() ? revision.isGeneral() : revision.affects(folio_uuid))
			list << revision;
	return list;
}

/**
	@return the edition of the newest revision changing the folio of uuid
	folio_uuid, empty if none
*/
QString ProjectRevisions::folioEdition(const QList<Revision> &revisions, const QString &folio_uuid)
{
	const QList<Revision> list = forFolio(revisions, folio_uuid);
	return list.isEmpty() ? QString() : list.constFirst().edition;
}

/**
	@brief ProjectRevisions::changedFolios
	@return the folios of hashes (uuid -> hash of their content now) whose
	content changed since the newest revision of revisions: its hash
	differs, or the folio did not exist yet. Nothing when the newest
	revision kept no content (a revision of a project saved before).
*/
QStringList ProjectRevisions::changedFolios(const QList<Revision> &revisions,
					    const QHash<QString, QString> &hashes)
{
	QStringList changed;
	if (revisions.isEmpty() || revisions.constFirst().snapshot.isEmpty())
		return changed;
	const QHash<QString, QString> &before = revisions.constFirst().snapshot;
	for (auto it = hashes.constBegin(); it != hashes.constEnd(); ++it)
		if (before.value(it.key()) != it.value())
			changed << it.key();
	std::sort(changed.begin(), changed.end());
	return changed;
}

/**
	@brief ProjectRevisions::fromXml
	Read the <project_revisions> child of project_root
	@param found : set to whether project_root has one
*/
QList<ProjectRevisions::Revision> ProjectRevisions::fromXml(const QDomElement &project_root, bool *found)
{
	QList<Revision> revisions;
	const QDomElement root = project_root.firstChildElement(QStringLiteral("project_revisions"));
	if (found) *found = !root.isNull();
	for (QDomElement e = root.firstChildElement(QStringLiteral("revision")); !e.isNull();
	     e = e.nextSiblingElement(QStringLiteral("revision"))) {
		Revision revision;
		revision.edition = e.attribute(QStringLiteral("edition"));
		revision.description = e.attribute(QStringLiteral("description"));
		revision.modified_by = e.attribute(QStringLiteral("modified_by"));
		revision.approved_by = e.attribute(QStringLiteral("approved_by"));
		revision.date = e.attribute(QStringLiteral("date"));
		for (QDomElement f = e.firstChildElement(QStringLiteral("folio")); !f.isNull();
		     f = f.nextSiblingElement(QStringLiteral("folio")))
			revision.folios << f.attribute(QStringLiteral("uuid"));
		for (QDomElement f = e.firstChildElement(QStringLiteral("content")); !f.isNull();
		     f = f.nextSiblingElement(QStringLiteral("content")))
			revision.snapshot.insert(f.attribute(QStringLiteral("folio")), f.attribute(QStringLiteral("hash")));
		revisions << revision;
	}
	return revisions;
}

/**
	@brief ProjectRevisions::toXml
	Write revisions as a <project_revisions> child of project_root, the
	newest first
*/
void ProjectRevisions::toXml(const QList<Revision> &revisions, QDomElement &project_root)
{
	QDomDocument document = project_root.ownerDocument();
	QDomElement root = document.createElement(QStringLiteral("project_revisions"));
	for (const Revision &revision : revisions) {
		QDomElement e = document.createElement(QStringLiteral("revision"));
		e.setAttribute(QStringLiteral("edition"), revision.edition);
		e.setAttribute(QStringLiteral("description"), revision.description);
		e.setAttribute(QStringLiteral("modified_by"), revision.modified_by);
		e.setAttribute(QStringLiteral("approved_by"), revision.approved_by);
		e.setAttribute(QStringLiteral("date"), revision.date);
		for (const QString &uuid : revision.folios) {
			QDomElement f = document.createElement(QStringLiteral("folio"));
			f.setAttribute(QStringLiteral("uuid"), uuid);
			e.appendChild(f);
		}
			//Sorted, so that saving twice gives the same file
		QStringList uuids = revision.snapshot.keys();
		uuids.sort();
		for (const QString &uuid : std::as_const(uuids)) {
			QDomElement f = document.createElement(QStringLiteral("content"));
			f.setAttribute(QStringLiteral("folio"), uuid);
			f.setAttribute(QStringLiteral("hash"), revision.snapshot.value(uuid));
			e.appendChild(f);
		}
		root.appendChild(e);
	}
	project_root.appendChild(root);
}

/**
	@return the edition that follows edition: A, B... Z, AA, AB... The
	letters I and O are skipped, they are read as 1 and 0. A numeric
	edition is followed by the next number, an empty one by A.
*/
QString ProjectRevisions::nextEdition(const QString &edition)
{
	const QString current = edition.trimmed().toUpper();
	if (current.isEmpty())
		return QStringLiteral("A");
	bool is_number = false;
	const int number = current.toInt(&is_number);
	if (is_number)
		return QString::number(number + 1);

	QString next = current;
	int i = next.size() - 1;
	for (; i >= 0; --i)
	{
		QChar c = next.at(i);
		if (c < QLatin1Char('A') || c > QLatin1Char('Z'))
			return current + QLatin1Char('1');
		if (c != QLatin1Char('Z'))
		{
			c = QChar(c.unicode() + 1);
			if (c == QLatin1Char('I') || c == QLatin1Char('O'))
				c = QChar(c.unicode() + 1);
			next[i] = c;
			return next;
		}
		next[i] = QLatin1Char('A');
	}
	return QLatin1Char('A') + next;
}
