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
#include "qetapp.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetgraphicsitem/qetshapeitem.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"
#include "textstyles.h"

#include <QCollator>
#include <QCoreApplication>
#include <QHash>
#include <QSet>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("TerminalSheet", text);
	}

		///The title block variable marking the folios of the sheets
	const QString marker_key = QStringLiteral("homiservi_generated");
	const QString marker_value = QStringLiteral("terminal_sheet");

		///Columns of the table: potential, terminal, jumper, devices, location
	const qreal column_widths[] = {150, 60, 64, 200, 104};
		///The columns whose values are centred: terminal, jumper, location
	const bool column_centred[] = {false, true, true, false, true};
	constexpr int column_count = 5;
	constexpr qreal row_height = 16;
	constexpr qreal header_height = 22;

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

	QString location(Element *element)
	{
		Diagram *diagram = element->diagram();
		if (!diagram)
			return QString();
		return QStringLiteral("%1.%2").arg(diagram->project()->folioIndex(diagram) + 1)
				.arg(diagram->convertPosition(element->scenePos()).toString());
	}

	TerminalSheet::Row row(Element *element, const QString &terminal)
	{
		TerminalSheet::Row row;
		row.potential = potential(element);
		row.terminal = terminal;
		row.devices = devices(element);
		row.location = location(element);
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

		///The text of a table cell at x, centred in its height, and in width
		///when width is given
	void addText(Diagram *diagram, const QString &value, qreal x, qreal y, qreal height,
		     int size = 7, bool bold = false, qreal width = 0)
	{
		if (value.isEmpty())
			return;
		auto text = new IndependentTextItem();
		QFont font = QETApp::diagramTextsFont(size);
		font.setBold(bold);
		text->setFont(font);
		text->setTextStyle(TextStyles::NoStyle);
		text->setPlainText(value);
		diagram->addItem(text);
		const QRectF rect = text->boundingRect();
		text->setPos(width > 0 ? x + (width - rect.width()) / 2 : x,
			     y + (height - rect.height()) / 2);
	}

	void addLine(Diagram *diagram, const QPointF &p1, const QPointF &p2)
	{
		auto line = new QetShapeItem(p1, p2, QetShapeItem::Line);
		diagram->addItem(line);
	}

	void addDot(Diagram *diagram, const QPointF &centre)
	{
		auto dot = new QetShapeItem(centre - QPointF(3, 3), centre + QPointF(3, 3), QetShapeItem::Ellipse);
		dot->setBrush(QBrush(Qt::black));
		diagram->addItem(dot);
	}

		///Draw rows of strip on diagram, the header first
	void drawPage(Diagram *diagram, const QString &strip, const QList<TerminalSheet::Row> &rows)
	{
		const QRectF inside = diagram->border_and_titleblock.insideBorderRect();
		qreal width = 0;
		for (qreal w : column_widths) width += w;
		const qreal left = inside.center().x() - width / 2;
		qreal y = inside.top() + 20;

		addText(diagram, strip, left, y, 26, 12, true);
		addText(diagram, tr("LISTA PLANILHA BORNE"), left + 180, y, 26, 11);
		y += 34;

		qreal x = left;
		const QStringList headers {tr("POTENCIAL"), tr("BORNE"), tr("JUMPER"), tr("DISPOSITIVO"),
					   tr("LOCALIZAÇÃO")};
		const qreal table_top = y;
		for (int c = 0; c < column_count; ++c) {
			addText(diagram, headers.at(c), x, y, header_height, 7, true, column_widths[c]);
			x += column_widths[c];
		}
		y += header_height;
		addLine(diagram, QPointF(left, table_top), QPointF(left + width, table_top));
		addLine(diagram, QPointF(left, y), QPointF(left + width, y));

		const qreal jumper_x = left + column_widths[0] + column_widths[1] + column_widths[2] / 2;
		QHash<int, QPointF> first_dot;
		QHash<int, QPointF> last_dot;
		for (const TerminalSheet::Row &row : rows)
		{
			x = left;
			const QStringList values {row.potential, row.terminal, QString(), row.devices, row.location};
			for (int c = 0; c < column_count; ++c) {
				if (column_centred[c])
					addText(diagram, values.at(c), x, y, row_height, 7, false, column_widths[c]);
				else
					addText(diagram, values.at(c), x + 4, y, row_height);
				x += column_widths[c];
			}
			if (row.bridge >= 0) {
				const QPointF centre(jumper_x, y + row_height / 2);
				addDot(diagram, centre);
				if (!first_dot.contains(row.bridge))
					first_dot.insert(row.bridge, centre);
				last_dot.insert(row.bridge, centre);
			}
			y += row_height;
			addLine(diagram, QPointF(left, y), QPointF(left + width, y));
		}
		for (auto it = first_dot.cbegin(); it != first_dot.cend(); ++it)
			if (last_dot.value(it.key()) != it.value())
				addLine(diagram, it.value(), last_dot.value(it.key()));

		x = left;
		for (int c = 0; c <= column_count; ++c) {
			addLine(diagram, QPointF(x, table_top), QPointF(x, y));
			if (c < column_count)
				x += column_widths[c];
		}
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
	QList<Diagram *> folios;
	for (Diagram *diagram : project->diagrams())
		if (diagram->border_and_titleblock.exportTitleBlock().context.value(marker_key).toString()
		    == marker_value)
			folios << diagram;
	return folios;
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
			Diagram *diagram = project->addNewDiagram();
			TitleBlockProperties title_block = diagram->border_and_titleblock.exportTitleBlock();
			title_block.title = tr("Planilha de bornes %1").arg(strip.name);
			title_block.context.addValue(marker_key, marker_value);
			diagram->border_and_titleblock.importTitleBlock(title_block);

			const QRectF inside = diagram->border_and_titleblock.insideBorderRect();
			const int fit = qMax(1, int((inside.height() - 20 - 34 - header_height - 10) / row_height));
			drawPage(diagram, strip.name, strip.rows.mid(start, fit));
			start += fit;
			++count;
		} while (start < strip.rows.size());
	}
	return count;
}
