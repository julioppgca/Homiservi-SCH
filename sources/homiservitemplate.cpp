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
#include "homiservitemplate.h"

#include "dataBase/ui/elementquerywidget.h"
#include "dataBase/ui/summaryquerywidget.h"
#include "diagram.h"
#include "factory/qetgraphicstablefactory.h"
#include "factory/ui/addtabledialog.h"
#include "potentialrules.h"
#include "projectrevisions.h"
#include "projectvariants.h"
#include "properties/elementdata.h"
#include "qetapp.h"
#include "qetgraphicsitem/diagramimageitem.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetproject.h"
#include "textstyles.h"
#include "titleblock/templatescollection.h"

#include <QCoreApplication>
#include <QFile>
#include <QUuid>

const QString HomiserviTemplate::TitleBlockName = QStringLiteral("Homiservi_Legenda");
const QString HomiserviTemplate::RevisionsTitleBlockName = QStringLiteral("Homiservi_Legenda_Revisoes");

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("HomiserviTemplate", text);
	}

		///The data of every folio kept as project properties, not per folio
	const QStringList project_keys {QStringLiteral("cliente"), QStringLiteral("drwnumber"),
					QStringLiteral("verficado"), QStringLiteral("verifie"),
					QStringLiteral("projetado")};

		///Cross references and folio reports written folio.rowcolumn: 15.B4
	const QString revisions_reference = QStringLiteral("%f.%l%c");

		///A3 folios of the Revisions layout: 13 columns, rows A to F
	BorderProperties revisionsBorder()
	{
		BorderProperties border = BorderProperties::defaultProperties();
		border.columns_count = 13;
		border.columns_width = 90;
		border.rows_count = 6;
		border.rows_height = 120;
		border.display_columns = true;
		border.display_rows = true;
		return border;
	}

	void embedTitleBlock(QETProject *project, const QString &name)
	{
		TitleBlockTemplatesProjectCollection *collection = project->embeddedTitleBlockTemplatesCollection();
		if (collection->templates().contains(name))
			return;
		QFile file(QStringLiteral(":/ico/homiservi/%1.titleblock").arg(name));
		QDomDocument document;
		if (file.open(QIODevice::ReadOnly) && document.setContent(&file))
			collection->setTemplateXmlDescription(name, document.documentElement());
	}

		///The texts of the cover, refreshed by apply()
	enum CoverText { Heading, Title, Client, Place, Number, CoverTextCount };

	/**
		The cover texts are found again by their uuid, the same in every
		project: derived from their role, not random.
	*/
	QUuid coverUuid(CoverText text)
	{
		static const QUuid name_space(QStringLiteral("{3c0f9d52-6a8e-4c1b-9f43-1d7b2e5a8c61}"));
		return QUuid::createUuidV5(name_space, QStringLiteral("homiservi-cover-%1").arg(int(text)));
	}

	QString coverText(CoverText text, const HomiserviTemplate::Data &data)
	{
		switch (text)
		{
			case Heading: return tr("DIAGRAMA ELÉTRICO");
			case Title:   return data.title;
			case Client:  return data.client.isEmpty() ? QString() : tr("Cliente: %1").arg(data.client);
			case Place:
			{
				QStringList parts;
				if (!data.plant.isEmpty()) parts << tr("Instalação: %1").arg(data.plant);
				if (!data.location.isEmpty()) parts << tr("Local: %1").arg(data.location);
				return parts.join(QStringLiteral("   —   "));
			}
			case Number:
			{
				QStringList parts;
				if (!data.drawing_number.isEmpty()) parts << tr("Desenho nº %1").arg(data.drawing_number);
				if (!data.revision.isEmpty()) parts << tr("Revisão %1").arg(data.revision);
				if (data.date.isValid()) parts << QLocale().toString(data.date, QLocale::ShortFormat);
				return parts.join(QStringLiteral("   —   "));
			}
			case CoverTextCount: break;
		}
		return QString();
	}

	QFont coverFont(CoverText text)
	{
		QFont font = QETApp::diagramTextsFont(text == Heading ? 22 : text == Title ? 18 : 12);
		font.setBold(text == Heading || text == Title);
		return font;
	}

		///Centre text on x, keeping its height
	void centre(IndependentTextItem *text, qreal x)
	{
		text->setPos(x - text->boundingRect().width() / 2, text->pos().y());
	}

	void createCover(Diagram *diagram, const HomiserviTemplate::Data &data)
	{
		const QRectF inside = diagram->border_and_titleblock.insideBorderRect();
		const qreal x = inside.center().x();

		const QPixmap logo(QStringLiteral(":/ico/homiservi/homiservi-logo.png"));
		if (!logo.isNull())
		{
			auto image = new DiagramImageItem(logo);
			const qreal scale = 260.0 / logo.width();
			image->setScaleFactorX(scale);
			image->setScaleFactorY(scale);
			diagram->addItem(image);
			image->setPos(QPointF(x, inside.top() + 150) - image->boundingRect().center());
		}

		qreal y = inside.top() + 290;
		for (int i = 0; i < CoverTextCount; ++i)
		{
			const auto role = CoverText(i);
			auto text = new IndependentTextItem();
			text->setUuid(coverUuid(role));
			text->setFont(coverFont(role));
				//Its own look, not the one of the free texts
			text->setTextStyle(TextStyles::NoStyle);
			text->setPlainText(coverText(role, data));
			diagram->addItem(text);
			text->setPos(0, y);
			centre(text, x);
			y += role == Heading ? 55 : role == Title ? 60 : 32;
		}
	}

	void updateCover(QETProject *project, const HomiserviTemplate::Data &data)
	{
		for (Diagram *diagram : project->diagrams())
		{
			const qreal x = diagram->border_and_titleblock.insideBorderRect().center().x();
			for (QGraphicsItem *item : diagram->items())
			{
				if (item->type() != IndependentTextItem::Type) continue;
				auto text = static_cast<IndependentTextItem *>(item);
				for (int i = 0; i < CoverTextCount; ++i)
				{
					if (text->uuid() != coverUuid(CoverText(i))) continue;
					const QString value = coverText(CoverText(i), data);
					if (text->toPlainText() != value) {
						text->setPlainText(value);
						centre(text, x);
					}
				}
			}
		}
	}

	void setTitle(Diagram *diagram, const QString &title)
	{
		TitleBlockProperties properties = diagram->border_and_titleblock.exportTitleBlock();
		properties.title = title;
		diagram->border_and_titleblock.importTitleBlock(properties);
	}

		///With more_folios, the rows that do not fit go on the next new folios
	void addTable(Diagram *diagram, QWidget *content, const QString &name, bool more_folios = false)
	{
		AddTableDialog dialog(content);
		dialog.setTableName(name);
		dialog.setAdjustTableToFolio(true);
		dialog.setAddNewTableToNewDiagram(more_folios);
		QetGraphicsTableFactory::create(diagram, &dialog);
	}

		///"TIPO: X = PADRÃO...", at the bottom left of the folio of diagram
	void addVariantLegend(Diagram *diagram)
	{
		auto text = new IndependentTextItem();
		text->setPlainText(tr("TIPO:") + QStringLiteral("    ") + ProjectVariants::legend());
		diagram->addItem(text);
		const QRectF inside = diagram->border_and_titleblock.insideBorderRect();
		text->setPos(inside.left() + 10, inside.bottom() - text->boundingRect().height() - 6);
	}

		///Write the title block data of data in properties
	void fill(TitleBlockProperties &properties, const HomiserviTemplate::Data &data,
		  bool use_title_block)
	{
		if (use_title_block)
		{
			properties.template_name = HomiserviTemplate::titleBlockName(data.layout);
			properties.collection = QET::Embedded;
		}
		properties.author = data.author;
		properties.date = data.date;
		properties.useDate = TitleBlockProperties::UseDateValue;
		properties.plant = data.plant;
		properties.locmach = data.location;
		properties.indexrev = data.revision;
		properties.filename = data.filename;
		if (properties.folio.isEmpty())
			properties.folio = QStringLiteral("%id/%total");
			//A value of the folio would hide the one of the project
		for (const QString &key : project_keys)
			properties.context.remove(key);
	}
}

/**
	@return the data of project as the template keeps it, today's date if
	it has none
*/
HomiserviTemplate::Data HomiserviTemplate::fromProject(QETProject *project)
{
	Data data;
	const DiagramContext properties = project->projectProperties();
	data.title = project->title();
	data.client = properties.value(QStringLiteral("cliente")).toString();
	data.drawing_number = properties.value(QStringLiteral("drwnumber")).toString();
	data.checker = properties.value(QStringLiteral("verficado")).toString();
	data.designer = properties.value(QStringLiteral("projetado")).toString();
	data.layout = project->defaultTitleBlockProperties().template_name == RevisionsTitleBlockName
			? Revisions : Simple;

	const TitleBlockProperties title_block = project->diagrams().isEmpty()
			? project->defaultTitleBlockProperties()
			: project->diagrams().constFirst()->border_and_titleblock.exportTitleBlock();
	data.author = title_block.author;
	data.plant = title_block.plant;
	data.location = title_block.locmach;
	data.revision = title_block.indexrev;
	data.filename = title_block.filename;
	data.date = title_block.date.isValid() ? title_block.date : QDate::currentDate();

		//The first folios of the template may still hold the client
	if (data.client.isEmpty())
		data.client = title_block.context.value(QStringLiteral("cliente")).toString();
	if (data.checker.isEmpty())
		data.checker = title_block.context.value(QStringLiteral("verficado")).toString();
	return data;
}

QString HomiserviTemplate::titleBlockName(Layout layout)
{
	return layout == Revisions ? RevisionsTitleBlockName : TitleBlockName;
}

/**
	@brief HomiserviTemplate::prepare
	Give project the title block of layout, as the one of its new folios,
	and with styles the text styles and wire rules of the template when it
	has none. The Revisions layout also gives every folio its border and
	the project its way of writing cross references.
*/
void HomiserviTemplate::prepare(QETProject *project, Layout layout, bool styles)
{
	const QString name = titleBlockName(layout);
	embedTitleBlock(project, name);

	TitleBlockProperties properties = project->defaultTitleBlockProperties();
	properties.template_name = name;
	properties.collection = QET::Embedded;
	if (properties.folio.isEmpty())
		properties.folio = QStringLiteral("%id/%total");
	project->setDefaultTitleBlockProperties(properties);

	if (layout == Revisions)
	{
		const BorderProperties border = revisionsBorder();
		project->setDefaultBorderProperties(border);
		for (Diagram *diagram : project->diagrams())
			if (diagram->border_and_titleblock.exportBorder() != border)
				diagram->border_and_titleblock.importBorder(border);

		QHash<QString, XRefProperties> xrefs = project->defaultXRefProperties();
		for (auto it = xrefs.begin(); it != xrefs.end(); ++it)
		{
			it->setMasterLabel(revisions_reference);
			it->setSlaveLabel(revisions_reference);
		}
		project->setDefaultXRefProperties(xrefs);
		project->setDefaultReportProperties(revisions_reference);
		ProjectRevisions::ensureTable(project);
	}

	if (styles && project->textStyles().isEmpty())
		project->setTextStyles(TextStyles::examples());
	if (styles && project->potentialRules().rules.isEmpty())
		project->setPotentialRules(PotentialRules::examples());
	project->setModified(true);
}

/**
	@brief HomiserviTemplate::apply
	Write data in project: its title and properties, the title block of
	every folio and of the new ones, and the texts of the cover. With
	use_title_block, every folio also gets the Homiservi title block.
*/
void HomiserviTemplate::apply(QETProject *project, const Data &data, bool use_title_block)
{
	if (use_title_block)
		prepare(project, data.layout, false);

	DiagramContext properties = project->projectProperties();
	properties.addValue(QStringLiteral("cliente"), data.client);
	properties.addValue(QStringLiteral("drwnumber"), data.drawing_number);
	properties.addValue(QStringLiteral("verficado"), data.checker);
	properties.addValue(QStringLiteral("verifie"), data.checker);
	properties.addValue(QStringLiteral("projetado"), data.designer);
	project->setTitle(data.title);

	TitleBlockProperties defaults = project->defaultTitleBlockProperties();
	fill(defaults, data, use_title_block);
	project->setDefaultTitleBlockProperties(defaults);

	for (Diagram *diagram : project->diagrams())
	{
		TitleBlockProperties title_block = diagram->border_and_titleblock.exportTitleBlock();
		fill(title_block, data, use_title_block);
		diagram->border_and_titleblock.importTitleBlock(title_block);
	}
		//Last: it refreshes the title block of every folio
	project->setProjectProperties(properties);
	updateCover(project, data);
	project->setModified(true);
}

/**
	@brief HomiserviTemplate::createFolios
	Add the folios of a new project at the end of project: cover, table of
	contents, one folio per title of folios.schematics, bill of
	materials. The tables fill themselves as the project grows.
*/
void HomiserviTemplate::createFolios(QETProject *project, const Data &data, const Folios &folios)
{
		//The section each new folio starts: "02" for the first folio of
		//the second section
	ProjectNumbering::Settings settings = numbering();
	auto start_section = [&settings, project](const QString &name) {
		const QString code = QString::number(settings.sections.size() + 1)
				.rightJustified(settings.section_digits, QLatin1Char('0'));
		settings.sections << ProjectNumbering::Section {code, name};
		const QList<Diagram *> diagrams = project->diagrams();
		settings.starts.insert(diagrams.last()->uuid().toString(), code);
	};

	if (folios.cover)
	{
		Diagram *diagram = project->addNewDiagram();
		setTitle(diagram, tr("Capa"));
		createCover(diagram, data);
		start_section(tr("Geral"));
	}
	if (folios.contents)
	{
		Diagram *diagram = project->addNewDiagram();
		setTitle(diagram, tr("Índice"));
		if (!folios.cover)
			start_section(tr("Geral"));
		auto query = new SummaryQueryWidget();
		if (data.layout == Revisions)
		{
			query->setQuery(QStringLiteral("SELECT title AS \"%1\", folio AS \"%2\", indexrev AS \"%3\","
						       " COALESCE(NULLIF(variant, ''), '%4') AS \"%5\""
						       " FROM project_summary_view ORDER BY pos")
					.arg(tr("Assunto"), tr("Folha"), tr("Ed."), ProjectVariants::Standard,
					     tr("Tipo")));
			addVariantLegend(diagram);
		}
		else
			query->setQuery(QStringLiteral("SELECT folio, title, plant, locmach, indexrev, date"
						       " FROM project_summary_view ORDER BY pos"));
		addTable(diagram, query, tr("Índice"));
	}
	for (const QString &title : folios.schematics)
	{
		Diagram *diagram = project->addNewDiagram();
		setTitle(diagram, title);
		start_section(title);
	}
	if (folios.materials && data.layout == Revisions)
	{
		const int first = int(project->diagrams().size());
		addComponentList(project);
			//The component list may take several folios: the section
			//starts at the first one
		const QList<Diagram *> diagrams = project->diagrams();
		if (diagrams.size() > first) {
			const QString code = QString::number(settings.sections.size() + 1)
					.rightJustified(settings.section_digits, QLatin1Char('0'));
			settings.sections << ProjectNumbering::Section {code, tr("Listas")};
			settings.starts.insert(diagrams.at(first)->uuid().toString(), code);
		}
	}
	else if (folios.materials)
	{
		Diagram *diagram = project->addNewDiagram();
		setTitle(diagram, tr("Lista de materiais"));
		start_section(tr("Listas"));
		auto query = new ElementQueryWidget();
		query->setQuery(QStringLiteral("SELECT label, designation, description, manufacturer,"
					       " manufacturer_reference, location"
					       " FROM element_nomenclature_view"
					       " WHERE (element_type = '%1' OR element_type = '%2' OR element_type = '%3')"
					       " ORDER BY label")
				.arg(ElementData::typeToString(ElementData::Simple),
				     ElementData::typeToString(ElementData::Master),
				     ElementData::typeToString(ElementData::Terminal)));
		addTable(diagram, query, tr("Lista de materiais"));
	}
	if (project->diagrams().isEmpty()) {
		project->addNewDiagram();
		start_section(tr("Geral"));
	}
	apply(project, data);
		//Last: it numbers every folio
	if (folios.numbering)
		project->setNumbering(settings);
}

/**
	@brief HomiserviTemplate::numbering
	@return the numbering of a Homiservi project, without sections: folios
	02.01 by section, tags -KA21 (the second -KA of section 02... the
	section without its zeros, then one digit, as the PV500 FANUC project),
	wires 02.01-01 by folio with their number near the component they
	come from
*/
ProjectNumbering::Settings HomiserviTemplate::numbering()
{
	ProjectNumbering::Settings settings;
	settings.enabled = true;
	settings.format = ProjectNumbering::SectionPage;
	settings.separator = QStringLiteral(".");
	settings.section_digits = 2;
	settings.page_digits = 2;
	settings.strip_titles = true;
	settings.tag_mode = ProjectNumbering::TagsBySectionCompact;
	settings.tag_digits = 1;
	settings.wire_mode = ProjectNumbering::WiresByFolio;
	settings.wire_digits = 2;
	settings.wire_separator = QStringLiteral("-");
	settings.wire_text_near_terminal = true;
	return settings;
}

/**
	@return true if project has the cover of the template
*/
bool HomiserviTemplate::hasCover(QETProject *project)
{
	for (Diagram *diagram : project->diagrams())
		for (QGraphicsItem *item : diagram->items())
			if (item->type() == IndependentTextItem::Type
			    && static_cast<IndependentTextItem *>(item)->uuid() == coverUuid(Title))
				return true;
	return false;
}

/**
	@return the query of the component list: one row per component (an
	element with a label, the others are wiring symbols) with
	its symbol, its position (folio.rowcolumn, as the cross references),
	its function or description, the supplier and its code, the internal
	code, the application and the variant (ProjectVariants, standard when
	not marked)
*/
QString HomiserviTemplate::componentListQuery()
{
	return QStringLiteral("SELECT label AS \"%1\","
			      " diagram_position || '.' || position AS \"%2\","
			      " COALESCE(NULLIF(description, ''), NULLIF(function, ''), designation) AS \"%3\","
			      " manufacturer_reference AS \"%4\","
			      " manufacturer AS \"%5\","
			      " machine_manufacturer_reference AS \"%6\","
			      " application AS \"%7\","
			      " COALESCE(NULLIF(variant, ''), '%8') AS \"%9\""
			      " FROM element_nomenclature_view"
			      " WHERE element_type IN ('%10', '%11', '%12') AND COALESCE(label, '') <> ''"
			      " ORDER BY label")
			.arg(tr("Símbolo"), tr("Posição esquema"), tr("Função / descrição do componente"),
			     tr("Código fornecedor"), tr("Fornecedor"), tr("Código interno"),
			     tr("Aplicação"), ProjectVariants::Standard, tr("Tipo"),
			     ElementData::typeToString(ElementData::Simple),
			     ElementData::typeToString(ElementData::Master),
			     ElementData::typeToString(ElementData::Terminal));
}

/**
	@brief HomiserviTemplate::addComponentList
	Add at the end of project a folio with the component list, continued
	on new folios when it does not fit.
*/
void HomiserviTemplate::addComponentList(QETProject *project)
{
	Diagram *diagram = project->addNewDiagram();
	setTitle(diagram, tr("Lista de componentes"));
	auto query = new ElementQueryWidget();
	query->setCustomQuery(componentListQuery());
	addTable(diagram, query, tr("Lista de componentes"), true);
	addVariantLegend(diagram);
}
