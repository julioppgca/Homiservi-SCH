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
#include "potentialrules.h"

#include "conductorproperties.h"

#include <QCoreApplication>
#include <QDomElement>
#include <QRegularExpression>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("PotentialRules", text);
	}

	struct NamedColor
	{
		QStringList names;	///IEC 60757 code first, then the names
		const char *display;
		QColor color;
	};

	const QList<NamedColor> &namedColors()
	{
		static const QList<NamedColor> colors = {
			{{"BK", "PRETO", "BLACK"},              QT_TRANSLATE_NOOP("PotentialRules", "Preto"),       Qt::black},
			{{"BN", "MARROM", "BROWN"},             QT_TRANSLATE_NOOP("PotentialRules", "Marrom"),      QColor(139, 69, 19)},
			{{"RD", "VERMELHO", "RED"},             QT_TRANSLATE_NOOP("PotentialRules", "Vermelho"),    Qt::red},
			{{"OG", "LARANJA", "ORANGE"},           QT_TRANSLATE_NOOP("PotentialRules", "Laranja"),     QColor(255, 140, 0)},
			{{"YE", "AMARELO", "YELLOW"},           QT_TRANSLATE_NOOP("PotentialRules", "Amarelo"),     QColor(230, 200, 0)},
			{{"GN", "VERDE", "GREEN"},              QT_TRANSLATE_NOOP("PotentialRules", "Verde"),       QColor(0, 150, 0)},
			{{"BU", "AZUL", "BLUE"},                QT_TRANSLATE_NOOP("PotentialRules", "Azul"),        Qt::blue},
			{{"LBU", "AZUL CLARO", "AZUL-CLARO", "LIGHT BLUE"},
								QT_TRANSLATE_NOOP("PotentialRules", "Azul claro"),  QColor(0, 160, 230)},
			{{"VT", "VI", "VIOLETA", "ROXO", "VIOLET"}, QT_TRANSLATE_NOOP("PotentialRules", "Violeta"), QColor(128, 0, 160)},
			{{"GY", "GR", "CINZA", "GREY", "GRAY"}, QT_TRANSLATE_NOOP("PotentialRules", "Cinza"),       Qt::gray},
			{{"WH", "BRANCO", "WHITE"},             QT_TRANSLATE_NOOP("PotentialRules", "Branco"),      QColor(200, 200, 200)},
			{{"PK", "ROSA", "PINK"},                QT_TRANSLATE_NOOP("PotentialRules", "Rosa"),        QColor(255, 105, 180)},
			{{"TQ", "TURQUESA", "TURQUOISE"},       QT_TRANSLATE_NOOP("PotentialRules", "Turquesa"),    QColor(0, 180, 180)},
			{{"GD", "DOURADO", "GOLD"},             QT_TRANSLATE_NOOP("PotentialRules", "Dourado"),     QColor(212, 175, 55)},
			{{"SR", "PRATA", "SILVER"},             QT_TRANSLATE_NOOP("PotentialRules", "Prata"),       QColor(170, 170, 170)},
		};
		return colors;
	}

	bool isGreenYellow(const QString &key)
	{
		static const QStringList names = {"GNYE", "GN-YE", "GN/YE", "PE",
			"VERDE-AMARELO", "VERDE/AMARELO", "VERDE AMARELO", "GREEN-YELLOW"};
		return names.contains(key);
	}

	const NamedColor *namedColor(const QString &key)
	{
		for (const NamedColor &color : namedColors())
			if (color.names.contains(key))
				return &color;
		return nullptr;
	}

	QString fieldKey(PotentialRules::Field field)
	{
		switch (field) {
			case PotentialRules::Voltage:  return QStringLiteral("voltage");
			case PotentialRules::Function: return QStringLiteral("function");
			case PotentialRules::Number:   return QStringLiteral("number");
			case PotentialRules::Bus:      return QStringLiteral("bus");
			case PotentialRules::Cable:    return QStringLiteral("cable");
		}
		return QString();
	}

		///Case and spaces ignored: "24 VCC" is "24vcc"
	QString normalized(const QString &text)
	{
		QString result = text.toUpper();
		result.remove(QLatin1Char(' '));
		return result;
	}

	bool matches(const PotentialRules::Rule &rule, const ConductorProperties &properties)
	{
		const QString pattern = normalized(rule.pattern);
		if (pattern.isEmpty()) return false;
		const QString value = normalized(PotentialRules::fieldValue(properties, rule.field));
		if (value.isEmpty()) return false;
		const QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(
			pattern, QRegularExpression::NonPathWildcardConversion));
		return rx.match(value).hasMatch();
	}

		///A plain number is in mm2
	QString sectionText(const QString &section)
	{
		static const QRegularExpression number(QStringLiteral("^\\d+([.,]\\d+)?$"));
		return number.match(section.trimmed()).hasMatch()
				? section.trimmed() + QStringLiteral(" mm²") : section.trimmed();
	}
}

QString PotentialRules::fieldName(Field field)
{
	switch (field) {
		case Voltage:  return tr("Tensão / protocolo");
		case Function: return tr("Função");
		case Number:   return tr("Número");
		case Bus:      return tr("Barramento");
		case Cable:    return tr("Cabo");
	}
	return QString();
}

QString PotentialRules::fieldValue(const ConductorProperties &properties, Field field)
{
	switch (field) {
		case Voltage:  return properties.m_tension_protocol;
		case Function: return properties.m_function;
		case Number:   return properties.text;
		case Bus:      return properties.m_bus;
		case Cable:    return properties.m_cable;
	}
	return QString();
}

/**
	@brief PotentialRules::examples
	A starting point after IEC 60204-1 and NBR 5410: power phases black,
	neutral light blue, PE green-yellow, 24 V DC control blue.
*/
PotentialRules::Settings PotentialRules::examples()
{
	Settings settings;
	settings.rules = {
		{Function, QStringLiteral("PE"), QStringLiteral("GNYE"), QStringLiteral("2,5")},
		{Function, QStringLiteral("N"), QStringLiteral("LBU"), QStringLiteral("2,5")},
		{Function, QStringLiteral("L?"), QStringLiteral("BK"), QStringLiteral("2,5")},
		{Voltage, QStringLiteral("24VCC"), QStringLiteral("BU"), QStringLiteral("0,75")},
		{Voltage, QStringLiteral("0VCC"), QStringLiteral("BU"), QStringLiteral("0,75")},
	};
	return settings;
}

/**
	@brief PotentialRules::fromXml
	@return the rules of the <potential_rules> child of project_root, none
	if it has no such child
*/
PotentialRules::Settings PotentialRules::fromXml(const QDomElement &project_root)
{
	Settings settings;
	const QDomElement root = project_root.firstChildElement(QStringLiteral("potential_rules"));
	if (root.isNull()) return settings;
	settings.automatic = root.attribute(QStringLiteral("automatic"), QStringLiteral("true"))
			== QLatin1String("true");
	for (QDomElement e = root.firstChildElement(QStringLiteral("rule")); !e.isNull();
	     e = e.nextSiblingElement(QStringLiteral("rule"))) {
		Rule rule;
		const QString field = e.attribute(QStringLiteral("field"));
		for (Field f : {Voltage, Function, Number, Bus, Cable})
			if (fieldKey(f) == field) rule.field = f;
		rule.pattern = e.attribute(QStringLiteral("match"));
		rule.color = e.attribute(QStringLiteral("color"));
		rule.section = e.attribute(QStringLiteral("section"));
		settings.rules << rule;
	}
	return settings;
}

/**
	@brief PotentialRules::toXml
	Write settings as a <potential_rules> child of project_root, only when
	there is a rule: a project without rules saves as before.
*/
void PotentialRules::toXml(const Settings &settings, QDomElement &project_root)
{
	if (settings.rules.isEmpty()) return;
	QDomDocument document = project_root.ownerDocument();
	QDomElement root = document.createElement(QStringLiteral("potential_rules"));
	root.setAttribute(QStringLiteral("automatic"),
			  settings.automatic ? QStringLiteral("true") : QStringLiteral("false"));
	for (const Rule &rule : settings.rules) {
		QDomElement e = document.createElement(QStringLiteral("rule"));
		e.setAttribute(QStringLiteral("field"), fieldKey(rule.field));
		e.setAttribute(QStringLiteral("match"), rule.pattern);
		e.setAttribute(QStringLiteral("color"), rule.color);
		e.setAttribute(QStringLiteral("section"), rule.section);
		root.appendChild(e);
	}
	project_root.appendChild(root);
}

/**
	@return the index of the first rule matching properties, -1 if none
*/
int PotentialRules::matchingRule(const Settings &settings, const ConductorProperties &properties)
{
	for (int i = 0; i < settings.rules.size(); ++i)
		if (matches(settings.rules.at(i), properties))
			return i;
	return -1;
}

/**
	@brief PotentialRules::apply
	Give properties the colour and section of the first rule it matches.
	@return true if properties changed
*/
bool PotentialRules::apply(const Settings &settings, ConductorProperties &properties)
{
	const int index = matchingRule(settings, properties);
	if (index < 0) return false;
	const ConductorProperties before = properties;
	const Rule &rule = settings.rules.at(index);
	if (!rule.color.isEmpty()) setWireColor(properties, rule.color);
	if (!rule.section.isEmpty()) properties.m_wire_section = rule.section;
	return properties != before;
}

/**
	@brief PotentialRules::applyOnEdit
	When settings are automatic and an edit from before to after made the
	wire match another rule, give after that rule's colour and section,
	unless the edit typed them itself.
	@return true if after changed
*/
bool PotentialRules::applyOnEdit(const Settings &settings, const ConductorProperties &before,
				 ConductorProperties &after)
{
	if (!settings.automatic) return false;
	const int index = matchingRule(settings, after);
	if (index < 0 || index == matchingRule(settings, before)) return false;
	const ConductorProperties edited = after;
	const Rule &rule = settings.rules.at(index);
	if (!rule.color.isEmpty() && before.m_wire_color == after.m_wire_color
	    && before.color == after.color)
		setWireColor(after, rule.color);
	if (!rule.section.isEmpty() && before.m_wire_section == after.m_wire_section)
		after.m_wire_section = rule.section;
	return after != edited;
}

/**
	@return one line per rule: "Tensão 24VCC: Azul (BU), 0,75 mm²"
*/
QStringList PotentialRules::legend(const Settings &settings)
{
	QStringList lines;
	for (const Rule &rule : settings.rules) {
		if (rule.pattern.isEmpty()) continue;
		QStringList parts;
		if (!rule.color.isEmpty()) parts << colorName(rule.color);
		if (!rule.section.isEmpty()) parts << sectionText(rule.section);
		if (parts.isEmpty()) continue;
		lines << QStringLiteral("%1 %2: %3").arg(fieldName(rule.field), rule.pattern,
							 parts.join(QStringLiteral(", ")));
	}
	return lines;
}

/**
	The drawing colour(s) named by a wire colour text: an IEC 60757 code
	(BK, BU, GNYE...), a Portuguese or English name, or #rrggbb. Two
	colours make a bicolour wire (green-yellow). Empty if the text names no
	colour.
*/
QList<QColor> PotentialRules::drawingColors(const QString &text)
{
	const QString key = text.trimmed().toUpper();
	if (key.isEmpty()) return {};
	if (isGreenYellow(key))
		return {QColor(0, 150, 0), QColor(230, 200, 0)};
	if (const NamedColor *color = namedColor(key))
		return {color->color};
	const QColor color = QColor::fromString(text.trimmed());
	if (color.isValid()) return {color};
	return {};
}

/**
	@return text spelled out for a legend: "Azul (BU)", or text itself if
	it names no known colour
*/
QString PotentialRules::colorName(const QString &text)
{
	const QString key = text.trimmed().toUpper();
	if (isGreenYellow(key))
		return tr("Verde-amarelo (GNYE)");
	if (const NamedColor *color = namedColor(key))
		return QStringLiteral("%1 (%2)").arg(tr(color->display), color->names.first());
	return text.trimmed();
}

/**
	Set the wire colour text of properties; a colour it names is also how
	the wire is drawn. A text naming no colour leaves the drawing as it is.
*/
void PotentialRules::setWireColor(ConductorProperties &properties, const QString &text)
{
	properties.m_wire_color = text;
	const QList<QColor> colors = drawingColors(text);
	if (colors.size() == 1) {
		properties.color = colors.first();
		properties.m_bicolor = false;
	} else if (colors.size() == 2) {
		properties.color = colors.first();
		properties.m_color_2 = colors.last();
		properties.m_bicolor = true;
	}
}
