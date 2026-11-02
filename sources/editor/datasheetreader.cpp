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
#include "datasheetreader.h"

#include "../qetinformation.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>

#ifdef QET_HAS_QTPDF
#include <QPdfDocument>
#include <QPdfSelection>
#endif

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("DatasheetReader", text);
	}

	const QString api_key_setting = QStringLiteral("homiservi/anthropic_api_key");
	const QString model = QStringLiteral("claude-opus-5-5");

		///Manufacturers found by name when the datasheet does not label it
	const QStringList known_manufacturers = {
		"Siemens", "Schneider Electric", "Schneider", "WEG", "ABB", "Phoenix Contact",
		"Omron", "Eaton", "Finder", "Weidmüller", "Weidmuller", "Allen-Bradley", "Rockwell",
		"Mitsubishi", "FANUC", "Festo", "SMC", "SICK", "Balluff", "ifm", "Pepperl+Fuchs",
		"Legrand", "Steck", "Metaltex", "Lovato", "Wago", "Murrelektronik", "Turck",
		"Telemecanique", "Delta", "Danfoss", "Yaskawa", "Beckhoff", "Lenze", "Pilz",
		"Banner", "Autonics", "Hager", "Soprano", "Margirius", "Novus", "Coel"};

	QStringList lines(const QString &text)
	{
		QStringList list;
		for (QString line : text.split(QLatin1Char('\n'))) {
			line = line.simplified();
			if (!line.isEmpty())
				list << line;
		}
		return list;
	}

		///The value written after one of labels, on its line or the next one
	QString labelled(const QStringList &text, const QString &labels)
	{
		const QRegularExpression label(QStringLiteral("^(?:%1)\\b[\\s:.\\-–]*(.*)$").arg(labels),
					       QRegularExpression::CaseInsensitiveOption);
		for (int i = 0; i < text.size(); ++i) {
			const QRegularExpressionMatch match = label.match(text.at(i));
			if (!match.hasMatch())
				continue;
			const QString value = match.captured(1).trimmed();
			if (!value.isEmpty())
				return value.left(80);
			if (i + 1 < text.size())
				return text.at(i + 1).left(80);
		}
		return QString();
	}

		///The first match of value on a line holding one of labels, else anywhere
	QString near(const QStringList &text, const QString &labels, const QRegularExpression &value)
	{
		const QRegularExpression label(labels, QRegularExpression::CaseInsensitiveOption);
		for (int i = 0; i < text.size(); ++i) {
			if (!label.match(text.at(i)).hasMatch())
				continue;
				//On the line of the label, or the next one for the tables
			for (int j = i; j < qMin(i + 2, int(text.size())); ++j) {
				const QRegularExpressionMatch match = value.match(text.at(j));
				if (match.hasMatch())
					return match.captured(0).simplified();
			}
		}
		return QString();
	}

	ModuleGenerator::Side sideOf(const QString &function)
	{
		static const QRegularExpression output(
			QStringLiteral("\\b(out|output|outputs|saída|saida|ausgang|sortie|relay contact|contato)\\b"),
			QRegularExpression::CaseInsensitiveOption);
		return output.match(function).hasMatch() ? ModuleGenerator::Right : ModuleGenerator::Left;
	}

		///Pins: "13  NO contact" lines in the part about the connections
	QList<DatasheetReader::Pin> pins(const QStringList &text)
	{
		static const QRegularExpression heading(
			QStringLiteral("pin ?(assignment|out|configuration|description)|terminal (assignment|"
				       "description)|connection|wiring|pinagem|liga[çc][ãa]o|conex[õo]es|"
				       "anschlussbelegung|bornes"),
			QRegularExpression::CaseInsensitiveOption);
		static const QRegularExpression pin(
			QStringLiteral("^(?:pin|pino|terminal|borne)?\\s*"
				       "((?:[A-Z]{1,3}\\d{0,3}[+-]?)|(?:\\d{1,3}[A-Z]?)|L\\+|L-|\\+V|-V|0V)"
				       "\\s+[-–:=]?\\s*([A-Za-zÀ-ú+].{2,60})$"));
		QList<DatasheetReader::Pin> list;
		QSet<QString> names;
		for (int i = 0; i < text.size(); ++i) {
			if (!heading.match(text.at(i)).hasMatch())
				continue;
			int misses = 0;
			for (int j = i + 1; j < text.size() && misses < 4 && list.size() < 64; ++j) {
				const QRegularExpressionMatch match = pin.match(text.at(j));
				if (!match.hasMatch()) {
					++misses;
					continue;
				}
				misses = 0;
				const QString name = match.captured(1);
				if (names.contains(name))
					continue;
				names.insert(name);
				DatasheetReader::Pin p;
				p.name = name;
				p.function = match.captured(2).trimmed();
				p.side = sideOf(p.function);
				list << p;
			}
			if (list.size() >= 2)
				break;
		}
		return list.size() >= 2 ? list : QList<DatasheetReader::Pin>();
	}

	QJsonObject stringProperty(const QString &description)
	{
		return {{"type", "string"}, {"description", description}};
	}

	QJsonObject schema()
	{
		QJsonObject pin_properties {
			{"name", stringProperty("Pin or terminal name exactly as marked on the product")},
			{"function", stringProperty("Short function, in Brazilian Portuguese")},
			{"side", QJsonObject {{"type", "string"},
					      {"enum", QJsonArray {"left", "right", "top", "bottom"}}}}};
		QJsonObject pin {{"type", "object"},
				 {"properties", pin_properties},
				 {"required", QJsonArray {"name", "function", "side"}},
				 {"additionalProperties", false}};

		QJsonObject properties {
			{"name", stringProperty("Short name of the component, in Brazilian Portuguese")},
			{"pins", QJsonObject {{"type", "array"}, {"items", pin}}}};
		QJsonArray required {"name", "pins"};
		for (const QString &key : DatasheetReader::infoKeys()) {
			properties.insert(key, stringProperty(QETInformation::translatedInfoKey(key)));
			required << key;
		}
		return {{"type", "object"},
			{"properties", properties},
			{"required", required},
			{"additionalProperties", false}};
	}

	const char *prompt =
		"Este é o datasheet de um componente elétrico. Extraia os dados do componente para o"
		" símbolo dele num esquema elétrico (QElectroTech).\n"
		"- Use só o que está no datasheet. Quando um dado não aparece, deixe a string vazia:"
		" nunca invente.\n"
		"- Se o datasheet cobre uma família de modelos, use o primeiro código de pedido em"
		" manufacturer_reference e cite a família em description.\n"
		"- Textos descritivos (name, designation, description, category, function) em português"
		" do Brasil; códigos, valores e nomes de pinos exatamente como no datasheet.\n"
		"- voltage_rating e current_rating com a unidade, ex.: \"24 V CC\", \"230 V CA\", \"10 A\".\n"
		"- width, height e depth: só o número em milímetros.\n"
		"- pins: os pontos de ligação elétrica externos (bornes, pinos de conector, terminais de"
		" bobina e de contatos), na ordem do datasheet. side: left para alimentação e entradas,"
		" right para saídas; top e bottom quando o componente tem a entrada em cima e a saída"
		" embaixo (contatores, disjuntores, relés de sobrecarga). Sem pinos identificáveis,"
		" lista vazia.";
}

	///The information of the element read from a datasheet
QStringList DatasheetReader::infoKeys()
{
	return {QETInformation::ELMT_DESIGNATION, QETInformation::ELMT_DESCRIPTION,
		QETInformation::ELMT_MANUFACTURER, QETInformation::ELMT_MANUFACTURER_REF,
		QETInformation::ELMT_MODEL, QETInformation::ELMT_CATEGORY,
		QETInformation::ELMT_VOLTAGE_RATING, QETInformation::ELMT_CURRENT_RATING,
		QETInformation::ELMT_WIDTH, QETInformation::ELMT_HEIGHT, QETInformation::ELMT_DEPTH};
}

	///The datasheet as an URL: a link, or a local file
QUrl DatasheetReader::url(const QString &datasheet)
{
	return isLink(datasheet) ? QUrl(datasheet.trimmed()) : QUrl::fromLocalFile(datasheet.trimmed());
}

bool DatasheetReader::isLink(const QString &datasheet)
{
	return datasheet.trimmed().startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
			|| datasheet.trimmed().startsWith(QLatin1String("https://"), Qt::CaseInsensitive);
}

/**
	@return the text of the PDF file, page after page, or an empty
	string and error set
*/
QString DatasheetReader::pdfText(const QString &file, QString *error)
{
#ifdef QET_HAS_QTPDF
	QPdfDocument document;
	if (document.load(file) != QPdfDocument::Error::None) {
		if (error)
			*error = tr("Não foi possível abrir o PDF %1.").arg(QFileInfo(file).fileName());
		return QString();
	}
	QStringList pages;
	for (int page = 0; page < document.pageCount(); ++page)
		pages << document.getAllText(page).text();
	const QString text = pages.join(QLatin1Char('\n'));
	if (text.trimmed().isEmpty() && error)
		*error = tr("O PDF não tem texto (é uma imagem escaneada): use a leitura por IA.");
	return text;
#else
	Q_UNUSED(file)
	if (error)
		*error = tr("Esta versão foi compilada sem o módulo Qt Pdf: use a leitura por IA.");
	return QString();
#endif
}

/**
	@brief DatasheetReader::readByRules
	Look in text for the usual labels of a datasheet ("Manufacturer",
	"Rated voltage", "Fabricante"...) and for a pin table. Offline, but
	only right on tidy datasheets.
*/
DatasheetReader::Result DatasheetReader::readByRules(const QString &text)
{
	const QStringList all = lines(text);
	Result result;
	auto set = [&result](const QString &key, const QString &value) {
		if (!value.trimmed().isEmpty())
			result.info.insert(key, value.trimmed());
	};

	QString manufacturer = labelled(all, QStringLiteral("manufacturer|fabricante|hersteller|fabricant"));
	if (manufacturer.isEmpty()) {
		for (const QString &name : known_manufacturers)
			if (text.contains(QRegularExpression(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(name)),
							     QRegularExpression::CaseInsensitiveOption))) {
				manufacturer = name;
				break;
			}
	}
	set(QETInformation::ELMT_MANUFACTURER, manufacturer);
	set(QETInformation::ELMT_MANUFACTURER_REF,
	    labelled(all, QStringLiteral("order(?:ing)? (?:number|no\\.?|code)|part (?:number|no\\.?)|"
					 "article (?:number|no\\.?)|catalog(?:ue)? (?:number|no\\.?)|"
					 "c[óo]digo(?: do produto| de pedido)?|refer[êe]ncia|bestellnummer|"
					 "artikelnummer|r[ée]f[ée]rence")));
	set(QETInformation::ELMT_MODEL, labelled(all, QStringLiteral("model|modelo|type|tipo|typ")));
	set(QETInformation::ELMT_DESIGNATION,
	    labelled(all, QStringLiteral("product (?:name|description)|designa[çc][ãa]o|description|descri[çc][ãa]o")));

	static const QRegularExpression voltage(
		QStringLiteral("\\d+(?:[.,]\\d+)?(?:\\s*(?:\\.\\.\\.|…|-|/)\\s*\\d+(?:[.,]\\d+)?)?\\s*V"
			       "(?:\\s*(?:AC|DC|CA|CC|AC/DC|CA/CC|~|=))?\\b"));
	static const QRegularExpression current(QStringLiteral("\\d+(?:[.,]\\d+)?\\s*m?A\\b"));
	set(QETInformation::ELMT_VOLTAGE_RATING,
	    near(all, QStringLiteral("voltage|tens[ãa]o|spannung|tension"), voltage));
	set(QETInformation::ELMT_CURRENT_RATING,
	    near(all, QStringLiteral("current|corrente|strom|courant|\\bIe\\b|\\bIn\\b"), current));

	static const QRegularExpression size(
		QStringLiteral("(\\d+(?:[.,]\\d+)?)\\s*[x×]\\s*(\\d+(?:[.,]\\d+)?)\\s*[x×]\\s*(\\d+(?:[.,]\\d+)?)\\s*mm"),
		QRegularExpression::CaseInsensitiveOption);
	const QRegularExpressionMatch dimensions = size.match(text);
	if (dimensions.hasMatch()) {
		set(QETInformation::ELMT_WIDTH, dimensions.captured(1));
		set(QETInformation::ELMT_HEIGHT, dimensions.captured(2));
		set(QETInformation::ELMT_DEPTH, dimensions.captured(3));
	}

	result.pins = pins(all);
	result.name = result.info.value(QETInformation::ELMT_DESIGNATION);
	return result;
}

	///The key of the Claude API: ANTHROPIC_API_KEY, else the one saved here
QString DatasheetReader::apiKey()
{
	const QString environment = qEnvironmentVariable("ANTHROPIC_API_KEY");
	if (!environment.isEmpty())
		return environment;
	return QSettings().value(api_key_setting).toString();
}

bool DatasheetReader::apiKeyFromEnvironment()
{
	return !qEnvironmentVariable("ANTHROPIC_API_KEY").isEmpty();
}

void DatasheetReader::setApiKey(const QString &key)
{
	if (key.trimmed().isEmpty())
		QSettings().remove(api_key_setting);
	else
		QSettings().setValue(api_key_setting, key.trimmed());
}

QNetworkRequest DatasheetReader::aiRequest()
{
	QNetworkRequest request(QUrl(QStringLiteral("https://api.anthropic.com/v1/messages")));
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	request.setRawHeader("x-api-key", apiKey().toUtf8());
	request.setRawHeader("anthropic-version", "2026-10-21");
		//A declined request is run again on the fallback model Anthropic picks
	request.setRawHeader("anthropic-beta", "server-side-fallback-2026-07-01");
		//A long datasheet takes a while to read
	request.setTransferTimeout(5 * 60 * 1000);
	return request;
}

/**
	@return the body of the request asking Claude to read pdf, its answer
	constrained to the JSON of schema()
*/
QByteArray DatasheetReader::aiRequestBody(const QByteArray &pdf)
{
	const QJsonObject document {
		{"type", "document"},
		{"source", QJsonObject {{"type", "base64"},
					{"media_type", "application/pdf"},
					{"data", QString::fromLatin1(pdf.toBase64())}}}};
	return aiBody(document, QString::fromUtf8(prompt), schema());
}

/**
	@return the body of a request asking Claude about source (a document
	or image content block) with prompt, its answer constrained to the
	JSON of schema
*/
QByteArray DatasheetReader::aiBody(const QJsonObject &source, const QString &prompt, const QJsonObject &schema)
{
	const QJsonObject question {{"type", "text"}, {"text", prompt}};
	const QJsonObject message {{"role", "user"}, {"content", QJsonArray {source, question}}};
	const QJsonObject body {
		{"model", model},
		{"max_tokens", 16000},
		{"fallbacks", "default"},
		{"output_config", QJsonObject {{"effort", "medium"},
					       {"format", QJsonObject {{"type", "json_schema"},
								       {"schema", schema}}}}},
		{"messages", QJsonArray {message}}};
	return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

/**
	@return the JSON Claude answered with, or an empty object and error set
	when the request failed, was declined or was cut short
*/
QJsonObject DatasheetReader::aiData(const QByteArray &reply, QString *error)
{
	const QJsonObject root = QJsonDocument::fromJson(reply).object();
	if (root.value("type").toString() == QLatin1String("error")) {
		if (error)
			*error = root.value("error").toObject().value("message").toString();
		return {};
	}
	const QString stop = root.value("stop_reason").toString();
	if (stop == QLatin1String("refusal")) {
		if (error)
			*error = tr("A IA recusou ler este arquivo.");
		return {};
	}
	if (stop == QLatin1String("max_tokens")) {
		if (error)
			*error = tr("A resposta da IA ficou incompleta: o componente tem pinos demais.");
		return {};
	}

	QString text;
	for (const QJsonValue &block : root.value("content").toArray())
		if (block.toObject().value("type").toString() == QLatin1String("text"))
			text += block.toObject().value("text").toString();
	const QJsonObject data = QJsonDocument::fromJson(text.toUtf8()).object();
	if (data.isEmpty() && error)
		*error = tr("A resposta da IA não pôde ser lida.");
	return data;
}

/**
	@return what Claude read in the datasheet, or an empty result and
	error set
*/
DatasheetReader::Result DatasheetReader::fromAiReply(const QByteArray &reply, QString *error)
{
	Result result;
	const QJsonObject data = aiData(reply, error);
	if (data.isEmpty())
		return result;

	result.name = data.value("name").toString().trimmed();
	for (const QString &key : infoKeys()) {
		const QString value = data.value(key).toString().trimmed();
		if (!value.isEmpty())
			result.info.insert(key, value);
	}
	const QMap<QString, ModuleGenerator::Side> sides {
		{"left", ModuleGenerator::Left}, {"right", ModuleGenerator::Right},
		{"top", ModuleGenerator::Top}, {"bottom", ModuleGenerator::Bottom}};
	for (const QJsonValue &value : data.value("pins").toArray()) {
		const QJsonObject object = value.toObject();
		Pin pin;
		pin.name = object.value("name").toString().trimmed();
		pin.function = object.value("function").toString().trimmed();
		pin.side = sides.value(object.value("side").toString(), ModuleGenerator::Left);
		if (!pin.name.isEmpty())
			result.pins << pin;
	}
	return result;
}

QString DatasheetReader::sideName(ModuleGenerator::Side side)
{
	switch (side) {
		case ModuleGenerator::Right: return tr("Direita");
		case ModuleGenerator::Top: return tr("Em cima");
		case ModuleGenerator::Bottom: return tr("Embaixo");
		default: return tr("Esquerda");
	}
}

	///The box of the component with pins on their sides and body_text inside
ModuleGenerator::Settings DatasheetReader::drawing(const QList<Pin> &pins, const QString &body_text)
{
	ModuleGenerator::Settings settings;
	for (const Pin &pin : pins) {
		ModuleGenerator::SidePins &side = settings.sides[pin.side];
		side.names << pin.name;
		side.count = int(side.names.size());
	}
	settings.body_text = body_text;
	return settings;
}
