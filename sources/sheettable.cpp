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
#include "sheettable.h"

#include "diagram.h"
#include "qetapp.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetgraphicsitem/qetshapeitem.h"
#include "qetproject.h"
#include "textstyles.h"

namespace {
		///The title block variable marking the generated folios, its value the kind
	const QString marker_key = QStringLiteral("homiservi_generated");
}

qreal SheetTable::width(const QList<Column> &columns)
{
	qreal total = 0;
	for (const Column &column : columns)
		total += column.width;
	return total;
}

/**
	@brief SheetTable::draw
	Draw at left, top the header of columns then rows, one value per
	column.
	@return the top of each row then the bottom of the table
*/
QList<qreal> SheetTable::draw(Diagram *diagram, qreal left, qreal top,
			      const QList<Column> &columns, const QList<QStringList> &rows)
{
	const qreal table_width = width(columns);
	qreal x = left;
	for (const Column &column : columns) {
		addText(diagram, column.header, x, top, HeaderHeight, 7, true, column.width);
		x += column.width;
	}
	qreal y = top + HeaderHeight;
	addLine(diagram, QPointF(left, top), QPointF(left + table_width, top));
	addLine(diagram, QPointF(left, y), QPointF(left + table_width, y));

	QList<qreal> tops;
	for (const QStringList &row : rows)
	{
		tops << y;
		x = left;
		for (int c = 0; c < columns.size(); ++c) {
			const QString value = row.value(c);
			if (columns.at(c).centred)
				addText(diagram, value, x, y, RowHeight, 7, false, columns.at(c).width);
			else
				addText(diagram, value, x + 4, y, RowHeight);
			x += columns.at(c).width;
		}
		y += RowHeight;
		addLine(diagram, QPointF(left, y), QPointF(left + table_width, y));
	}
	tops << y;

	x = left;
	addLine(diagram, QPointF(x, top), QPointF(x, y));
	for (const Column &column : columns) {
		x += column.width;
		addLine(diagram, QPointF(x, top), QPointF(x, y));
	}
	return tops;
}

	///A text at x, centred in height, and in width when width is given
void SheetTable::addText(Diagram *diagram, const QString &value, qreal x, qreal y, qreal height,
			 int size, bool bold, qreal width)
{
	if (value.isEmpty())
		return;
	auto text = new IndependentTextItem();
	QFont font = QETApp::diagramTextsFont(size);
	font.setBold(bold);
	text->setFont(font);
		//The look of the table, not the one of the free texts
	text->setTextStyle(TextStyles::NoStyle);
	text->setPlainText(value);
	diagram->addItem(text);
	const QRectF rect = text->boundingRect();
	text->setPos(width > 0 ? x + (width - rect.width()) / 2 : x,
		     y + (height - rect.height()) / 2);
}

void SheetTable::addLine(Diagram *diagram, const QPointF &p1, const QPointF &p2)
{
	diagram->addItem(new QetShapeItem(p1, p2, QetShapeItem::Line));
}

void SheetTable::addDot(Diagram *diagram, const QPointF &centre)
{
	auto dot = new QetShapeItem(centre - QPointF(3, 3), centre + QPointF(3, 3), QetShapeItem::Ellipse);
	dot->setBrush(QBrush(Qt::black));
	diagram->addItem(dot);
}

	///"15.B4": the folio and the cell where element is drawn
QString SheetTable::location(Element *element)
{
	Diagram *diagram = element->diagram();
	if (!diagram)
		return QString();
	return QStringLiteral("%1.%2").arg(diagram->project()->folioIndex(diagram) + 1)
			.arg(diagram->convertPosition(element->scenePos()).toString());
}

	///Add at the end of project a folio titled title, marked as generated of kind
Diagram *SheetTable::addFolio(QETProject *project, const QString &title, const QString &kind)
{
	Diagram *diagram = project->addNewDiagram();
	TitleBlockProperties title_block = diagram->border_and_titleblock.exportTitleBlock();
	title_block.title = title;
	title_block.context.addValue(marker_key, kind);
	diagram->border_and_titleblock.importTitleBlock(title_block);
	return diagram;
}

	///The folios of project generated of kind
QList<Diagram *> SheetTable::folios(QETProject *project, const QString &kind)
{
	QList<Diagram *> list;
	for (Diagram *diagram : project->diagrams())
		if (diagram->border_and_titleblock.exportTitleBlock().context.value(marker_key).toString() == kind)
			list << diagram;
	return list;
}
