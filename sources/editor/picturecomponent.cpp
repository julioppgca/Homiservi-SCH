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
#include "picturecomponent.h"

#include "../qetinformation.h"
#include "datasheetreader.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("PictureComponent", text);
	}

	QJsonObject stringProperty(const QString &description)
	{
		return {{"type", "string"}, {"description", description}};
	}

	QJsonObject strictObject(const QJsonObject &properties)
	{
		QJsonArray required;
		for (auto it = properties.begin(); it != properties.end(); ++it)
			required << it.key();
		return {{"type", "object"},
			{"properties", properties},
			{"required", required},
			{"additionalProperties", false}};
	}

	QJsonObject schema()
	{
		const QJsonObject pin = strictObject({
			{"name", stringProperty("Terminal name exactly as printed, e.g. A1, 13, S12")},
			{"side", QJsonObject {{"type", "string"},
					      {"enum", QJsonArray {"left", "right", "top", "bottom"}}}},
			{"position", QJsonObject {{"type", "integer"},
						  {"description", "Column (top, bottom) or row (left, right) from 0"}}},
			{"function", stringProperty("Short function, in Brazilian Portuguese")}});
		const QJsonObject part = strictObject({
			{"kind", QJsonObject {{"type", "string"},
					      {"enum", QJsonArray {"normally_open", "normally_closed", "coil"}}}},
			{"pin_a", stringProperty("Terminal at one end")},
			{"pin_b", stringProperty("Terminal at the other end")},
			{"label", stringProperty("Device the part belongs to, e.g. K1; empty when not shown")}});

		QJsonObject properties {
			{"name", stringProperty("Short name of the component, in Brazilian Portuguese")},
			{"body_text", stringProperty("Text of the box that is not a contact (the control logic), short")},
			{"pins", QJsonObject {{"type", "array"}, {"items", pin}}},
			{"parts", QJsonObject {{"type", "array"}, {"items", part}}}};
		for (const QString &key : DatasheetReader::infoKeys())
			properties.insert(key, stringProperty(QETInformation::translatedInfoKey(key)));
		return strictObject(properties);
	}

	const char *prompt =
		"Esta imagem é o diagrama de ligação de um componente elétrico (relé, módulo,"
		" contator...), como impresso no produto ou no datasheet. Extraia o que é preciso para"
		" desenhar o símbolo dele num esquema elétrico (QElectroTech).\n"
		"- pins: todos os bornes externos, com o nome exatamente como impresso. side: o lado"
		" da imagem em que o borne está (top, bottom, left, right). position: a coluna, para"
		" top e bottom, contada da esquerda a partir de 0, comum aos dois lados: um borne"
		" embaixo, alinhado sob um borne de cima, tem a mesma position que ele. Para left e"
		" right, a linha contada de cima. Bornes vizinhos têm positions seguidas, mesmo"
		" quando a imagem deixa espaço entre eles.\n"
		"- parts: os contatos e bobinas ligados diretamente entre dois bornes (ex.: 13-14"
		" normally_open, 41-42 normally_closed, A1-A2 coil só quando a bobina está mesmo entre"
		" esses bornes). Um contato formado por vários contatos em série (K1 e K2) conta como"
		" um, com label \"K1/K2\". Não liste a eletrônica interna.\n"
		"- body_text: um nome curto para a lógica interna que não é contato, em português"
		" (ex.: \"Lógica de comando\"); vazio se não houver.\n"
		"- Os outros campos só se aparecerem na imagem; senão string vazia. Nunca invente.";

		///Pins by name
	QMap<QString, PictureComponent::Pin> byName(const QList<PictureComponent::Pin> &pins)
	{
		QMap<QString, PictureComponent::Pin> map;
		for (const PictureComponent::Pin &pin : pins)
			map.insert(pin.name, pin);
		return map;
	}

		///Group positions given as coordinates: those closer than tolerance
		///to the first of a group share its index
	QList<int> groups(const QList<qreal> &values, qreal tolerance)
	{
		QList<int> order(values.size());
		for (int i = 0; i < order.size(); ++i)
			order[i] = i;
		std::sort(order.begin(), order.end(), [&](int a, int b) { return values.at(a) < values.at(b); });
		QList<int> index(values.size(), 0);
		int group = -1;
		qreal start = 0;
		for (int i : order) {
			if (group < 0 || values.at(i) - start > tolerance) {
				++group;
				start = values.at(i);
			}
			index[i] = group;
		}
		return index;
	}
}

QString PictureComponent::kindName(ModuleGenerator::SymbolKind kind)
{
	switch (kind) {
		case ModuleGenerator::NormallyClosed: return tr("Contato NF");
		case ModuleGenerator::Coil: return tr("Bobina");
		default: return tr("Contato NA");
	}
}

/**
	@brief PictureComponent::guessParts
	The contacts and coils the terminal names tell, by the numbering of
	IEC 60947 / EN 50005: the last digit 1-2 (and 5-6) is a normally
	closed contact, 3-4 (and 7-8) a normally open one, the rest of the
	name is the contact's number (13-14, 21-22, 113-114); A1-A2 is a coil.
*/
QList<PictureComponent::Part> PictureComponent::guessParts(const QList<Pin> &pins)
{
	const QMap<QString, Pin> pin = byName(pins);
	QList<Part> parts;
	static const QRegularExpression contact(QStringLiteral("^(\\d*)([1-8])$"));
	for (const Pin &p : pins) {
		const QRegularExpressionMatch match = contact.match(p.name);
		if (!match.hasMatch())
			continue;
		const int digit = match.captured(2).toInt();
		if (digit % 2 == 0)
			continue;	//the second terminal of a pair
		const QString other = match.captured(1) + QString::number(digit + 1);
		if (!pin.contains(other))
			continue;
		Part part;
		part.kind = (digit == 1 || digit == 5) ? ModuleGenerator::NormallyClosed
						       : ModuleGenerator::NormallyOpen;
		part.pin_a = p.name;
		part.pin_b = other;
		parts << part;
	}
	for (const QString &letter : {QStringLiteral("A"), QStringLiteral("B")}) {
		if (pin.contains(letter + "1") && pin.contains(letter + "2")) {
			Part part;
			part.kind = ModuleGenerator::Coil;
			part.pin_a = letter + "1";
			part.pin_b = letter + "2";
			parts << part;
		}
	}
	return parts;
}

/**
	@brief PictureComponent::placeClickedPins
	Side and position of the pins clicked on a picture of size picture:
	the top and bottom fifths of the picture are its top and bottom sides,
	the rest its left and right halves. Pins of the top and bottom sides
	closer than 2% of the width share a column, so a terminal right under
	another faces it; the same for the rows of the left and right sides.
*/
void PictureComponent::placeClickedPins(QList<Pin> &pins, const QSizeF &picture)
{
	QList<int> vertical, horizontal;
	QList<qreal> xs, ys;
	for (int i = 0; i < pins.size(); ++i) {
		Pin &pin = pins[i];
		if (pin.point.isNull())
			continue;
		const qreal x = pin.point.x(), y = pin.point.y();
		if (y < picture.height() * 0.3)
			pin.side = ModuleGenerator::Top;
		else if (y > picture.height() * 0.7)
			pin.side = ModuleGenerator::Bottom;
		else
			pin.side = x < picture.width() / 2 ? ModuleGenerator::Left : ModuleGenerator::Right;
		if (pin.side == ModuleGenerator::Top || pin.side == ModuleGenerator::Bottom) {
			vertical << i;
			xs << x;
		} else {
			horizontal << i;
			ys << y;
		}
	}
	const QList<int> columns = groups(xs, std::max<qreal>(4, picture.width() * 0.02));
	for (int i = 0; i < vertical.size(); ++i)
		pins[vertical.at(i)].position = columns.at(i);
	const QList<int> rows = groups(ys, std::max<qreal>(4, picture.height() * 0.02));
	for (int i = 0; i < horizontal.size(); ++i)
		pins[horizontal.at(i)].position = rows.at(i);
}

	///Whether part can be drawn: its two terminals face each other
bool PictureComponent::canDraw(const Part &part, const QList<Pin> &pins)
{
	const QMap<QString, Pin> pin = byName(pins);
	if (!pin.contains(part.pin_a) || !pin.contains(part.pin_b))
		return false;
	const Pin a = pin.value(part.pin_a), b = pin.value(part.pin_b);
	if (a.position != b.position)
		return false;
	const QSet<int> sides {a.side, b.side};
	return sides == QSet<int> {ModuleGenerator::Top, ModuleGenerator::Bottom}
	    || sides == QSet<int> {ModuleGenerator::Left, ModuleGenerator::Right};
}

/**
	@return the body of the request asking Claude to read image (PNG or
	JPEG bytes of type media_type)
*/
QByteArray PictureComponent::aiRequestBody(const QByteArray &image, const QString &media_type)
{
	const QJsonObject source {
		{"type", "image"},
		{"source", QJsonObject {{"type", "base64"},
					{"media_type", media_type},
					{"data", QString::fromLatin1(image.toBase64())}}}};
	return DatasheetReader::aiBody(source, QString::fromUtf8(prompt), schema());
}

PictureComponent::Result PictureComponent::fromAiReply(const QByteArray &reply, QString *error)
{
	Result result;
	const QJsonObject data = DatasheetReader::aiData(reply, error);
	if (data.isEmpty())
		return result;

	result.name = data.value("name").toString().trimmed();
	result.body_text = data.value("body_text").toString().trimmed();
	for (const QString &key : DatasheetReader::infoKeys()) {
		const QString value = data.value(key).toString().trimmed();
		if (!value.isEmpty())
			result.info.insert(key, value);
	}
	const QMap<QString, ModuleGenerator::Side> sides {
		{"left", ModuleGenerator::Left}, {"right", ModuleGenerator::Right},
		{"top", ModuleGenerator::Top}, {"bottom", ModuleGenerator::Bottom}};
	QSet<QString> names;
	for (const QJsonValue &value : data.value("pins").toArray()) {
		const QJsonObject object = value.toObject();
		Pin pin;
		pin.name = object.value("name").toString().trimmed();
		pin.side = sides.value(object.value("side").toString(), ModuleGenerator::Top);
		pin.position = std::max(0, object.value("position").toInt());
		pin.function = object.value("function").toString().trimmed();
		if (!pin.name.isEmpty() && !names.contains(pin.name)) {
			names.insert(pin.name);
			result.pins << pin;
		}
	}
	const QMap<QString, ModuleGenerator::SymbolKind> kinds {
		{"normally_open", ModuleGenerator::NormallyOpen},
		{"normally_closed", ModuleGenerator::NormallyClosed},
		{"coil", ModuleGenerator::Coil}};
	for (const QJsonValue &value : data.value("parts").toArray()) {
		const QJsonObject object = value.toObject();
		Part part;
		part.kind = kinds.value(object.value("kind").toString(), ModuleGenerator::NormallyOpen);
		part.pin_a = object.value("pin_a").toString().trimmed();
		part.pin_b = object.value("pin_b").toString().trimmed();
		part.label = object.value("label").toString().trimmed();
		if (names.contains(part.pin_a) && names.contains(part.pin_b))
			result.parts << part;
	}
	return result;
}

/**
	@brief PictureComponent::build
	The box with the terminals at their positions and, when draw_parts,
	the parts that can be drawn (canDraw()) between their terminals.
*/
QDomDocument PictureComponent::build(const Result &result, bool draw_parts)
{
	ModuleGenerator::Settings settings;
	for (const Pin &pin : result.pins) {
		ModuleGenerator::SidePins &side = settings.sides[pin.side];
		side.names << pin.name;
		side.places << pin.position;
		side.count = int(side.names.size());
	}
	settings.body_text = result.body_text;
	if (draw_parts) {
		const QMap<QString, Pin> pin = byName(result.pins);
		for (const Part &part : result.parts) {
			if (!canDraw(part, result.pins))
				continue;
			const Pin a = pin.value(part.pin_a);
			ModuleGenerator::Symbol symbol;
			symbol.kind = part.kind;
			symbol.vertical = a.side == ModuleGenerator::Top || a.side == ModuleGenerator::Bottom;
			symbol.place = a.position;
			symbol.label = part.label;
			settings.symbols << symbol;
		}
	}
	return ModuleGenerator::build(settings);
}
