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
#ifndef SHEETTABLE_H
#define SHEETTABLE_H

#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

class Diagram;
class Element;
class QETProject;

/**
	@brief The SheetTable namespace
	Draw tables with lines and texts on the folios generated from a
	project (terminal sheets, PLC input/output maps), and find those
	folios again: they are marked by a title block variable, so that
	generating them again replaces them.
*/
namespace SheetTable
{
	constexpr qreal RowHeight = 16;
	constexpr qreal HeaderHeight = 22;

	struct Column
	{
		QString header;
		qreal width;
		bool centred = false;
	};

	qreal width(const QList<Column> &columns);
	QList<qreal> draw(Diagram *diagram, qreal left, qreal top,
			  const QList<Column> &columns, const QList<QStringList> &rows);
	void addText(Diagram *diagram, const QString &value, qreal x, qreal y, qreal height,
		     int size = 7, bool bold = false, qreal width = 0);
	void addLine(Diagram *diagram, const QPointF &p1, const QPointF &p2);
	void addDot(Diagram *diagram, const QPointF &centre);

	QString location(Element *element);
	Diagram *addFolio(QETProject *project, const QString &title, const QString &kind);
	QList<Diagram *> folios(QETProject *project, const QString &kind);
}

#endif // SHEETTABLE_H
