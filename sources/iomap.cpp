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
#include "iomap.h"

#include "diagram.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"
#include "sheettable.h"

#include <QCollator>
#include <QCoreApplication>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("IoMap", text);
	}

		///The kind of the folios of the maps (SheetTable)
	const QString folio_kind = QStringLiteral("io_map");

	QList<SheetTable::Column> columns()
	{
		return {{tr("PINO"), 44, true}, {tr("ENDEREÇO"), 70, true}, {tr("LOCALIZAÇÃO"), 84, true},
			{tr("DESCRIÇÃO"), 290}};
	}
		///Space between the input and the output tables
	constexpr qreal gap = 20;
		///The title above the tables and the space under it
	constexpr qreal title_height = 70;

	bool isInput(ElementData::PlcIOType type)
	{
		return type == ElementData::EntreeDigitale || type == ElementData::EntreeAnalogique
				|| type == ElementData::EntreeUniverselle;
	}

	bool isAnalog(ElementData::PlcIOType type)
	{
		return type == ElementData::EntreeAnalogique || type == ElementData::SortieAnalogique;
	}

	IoMap::Module module(Element *plc)
	{
		const ElementData data = plc->elementData();
		IoMap::Module module;
		module.label = plc->actualLabel();
		module.description = plc->elementInformations().value(QStringLiteral("description")).toString();
		if (module.description.isEmpty())
			module.description = plc->name();

			//The contacts drawn for each input or output, by its index
		QMap<int, QStringList> locations;
		for (Element *slave : plc->linkedElements())
		{
			const int index = plc->groupIndexForElement(slave);
			if (index >= 0)
				locations[index] << SheetTable::location(slave);
		}

		const QVector<ElementData::PlcIO> ios = data.plcMasterData().ios;
		for (int i = 0; i < ios.size(); ++i)
		{
			const ElementData::PlcIO &plc_io = ios.at(i);
			IoMap::Io io;
				//The pins the user wrote, not the T1, T2... standing for them
			io.pins = plc_io.terminals.join(QStringLiteral(", "));
			io.address = plc_io.address;
			io.location = locations.value(i).join(QStringLiteral(", "));
			QStringList description;
			if (isAnalog(plc_io.type))
				description << tr("(analógica)");
			if (plc_io.functionText.isEmpty() && plc_io.comment.isEmpty())
				description << tr("RESERVA");
			if (!plc_io.functionText.isEmpty())
				description << plc_io.functionText;
			if (!plc_io.comment.isEmpty())
				description << QStringLiteral("– ") + plc_io.comment;
			io.description = description.join(QLatin1Char(' '));
			(isInput(plc_io.type) ? module.inputs : module.outputs) << io;
		}
		return module;
	}

	QList<QStringList> values(const QList<IoMap::Io> &ios)
	{
		QList<QStringList> rows;
		for (const IoMap::Io &io : ios)
			rows << QStringList {io.pins, io.address, io.location, io.description};
		return rows;
	}

	void drawPage(Diagram *diagram, const IoMap::Module &module,
		      const QList<IoMap::Io> &inputs, const QList<IoMap::Io> &outputs)
	{
		const QList<SheetTable::Column> table = columns();
		const qreal table_width = SheetTable::width(table);
		const QRectF inside = diagram->border_and_titleblock.insideBorderRect();
		const qreal left = inside.center().x() - table_width - gap / 2;
		const qreal right = left + table_width + gap;
		qreal top = inside.top() + 20;

		SheetTable::addText(diagram, tr("MAPA DE ENTRADAS E SAÍDAS"), left, top, 26, 12, true);
		SheetTable::addText(diagram, QStringLiteral("%1   %2").arg(module.label, module.description),
				    right, top, 26, 11);
		top += 34;
		SheetTable::addText(diagram, tr("ENTRADAS"), left, top, 16, 8, true);
		SheetTable::addText(diagram, tr("SAÍDAS"), right, top, 16, 8, true);
		top += title_height - 54;
		if (!inputs.isEmpty())
			SheetTable::draw(diagram, left, top, table, values(inputs));
		if (!outputs.isEmpty())
			SheetTable::draw(diagram, right, top, table, values(outputs));
	}
}

/**
	@return the PLCs of project, in the order of their labels, with their
	inputs and outputs
*/
QList<IoMap::Module> IoMap::collect(QETProject *project)
{
	QList<Module> modules;
	for (Diagram *diagram : project->diagrams())
		for (QGraphicsItem *item : diagram->items())
		{
			Element *element = qgraphicsitem_cast<Element *>(item);
			if (!element || element->linkType() != Element::Master)
				continue;
			const ElementData data = element->elementData();
			if (data.m_master_type == ElementData::PLC && !data.plcMasterData().ios.isEmpty())
				modules << module(element);
		}
	QCollator collator;
	collator.setNumericMode(true);
	std::stable_sort(modules.begin(), modules.end(), [&collator](const Module &a, const Module &b) {
		return collator.compare(a.label, b.label) < 0;
	});
	return modules;
}

	///@return the folios of project made by createFolios()
QList<Diagram *> IoMap::generatedFolios(QETProject *project)
{
	return SheetTable::folios(project, folio_kind);
}

/**
	@brief IoMap::createFolios
	Draw the map of each module at the end of project, on as many folios
	as its inputs or its outputs need.
	@return the number of folios added
*/
int IoMap::createFolios(QETProject *project, const QList<Module> &modules)
{
	int count = 0;
	for (const Module &module : modules)
	{
		const int rows = int(qMax(module.inputs.size(), module.outputs.size()));
		int start = 0;
		do
		{
			Diagram *diagram = SheetTable::addFolio(
						project, tr("Mapa de entradas e saídas %1").arg(module.label), folio_kind);
			const QRectF inside = diagram->border_and_titleblock.insideBorderRect();
			const int fit = qMax(1, int((inside.height() - 20 - title_height - SheetTable::HeaderHeight - 10)
						    / SheetTable::RowHeight));
			drawPage(diagram, module, module.inputs.mid(start, fit), module.outputs.mid(start, fit));
			start += fit;
			++count;
		} while (start < rows);
	}
	return count;
}
