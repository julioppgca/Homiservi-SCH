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
#ifndef POTENTIALRULES_H
#define POTENTIALRULES_H

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>

class ConductorProperties;
class QDomElement;

/**
	@brief The PotentialRules namespace
	Colour and section of the wires by potential: "voltage 24VCC is blue
	0,75 mm2", "function PE is green-yellow". The first rule whose field
	matches a wire gives it its colour and its section. The rules are saved
	in the project, so they travel with the drawing.

	When automatic, a rule applies as soon as an edit makes a wire match
	it (its voltage or function typed in the conductor panel or in the
	wiring editor); a colour or section typed in the same edit wins.
	apply() applies them to a wire whatever it has.
*/
namespace PotentialRules
{
	enum Field { Voltage, Function, Number, Bus, Cable };

	struct Rule
	{
		Field field = Voltage;
			///Text the field must match: case and spaces ignored, * and ?
			///as wildcards
		QString pattern;
			///A wire colour text (BU, GNYE, azul...), empty to leave it
		QString color;
			///A wire section text (0,75...), empty to leave it
		QString section;

		bool operator==(const Rule &other) const {
			return field == other.field && pattern == other.pattern
					&& color == other.color && section == other.section;
		}
	};

		///What the label at each end of a wire (its ferrule) says
	enum EndLabels {
			///The terminal of that end: -K1A1 at the end in -K1:A1
		OwnTerminal,
			///The terminal at the other end of the wire
		Destination,
			///The number of the wire, the same at both ends
		WireNumber,
			///The number and the terminal of that end: 101 -K1A1
		NumberAndTerminal
	};

	struct Settings
	{
		bool automatic = true;
		QList<Rule> rules;
			///Homiservi-SCH: the rule of the wire end labels, see
			///ConductorEndLabeling. Kept here with the other wiring rules
			///of the project.
		EndLabels end_labels = OwnTerminal;

		bool operator==(const Settings &other) const {
			return automatic == other.automatic && rules == other.rules
					&& end_labels == other.end_labels;
		}
		bool operator!=(const Settings &other) const { return !(*this == other); }
	};

	QString fieldName(Field field);
	QString endLabelsName(EndLabels rule);
	QString fieldValue(const ConductorProperties &properties, Field field);
	Settings examples();

	Settings fromXml(const QDomElement &project_root);
	void toXml(const Settings &settings, QDomElement &project_root);

	int matchingRule(const Settings &settings, const ConductorProperties &properties);
	bool apply(const Settings &settings, ConductorProperties &properties);
	bool applyOnEdit(const Settings &settings, const ConductorProperties &before,
			 ConductorProperties &after);
	QStringList legend(const Settings &settings);

	QList<QColor> drawingColors(const QString &text);
	QString colorName(const QString &text);
	QString sectionText(const QString &section);
	void setWireColor(ConductorProperties &properties, const QString &text);
}

#endif // POTENTIALRULES_H
