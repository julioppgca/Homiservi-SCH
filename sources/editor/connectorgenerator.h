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
#ifndef CONNECTORGENERATOR_H
#define CONNECTORGENERATOR_H

#include <QDomDocument>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

/**
	@brief The ConnectorGenerator namespace
	Draws a connector as an element: the symbol of the diagrams (a box
	with one terminal per pin, its number outside and its signal inside,
	on the 10 px grid) and, beside it, the view of its face (the shell
	and its pins or sockets, numbered), seen from the mating side or from
	the solder side.

	The variants know how their pins are numbered: D-sub (standard and
	high density), modular jacks (RJ45, RJ11, RJ12), USB, and a generic
	grid. Some have the signals of a public standard (RS-232, VGA,
	Ethernet and its T568A/B colours, USB); the user fills the others.
*/
namespace ConnectorGenerator
{
	enum Family {DSub, Modular, Usb, Generic, FamilyCount};

		///How the pins of the symbol leave the box
	enum PinSide {Left, Right, BothSides};

		///How the pins of a generic connector are numbered
	enum Numbering {ByRow, ByColumn, LetterNumber};

	struct Pin
	{
		QString id;
			///Where the pin is on the face seen from the mating side of
			///the male, in pitches
		QPointF position;
	};

	struct Variant
	{
		QString id;
		Family family = Generic;
		QString name;
			///The signal presets the variant has, first being none
		QStringList presets;
	};

	struct Settings
	{
		QString variant = QStringLiteral("db9");
		bool male = true;
		bool symbol = true;
		bool front_view = true;
			///Face seen from the solder (back) side instead of the mating side
		bool solder_side = false;
		PinSide side = Left;
			///Index in Variant::presets, 0 for none
		int preset = 0;
			///RJ45: 0 no colours, 1 T568A, 2 T568B
		int colours = 0;
			///Signal of each pin, in the order of pins()
		QStringList signal_names;
			///Generic connector
		int rows = 2;
		int columns = 5;
		Numbering numbering = ByRow;
		QString prefix;
		int first = 1;

		QJsonObject toJson() const;
		static Settings fromJson(const QJsonObject &json);
	};

	QString familyName(Family family);
	QList<Variant> variants();
	const Variant *variant(const QString &id);
	QList<Pin> pins(const Settings &settings);
	QStringList presetSignals(const Settings &settings);
	QString defaultName(const Settings &settings);
	QDomDocument build(const Settings &settings);
}

#endif // CONNECTORGENERATOR_H
