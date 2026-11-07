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
#ifndef COMPONENTWIZARD_H
#define COMPONENTWIZARD_H

#include <QDomDocument>
#include <QString>
#include <QStringList>

class ElementData;

/**
	@brief The ComponentWizard namespace
	Draws a usual component from its kind: contactor coil, auxiliary and
	power contacts, circuit breaker, push button, pilot light, motor,
	power supply, terminal block. The terminals get their IEC markings
	(A1/A2, 13/14, 21/22, 1/2...), on the 10 px grid, and applyData() gives
	the element the matching type (master, slave, terminal...), its name
	and the prefix of its label.
*/
namespace ComponentWizard
{
	enum Kind {
		Coil,
		NoContact,
		NcContact,
		PowerContacts,
		Breaker,
		PushButtonNo,
		PushButtonNc,
		Lamp,
		Motor,
		PowerSupply,
		TerminalBlock,
		KindCount
	};

	struct Settings
	{
		Kind kind = Coil;
		int poles = 3;
			///Earth terminal of a motor or a power supply
		bool earth = true;
		bool pin_names = true;
		bool label_field = true;
		QString prefix;
		QString name;
	};

	QString kindName(Kind kind);
	QString defaultName(Kind kind, int poles);
	QString defaultPrefix(Kind kind);
	bool hasPoles(Kind kind);
	bool hasEarth(Kind kind);
	QStringList terminalNames(const Settings &settings);
	QString typeDescription(Kind kind);

	QDomDocument build(const Settings &settings);
	void applyData(const Settings &settings, ElementData &data);
}

#endif // COMPONENTWIZARD_H
