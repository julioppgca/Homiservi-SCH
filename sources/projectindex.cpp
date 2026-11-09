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
#include "projectindex.h"

#include "dataBase/ui/summaryquerywidget.h"
#include "diagram.h"
#include "factory/qetgraphicstablefactory.h"
#include "factory/ui/addtabledialog.h"
#include "projectvariants.h"
#include "qetproject.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace
{
	QString tr(const char *text)
	{
		return QCoreApplication::translate("ProjectIndex", text);
	}

		///The SQL of a column of the folio index, read from project_summary_view
	QString folioExpression(const QString &key)
	{
		if (key == QLatin1String("section"))
			return QStringLiteral("COALESCE(section_code || ' – ' || section_name, section_code, '')");
		if (key == QLatin1String("variant"))
			return QStringLiteral("COALESCE(NULLIF(variant, ''), '%1')").arg(ProjectVariants::Standard);
		return QStringLiteral("COALESCE(%1, '')").arg(key);
	}

		///The SQL of a column of the section index, read from project_sections_view
	QString sectionExpression(const QString &key)
	{
		if (key == QLatin1String("folios"))
			return QStringLiteral("CASE WHEN folio_count > 1 THEN first_folio || ' – ' || last_folio"
					      " ELSE COALESCE(first_folio, '') END");
		return key;
	}

	QString quoted(const QString &title)
	{
		QString escaped = title;
		escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
		return QLatin1Char('"') + escaped + QLatin1Char('"');
	}
}

/**
	@return the columns an index of kind can show, in their usual order
*/
QList<ProjectIndex::Column> ProjectIndex::columns(Kind kind)
{
	if (kind == Sections)
		return {{QStringLiteral("code"), tr("Código")},
			{QStringLiteral("name"), tr("Seção")},
			{QStringLiteral("folios"), tr("Folhas")},
			{QStringLiteral("folio_count"), tr("Qtd. de folhas")}};
	return {{QStringLiteral("folio"), tr("Folha")},
		{QStringLiteral("title"), tr("Título")},
		{QStringLiteral("section"), tr("Seção")},
		{QStringLiteral("locmach"), tr("Localização")},
		{QStringLiteral("plant"), tr("Instalação")},
		{QStringLiteral("indexrev"), tr("Revisão")},
		{QStringLiteral("date"), tr("Data")},
		{QStringLiteral("variant"), tr("Tipo")},
		{QStringLiteral("author"), tr("Autor")}};
}

/**
	@return the columns shown by default; for the folio index of the
	Revisions layout, its revision and type columns
*/
QStringList ProjectIndex::defaultColumns(Kind kind, bool revisions)
{
	if (kind == Sections)
		return {QStringLiteral("code"), QStringLiteral("name"), QStringLiteral("folios"),
			QStringLiteral("folio_count")};
	if (revisions)
		return {QStringLiteral("folio"), QStringLiteral("title"), QStringLiteral("indexrev"),
			QStringLiteral("variant")};
	return {QStringLiteral("folio"), QStringLiteral("title"), QStringLiteral("locmach"),
		QStringLiteral("indexrev")};
}

QString ProjectIndex::columnTitle(Kind kind, const QString &key)
{
	for (const Column &column : columns(kind))
		if (column.key == key) return column.title;
	return key;
}

QString ProjectIndex::defaultName(Kind kind)
{
	return kind == Sections ? tr("Índice de seções") : tr("Índice de folhas");
}

/**
	@brief ProjectIndex::query
	@return the query of an index of kind showing the columns keys
	@param grouped : for the folio index, each section opens with a heading
	row, its code and name in the title column (or the first one), and the
	section column is left out
*/
QString ProjectIndex::query(Kind kind, const QStringList &keys, bool grouped)
{
	if (kind == Sections) {
		QStringList select;
		for (const QString &key : keys)
			select << sectionExpression(key) + QStringLiteral(" AS ") + quoted(columnTitle(kind, key));
		if (select.isEmpty()) select << QStringLiteral("code");
		return QStringLiteral("SELECT %1 FROM project_sections_view ORDER BY rank").arg(select.join(QStringLiteral(", ")));
	}

	QStringList shown = keys;
	if (grouped) shown.removeAll(QStringLiteral("section"));
	if (shown.isEmpty()) shown << QStringLiteral("folio") << QStringLiteral("title");

	if (!grouped) {
		QStringList select;
		for (const QString &key : std::as_const(shown))
			select << folioExpression(key) + QStringLiteral(" AS ") + quoted(columnTitle(kind, key));
		return QStringLiteral("SELECT %1 FROM project_summary_view ORDER BY pos").arg(select.join(QStringLiteral(", ")));
	}

		//The folios and, before each section, its heading row
	const int heading_column = qMax(0, int(shown.indexOf(QStringLiteral("title"))));
	QStringList folio_select, heading_select, outer;
	for (int i = 0; i < shown.size(); ++i) {
		const QString alias = QStringLiteral("c%1").arg(i);
		folio_select << folioExpression(shown.at(i)) + QStringLiteral(" AS ") + alias;
		heading_select << (i == heading_column ? QStringLiteral("heading") : QStringLiteral("''"))
				  + QStringLiteral(" AS ") + alias;
		outer << alias + QStringLiteral(" AS ") + quoted(columnTitle(kind, shown.at(i)));
	}
	return QStringLiteral("SELECT %1 FROM ("
			      "SELECT COALESCE(section_rank, -1) AS r, 1 AS h, pos AS p, %2 FROM project_summary_view"
			      " UNION ALL "
			      "SELECT rank AS r, 0 AS h, 0 AS p, %3 FROM project_sections_view WHERE folio_count > 0"
			      ") ORDER BY r, h, p")
			.arg(outer.join(QStringLiteral(", ")), folio_select.join(QStringLiteral(", ")),
			     heading_select.join(QStringLiteral(", ")));
}

/**
	@return true if the project has sections (ProjectNumbering)
*/
bool ProjectIndex::hasSections(QETProject *project)
{
	return project && !project->numbering().sections.isEmpty();
}

/**
	@brief ProjectIndex::addIndexFolio
	Add a folio for an index of kind, titled with its name: after the last
	folio of the first section when the project has sections (with the
	cover), else after the folio after, or at the end.
	@return the new folio
*/
Diagram *ProjectIndex::addIndexFolio(QETProject *project, Kind kind, Diagram *after)
{
	if (!project) return nullptr;
	const QList<Diagram *> diagrams = project->diagrams();
	int position = int(diagrams.size());
	if (hasSections(project)) {
		const ProjectNumbering::Settings numbering = project->numbering();
		const QStringList codes = ProjectNumbering::sectionCodes(numbering, project->numberingFolios());
		const QString first = numbering.sections.first().code;
		position = 0;
		for (int i = 0; i < codes.size(); ++i)
			if (codes.at(i) == first) position = i + 1;
	} else if (after) {
		position = int(diagrams.indexOf(after)) + 1;
	}
	Diagram *diagram = project->addNewDiagram(position);
	if (!diagram) return nullptr;
	TitleBlockProperties properties = diagram->border_and_titleblock.exportTitleBlock();
	properties.title = defaultName(kind);
	diagram->border_and_titleblock.importTitleBlock(properties);
	return diagram;
}

/**
	@brief ProjectIndex::insert
	Add to diagram the table of query, named name; with more_folios, the
	rows that do not fit go on new folios after it
*/
void ProjectIndex::insert(Diagram *diagram, const QString &query, const QString &name,
			  bool adjust_to_folio, bool more_folios)
{
	if (!diagram) return;
	auto content = new SummaryQueryWidget();
	content->setQuery(query);
	AddTableDialog dialog(content);
	dialog.setTableName(name);
	dialog.setAdjustTableToFolio(adjust_to_folio);
	dialog.setAddNewTableToNewDiagram(more_folios);
	QETProject *project = diagram->project();
	const QList<Diagram *> before = project->diagrams();
	QetGraphicsTableFactory::create(diagram, &dialog);

		//The folios the rows that did not fit went on: "Índice de folhas (2)"
	int number = 1;
	for (Diagram *folio : project->diagrams()) {
		if (before.contains(folio)) continue;
		TitleBlockProperties properties = folio->border_and_titleblock.exportTitleBlock();
		properties.title = QStringLiteral("%1 (%2)").arg(name).arg(++number);
		folio->border_and_titleblock.importTitleBlock(properties);
	}
}

/**
	@brief ProjectIndex::recognize
	@return true if query is the query of an index, as query() makes it:
	then kind, keys and grouped tell which. The titles of its columns give
	the keys, and the query made from them must be query itself.
*/
bool ProjectIndex::recognize(const QString &query, Kind *kind, QStringList *keys, bool *grouped)
{
	static const QRegularExpression title_re(QStringLiteral("AS \"((?:[^\"]|\"\")*)\""));
	QStringList titles;
	QRegularExpressionMatchIterator it = title_re.globalMatch(query);
	while (it.hasNext())
		titles << it.next().captured(1).replace(QStringLiteral("\"\""), QStringLiteral("\""));

	for (Kind k : {Folios, Sections}) {
		QStringList found;
		for (const QString &title : std::as_const(titles))
			for (const Column &column : columns(k))
				if (column.title == title) { found << column.key; break; }
		if (found.size() != titles.size()) continue;
		for (bool g : {true, false}) {
			if (ProjectIndex::query(k, found, g) != query) continue;
			if (kind) *kind = k;
			if (keys) *keys = found;
			if (grouped) *grouped = k == Folios && g;
			return true;
		}
	}
	return false;
}
