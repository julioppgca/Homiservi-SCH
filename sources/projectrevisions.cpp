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

#include "diagram.h"
#include "qetproject.h"

namespace {
	QString key(int number, const char *field)
	{
		return QStringLiteral("rev%1_%2").arg(number).arg(QLatin1String(field));
	}

		///The revision number of properties, read until the first missing one
	ProjectRevisions::Revision revision(const DiagramContext &properties, int number)
	{
		ProjectRevisions::Revision revision;
		revision.edition = properties.value(key(number, "ed")).toString();
		revision.description = properties.value(key(number, "desc")).toString();
		revision.modified_by = properties.value(key(number, "by")).toString();
		revision.approved_by = properties.value(key(number, "appr")).toString();
		revision.date = properties.value(key(number, "date")).toString();
		return revision;
	}

	void setRevision(DiagramContext &properties, int number, const ProjectRevisions::Revision &revision)
	{
		properties.addValue(key(number, "ed"), revision.edition);
		properties.addValue(key(number, "desc"), revision.description);
		properties.addValue(key(number, "by"), revision.modified_by);
		properties.addValue(key(number, "appr"), revision.approved_by);
		properties.addValue(key(number, "date"), revision.date);
	}

	void removeRevision(DiagramContext &properties, int number)
	{
		for (const char *field : {"ed", "desc", "by", "appr", "date"})
			properties.remove(key(number, field));
	}
}

bool ProjectRevisions::Revision::isEmpty() const
{
	return edition.isEmpty() && description.isEmpty() && modified_by.isEmpty()
			&& approved_by.isEmpty() && date.isEmpty();
}

/**
	@return the revisions of project, the newest first, without the empty
	rows of the title block table
*/
QList<ProjectRevisions::Revision> ProjectRevisions::read(QETProject *project)
{
	const DiagramContext properties = project->projectProperties();
	QList<Revision> revisions;
	for (int number = 1; properties.contains(key(number, "ed")); ++number)
	{
		const Revision current = revision(properties, number);
		if (!current.isEmpty())
			revisions << current;
	}
	return revisions;
}

/**
	@brief ProjectRevisions::write
	Keep revisions (the newest first) in project, and the edition of the
	newest one as the revision index of every folio, the one the table of
	contents shows.
*/
void ProjectRevisions::write(QETProject *project, const QList<Revision> &revisions)
{
	DiagramContext properties = project->projectProperties();
	const int count = qMax(int(revisions.count()), TableRows);
	for (int number = 1; number <= count; ++number)
		setRevision(properties, number,
			    number <= revisions.count() ? revisions.at(number - 1) : Revision());
	for (int number = count + 1; properties.contains(key(number, "ed")); ++number)
		removeRevision(properties, number);

		//Without revisions, the index of the folios is the one the user gave
	const QString index = revisions.isEmpty() ? QString() : revisions.constFirst().edition;
	if (!index.isEmpty())
	{
		TitleBlockProperties defaults = project->defaultTitleBlockProperties();
		defaults.indexrev = index;
		project->setDefaultTitleBlockProperties(defaults);
		for (Diagram *diagram : project->diagrams())
		{
			TitleBlockProperties title_block = diagram->border_and_titleblock.exportTitleBlock();
			if (title_block.indexrev == index) continue;
			title_block.indexrev = index;
			diagram->border_and_titleblock.importTitleBlock(title_block);
		}
	}
		//Last: it refreshes the title block of every folio
	project->setProjectProperties(properties);
	project->setModified(true);
}

/**
	Write the empty rows of the revision table of project, if it has not
	all of them yet
*/
void ProjectRevisions::ensureTable(QETProject *project)
{
	if (!project->projectProperties().contains(key(TableRows, "ed")))
		write(project, read(project));
}

/**
	@return the edition that follows edition: A, B... Z, AA, AB... The
	letters I and O are skipped, they are read as 1 and 0. An empty or
	numeric edition is followed by the next number, 0 when empty.
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
