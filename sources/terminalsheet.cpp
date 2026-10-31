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
#include "terminalsheet.h"

#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "TerminalStrip/terminalstripbridge.h"
#include "diagram.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"
#include "sheettable.h"

#include <QCollator>
#include <QCoreApplication>
#include <QHash>
#include <QSet>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("TerminalSheet", text);
	}

		///The kind of the folios of the sheets (SheetTable)
	const QString folio_kind = QStringLiteral("terminal_sheet");

		///Columns of the table: potential, terminal, jumper, devices, location
	QList<SheetTable::Column> columns()
	{
		return {{tr("POTENCIAL"), 150}, {tr("BORNE"), 60, true}, {tr("JUMPER"), 64, true},
			{tr("DISPOSITIVO"), 200}, {tr("LOCALIZAÇÃO"), 104, true}};
	}
	constexpr int jumper_column = 2;
		///The title above the table and the space under it
	constexpr qreal title_height = 54;

	bool isReport(const Element *element)
	{
		return element->linkType() & Element::AllReport;
	}

		///"1L1 - 220V~": function and voltage of the first wire which has one,
		///else its text (the wire number)
	QString potential(Element *element)
	{
		QString text;
		for (Conductor *conductor : element->conductors())
		{
			const ConductorProperties properties = conductor->properties();
			QStringList parts;
			if (!properties.m_function.isEmpty()) parts << properties.m_function;
			if (!properties.m_tension_protocol.isEmpty()) parts << properties.m_tension_protocol;
			if (!parts.isEmpty())
				return parts.join(QStringLiteral(" - "));
				//"_" is the text of a wire without number
			if (text.isEmpty() && properties.text != QLatin1String("_"))
				text = properties.text;
		}
		return text;
	}

		///"-QM141 :1, -K12 :A1": the other ends of the wires of element
	QString devices(Element *element)
	{
		QStringList list;
		for (Conductor *conductor : element->conductors())
		{
			Terminal *other = conductor->terminal1->parentElement() == element
					? conductor->terminal2 : conductor->terminal1;
			Element *other_element = other ? other->parentElement() : nullptr;
			if (!other_element || other_element == element || isReport(other_element))
				continue;
			QString device = other_element->actualLabel();
			if (device.isEmpty())
				device = other_element->name();
			if (!other->name().isEmpty())
				device += QStringLiteral(" :") + other->name();
			if (!list.contains(device))
				list << device;
		}
		return list.join(QStringLiteral(", "));
	}

	TerminalSheet::Row row(Element *element, const QString &terminal)
	{
		TerminalSheet::Row row;
		row.potential = potential(element);
		row.terminal = terminal;
		row.devices = devices(element);
		row.location = SheetTable::location(element);
		return row;
	}

		///The rows of a strip of the terminal strip editor, in its order
	TerminalSheet::Strip fromStrip(TerminalStrip *strip, QSet<Element *> &done)
	{
		TerminalSheet::Strip sheet;
		sheet.name = strip->name();
		QHash<TerminalStripBridge *, int> bridges;
		for (const auto &physical : strip->physicalTerminal())
		{
			const int levels = physical->levelCount();
			for (const auto &real : physical->realTerminals())
			{
				QString name = QString::number(physical->pos() + 1);
				if (levels > 1)
					name += QChar('A' + physical->levelOf(real));
				Element *element = real->element();
				TerminalSheet::Row current;
				if (element) {
					current = row(element, name);
					done.insert(element);
				} else {
					current.terminal = name;
				}
				if (const auto bridge = real->bridge()) {
					if (!bridges.contains(bridge.data()))
						bridges.insert(bridge.data(), int(bridges.size()));
					current.bridge = bridges.value(bridge.data());
				}
				sheet.rows << current;
			}
		}
		return sheet;
	}

		///Draw rows of strip on diagram, under its title
	void drawPage(Diagram *diagram, const QString &strip, const QList<TerminalSheet::Row> &rows)
	{
		const QList<SheetTable::Column> table = columns();
		const QRectF inside = diagram->border_and_titleblock.insideBorderRect();
		const qreal left = inside.center().x() - SheetTable::width(table) / 2;
		const qreal top = inside.top() + 20;
		SheetTable::addText(diagram, strip, left, top, 26, 12, true);
		SheetTable::addText(diagram, tr("LISTA PLANILHA BORNE"), left + 180, top, 26, 11);

		QList<QStringList> values;
		for (const TerminalSheet::Row &row : rows)
			values << QStringList {row.potential, row.terminal, QString(), row.devices, row.location};
		const QList<qreal> tops = SheetTable::draw(diagram, left, top + title_height - 20, table, values);

			//The jumpers: a dot per terminal, a line from the first to the last
		qreal jumper_x = left + table.at(jumper_column).width / 2;
		for (int c = 0; c < jumper_column; ++c)
			jumper_x += table.at(c).width;
		QHash<int, QPointF> first_dot;
		QHash<int, QPointF> last_dot;
		for (int i = 0; i < rows.size(); ++i)
		{
			const int bridge = rows.at(i).bridge;
			if (bridge < 0)
				continue;
			const QPointF centre(jumper_x, tops.at(i) + SheetTable::RowHeight / 2);
			SheetTable::addDot(diagram, centre);
			if (!first_dot.contains(bridge))
				first_dot.insert(bridge, centre);
			last_dot.insert(bridge, centre);
		}
		for (auto it = first_dot.cbegin(); it != first_dot.cend(); ++it)
			if (last_dot.value(it.key()) != it.value())
				SheetTable::addLine(diagram, it.value(), last_dot.value(it.key()));
	}
}

/**
	@return the terminal strips of project with their rows: the strips of
	the terminal strip editor, then the terminal elements in none of them
	grouped by the part of their label before ':'
*/
QList<TerminalSheet::Strip> TerminalSheet::collect(QETProject *project)
{
	QList<Strip> strips;
	QSet<Element *> done;
	for (TerminalStrip *strip : project->terminalStrip())
		strips << fromStrip(strip, done);

	QCollator collator;
	collator.setNumericMode(true);
	QMap<QString, QList<QPair<QString, Element *>>> loose;
	for (Diagram *diagram : project->diagrams())
		for (QGraphicsItem *item : diagram->items())
		{
			Element *element = qgraphicsitem_cast<Element *>(item);
			if (!element || element->linkType() != Element::Terminale || done.contains(element))
				continue;
			const QString label = element->actualLabel();
			const int colon = label.indexOf(QLatin1Char(':'));
			const QString strip = colon > 0 ? label.left(colon) : tr("Bornes sem régua");
			const QString terminal = colon > 0 ? label.mid(colon + 1) : label;
			loose[strip] << qMakePair(terminal, element);
		}
	QStringList names = loose.keys();
	std::sort(names.begin(), names.end(),
		  [&collator](const QString &a, const QString &b) { return collator.compare(a, b) < 0; });
	for (const QString &name : names)
	{
		auto terminals = loose.value(name);
		std::stable_sort(terminals.begin(), terminals.end(),
				 [&collator](const auto &a, const auto &b) { return collator.compare(a.first, b.first) < 0; });
		Strip strip;
		strip.name = name;
		for (const auto &terminal : terminals)
			strip.rows << row(terminal.second, terminal.first);
		strips << strip;
	}
	return strips;
}

	///@return the folios of project made by createFolios()
QList<Diagram *> TerminalSheet::generatedFolios(QETProject *project)
{
	return SheetTable::folios(project, folio_kind);
}

/**
	@brief TerminalSheet::createFolios
	Draw the sheet of each strip at the end of project, on as many folios
	as its rows need.
	@return the number of folios added
*/
int TerminalSheet::createFolios(QETProject *project, const QList<Strip> &strips)
{
	int count = 0;
	for (const Strip &strip : strips)
	{
		int start = 0;
		do
		{
			Diagram *diagram = SheetTable::addFolio(project, tr("Planilha de bornes %1").arg(strip.name),
								folio_kind);
			const QRectF inside = diagram->border_and_titleblock.insideBorderRect();
			const int fit = qMax(1, int((inside.height() - title_height - SheetTable::HeaderHeight - 10)
						    / SheetTable::RowHeight));
			drawPage(diagram, strip.name, strip.rows.mid(start, fit));
			start += fit;
			++count;
		} while (start < strip.rows.size());
	}
	return count;
}
