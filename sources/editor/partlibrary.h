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
#ifndef PARTLIBRARY_H
#define PARTLIBRARY_H

#include <QDomDocument>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

/**
	@brief The PartLibrary namespace
	Ready made pieces for the element editor: IEC 60617 symbols (contacts,
	coil, fuse, resistor, lamp, motor...) and shapes drawn from a few
	numbers (regular polygon, rounded rectangle, row of terminals, arrow,
	arc). build() gives the parts of one as an element definition, to
	paste in the element being edited. The symbols are drawn vertically
	between two connection points 40 apart on the diagrams' grid, with
	their terminals if asked, and can be turned horizontal.
*/
namespace PartLibrary
{
	struct Param
	{
		enum Kind {Number, Text, Choice, Check};

		QString key;
		QString label;
		Kind kind = Number;
		QVariant value;
		double minimum = 0;
		double maximum = 1000;
		double step = 1;
			///The choices of a Choice, the value being the index
		QStringList choices;
	};

	struct Entry
	{
		QString id;
		QString group;
		QString name;
		QList<Param> params;
	};

	QList<Entry> entries();
	const Entry *find(const QList<Entry> &entries, const QString &id);
	QVariantMap defaults(const Entry &entry);
	QDomDocument build(const QString &id, const QVariantMap &values);
}

#endif // PARTLIBRARY_H
