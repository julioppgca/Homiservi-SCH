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

#include "dataBase/projectdatabase.h"
#include "diagram.h"
#include "qetproject.h"

#include <QCryptographicHash>
#include <QDomDocument>
#include <QTextStream>

namespace {
	QString key(int number, const char *field)
	{
		return QStringLiteral("rev%1_%2").arg(number).arg(QLatin1String(field));
	}

	const char *const fields[] = {"ed", "desc", "by", "appr", "date"};

	void setRow(DiagramContext &context, int number, const ProjectRevisions::Revision &revision)
	{
		context.addValue(key(number, "ed"), revision.edition);
		context.addValue(key(number, "desc"), revision.description);
		context.addValue(key(number, "by"), revision.modified_by);
		context.addValue(key(number, "appr"), revision.approved_by);
		context.addValue(key(number, "date"), revision.date);
	}

		///The revision table of context: the rows of revisions, then empty
		///rows up to TableRows, nothing after
	void setTable(DiagramContext &context, const QList<ProjectRevisions::Revision> &revisions)
	{
		for (int number = 1; number <= ProjectRevisions::TableRows; ++number)
			setRow(context, number, number <= revisions.size() ? revisions.at(number - 1)
									   : ProjectRevisions::Revision());
		for (int number = ProjectRevisions::TableRows + 1; context.contains(key(number, "ed")); ++number)
			for (const char *field : fields)
				context.remove(key(number, field));
	}
}

/**
	@return the revisions of project, the newest first
*/
QList<ProjectRevisions::Revision> ProjectRevisions::read(QETProject *project)
{
	return project ? project->revisions() : QList<Revision>();
}

/**
	@return the revisions a project saved before Homiservi-SCH kept them
	itself has as project properties (rev1_ed...): revisions of every
	folio, without the empty rows of the title block table
*/
QList<ProjectRevisions::Revision> ProjectRevisions::readLegacy(QETProject *project)
{
	const DiagramContext properties = project->projectProperties();
	QList<Revision> revisions;
	for (int number = 1; properties.contains(key(number, "ed")); ++number)
	{
		Revision revision;
		revision.edition = properties.value(key(number, "ed")).toString();
		revision.description = properties.value(key(number, "desc")).toString();
		revision.modified_by = properties.value(key(number, "by")).toString();
		revision.approved_by = properties.value(key(number, "appr")).toString();
		revision.date = properties.value(key(number, "date")).toString();
		if (!revision.isEmpty())
			revisions << revision;
	}
	return revisions;
}

/**
	@brief ProjectRevisions::write
	Keep revisions (the newest first) in project and give every folio its
	own: the revision table of its title block lists the revisions changing
	it, its revision index is the edition of the newest of them (left as it
	is when none does). The title block of the new folios gets the
	revisions of the whole project. The project properties rev1_ed... are
	left empty: a value of the project would show through the empty rows of
	a folio.
*/
void ProjectRevisions::write(QETProject *project, const QList<Revision> &revisions)
{
	project->setRevisions(revisions);

	DiagramContext properties = project->projectProperties();
	setTable(properties, QList<Revision>());

	TitleBlockProperties defaults = project->defaultTitleBlockProperties();
	setTable(defaults.context, forFolio(revisions, QString()));
	const QString general = folioEdition(revisions, QString());
	if (!general.isEmpty())
		defaults.indexrev = general;
	project->setDefaultTitleBlockProperties(defaults);

	for (Diagram *diagram : project->diagrams())
	{
		const QString uuid = diagram->uuid().toString();
		const TitleBlockProperties old_title_block = diagram->border_and_titleblock.exportTitleBlock();
		TitleBlockProperties title_block = old_title_block;
		setTable(title_block.context, forFolio(revisions, uuid));
		const QString edition = folioEdition(revisions, uuid);
		if (!edition.isEmpty())
			title_block.indexrev = edition;
		if (title_block != old_title_block)
			diagram->border_and_titleblock.importTitleBlock(title_block);
	}
		//Last: it refreshes the title block of every folio
	project->setProjectProperties(properties);
	project->setModified(true);
		//The list of revisions follows, even when no title block changed
	project->dataBase()->diagramsInfoChanged();
}

/**
	Give project and its folios their revision tables, if the project has
	not all of the rows yet
*/
void ProjectRevisions::ensureTable(QETProject *project)
{
	if (!project->projectProperties().contains(key(TableRows, "ed")))
		write(project, read(project));
}

/**
	@brief ProjectRevisions::folioHash
	@return a hash of what diagram draws: its elements, wires, texts,
	shapes, pictures, tables... without its title block, which the
	revisions write, nor what the project numbering sets
*/
QString ProjectRevisions::folioHash(Diagram *diagram)
{
	if (!diagram) return QString();
	const QDomDocument document = diagram->toXml(true);
	QCryptographicHash hash(QCryptographicHash::Sha1);
	for (QDomElement child = document.documentElement().firstChildElement(); !child.isNull();
	     child = child.nextSiblingElement()) {
		const QString tag = child.tagName();
		if (tag == QLatin1String("properties") || tag == QLatin1String("defaultconductor")
		    || tag.endsWith(QLatin1String("foliosequentials")))
			continue;
		QString text;
		QTextStream stream(&text);
		child.save(stream, 0);
		hash.addData(text.toUtf8());
	}
	return QString::fromLatin1(hash.result().toHex());
}

/**
	@return the hash of the content of every folio of project, by uuid
*/
QHash<QString, QString> ProjectRevisions::folioHashes(QETProject *project)
{
	QHash<QString, QString> hashes;
	if (project)
		for (Diagram *diagram : project->diagrams())
			hashes.insert(diagram->uuid().toString(), folioHash(diagram));
	return hashes;
}
