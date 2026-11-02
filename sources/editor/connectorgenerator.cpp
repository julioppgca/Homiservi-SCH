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
#include "connectorgenerator.h"

#include "../qetapp.h"
#include "elementtextstyles.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QJsonArray>
#include <QPolygonF>
#include <QRectF>
#include <QUuid>
#include <QtMath>

#include <algorithm>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("ConnectorGenerator", text);
	}

	const QString line_style = QStringLiteral("line-style:normal;line-weight:normal;filling:none;color:black");
	const QString thin_style = QStringLiteral("line-style:normal;line-weight:thin;filling:none;color:black");
	const QString filled_style = QStringLiteral("line-style:normal;line-weight:thin;filling:black;color:black");

		///The drawing pitch of the face views: one pin step
	const qreal pitch = 10;

	QString number(qreal value)
	{
		return QString::number(qRound(value * 100.0) / 100.0);
	}

	qreal ceilToGrid(qreal value)
	{
		return std::ceil(value / 10.0 - 1e-6) * 10.0;
	}

	class Writer
	{
		public:
			Writer()
			{
				QDomElement definition = m_document.createElement(QStringLiteral("definition"));
				definition.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
				m_document.appendChild(definition);
				m_description = m_document.createElement(QStringLiteral("description"));
				definition.appendChild(m_description);
			}

			void line(qreal x1, qreal y1, qreal x2, qreal y2, const QString &style = line_style)
			{
				QDomElement e = shape(QStringLiteral("line"), style);
				e.setAttribute(QStringLiteral("x1"), number(x1));
				e.setAttribute(QStringLiteral("y1"), number(y1));
				e.setAttribute(QStringLiteral("x2"), number(x2));
				e.setAttribute(QStringLiteral("y2"), number(y2));
				e.setAttribute(QStringLiteral("end1"), QStringLiteral("none"));
				e.setAttribute(QStringLiteral("end2"), QStringLiteral("none"));
				e.setAttribute(QStringLiteral("length1"), QStringLiteral("1.5"));
				e.setAttribute(QStringLiteral("length2"), QStringLiteral("1.5"));
			}

			void rect(const QRectF &r, qreal radius = 0, const QString &style = line_style)
			{
				QDomElement e = shape(QStringLiteral("rect"), style);
				e.setAttribute(QStringLiteral("x"), number(r.x()));
				e.setAttribute(QStringLiteral("y"), number(r.y()));
				e.setAttribute(QStringLiteral("width"), number(r.width()));
				e.setAttribute(QStringLiteral("height"), number(r.height()));
				e.setAttribute(QStringLiteral("rx"), number(radius));
				e.setAttribute(QStringLiteral("ry"), number(radius));
			}

			void circle(const QPointF &centre, qreal radius, const QString &style = line_style)
			{
				QDomElement e = shape(QStringLiteral("circle"), style);
				e.setAttribute(QStringLiteral("x"), number(centre.x() - radius));
				e.setAttribute(QStringLiteral("y"), number(centre.y() - radius));
				e.setAttribute(QStringLiteral("diameter"), number(2 * radius));
			}

			void polygon(const QPolygonF &points, const QString &style = line_style)
			{
				QDomElement e = shape(QStringLiteral("polygon"), style);
				for (int i = 0; i < points.size(); ++i) {
					e.setAttribute(QStringLiteral("x%1").arg(i + 1), number(points.at(i).x()));
					e.setAttribute(QStringLiteral("y%1").arg(i + 1), number(points.at(i).y()));
				}
			}

				///A text whose baseline starts at (x, y); align: -1 left, 0 centre, 1 right
			void text(const QString &text, qreal x, qreal y, qreal size, int align = -1)
			{
				this->text(text, x, y, TextStyles::Style {QString(), QETApp::diagramTextsFont(size), Qt::black}, align);
			}

				///A text in a style of the element editor, which it names
			void text(const QString &text, qreal x, qreal y, const TextStyles::Style &style, int align = -1)
			{
				if (text.isEmpty())
					return;
				const QFont &font = style.font;
				const qreal width = QFontMetricsF(font).horizontalAdvance(text);
				QDomElement e = m_document.createElement(QStringLiteral("text"));
				e.setAttribute(QStringLiteral("text"), text);
				e.setAttribute(QStringLiteral("x"), number(align < 0 ? x : align == 0 ? x - width / 2 : x - width));
				e.setAttribute(QStringLiteral("y"), number(y));
				e.setAttribute(QStringLiteral("rotation"), 0);
				e.setAttribute(QStringLiteral("font"), font.toString());
				e.setAttribute(QStringLiteral("color"), style.color.name());
				if (!style.name.isEmpty())
					e.setAttribute(QStringLiteral("element_style"), style.name);
				m_description.appendChild(e);
			}

			void terminal(const QString &name, qreal x, qreal y, const char *orientation)
			{
				QDomElement e = m_document.createElement(QStringLiteral("terminal"));
				e.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
				e.setAttribute(QStringLiteral("name"), name);
				e.setAttribute(QStringLiteral("x"), number(x));
				e.setAttribute(QStringLiteral("y"), number(y));
				e.setAttribute(QStringLiteral("orientation"), QString::fromLatin1(orientation));
				e.setAttribute(QStringLiteral("type"), QStringLiteral("Generic"));
				m_description.appendChild(e);
			}

				///The field showing the label of the element (X1...)
			void labelField(qreal x, qreal y, const QString &style)
			{
				QDomElement e = m_document.createElement(QStringLiteral("dynamic_text"));
				e.setAttribute(QStringLiteral("x"), number(x));
				e.setAttribute(QStringLiteral("y"), number(y));
				e.setAttribute(QStringLiteral("z"), 1);
				e.setAttribute(QStringLiteral("rotation"), 0);
				e.setAttribute(QStringLiteral("font_size"), 9);
				e.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
				e.setAttribute(QStringLiteral("frame"), QStringLiteral("false"));
				e.setAttribute(QStringLiteral("text_width"), -1);
				e.setAttribute(QStringLiteral("text_from"), QStringLiteral("ElementInfo"));
				if (!style.isEmpty())
					e.setAttribute(QStringLiteral("element_style"), style);
				e.appendChild(m_document.createElement(QStringLiteral("text")));
				QDomElement info = m_document.createElement(QStringLiteral("info_name"));
				info.appendChild(m_document.createTextNode(QStringLiteral("label")));
				e.appendChild(info);
				m_description.appendChild(e);
			}

			QDomDocument document() const {return m_document;}

		private:
			QDomElement shape(const QString &tag, const QString &style)
			{
				QDomElement e = m_document.createElement(tag);
				e.setAttribute(QStringLiteral("style"), style);
				e.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
				m_description.appendChild(e);
				return e;
			}

			QDomDocument m_document;
			QDomElement m_description;
	};

		///The rows of the D-sub variants, top first, as seen on the male mating face
	QList<int> dsubRows(const QString &id)
	{
		static const QHash<QString, QList<int>> rows {
			{QStringLiteral("db9"), {5, 4}},     {QStringLiteral("da15"), {8, 7}},
			{QStringLiteral("db25"), {13, 12}},  {QStringLiteral("dc37"), {19, 18}},
			{QStringLiteral("dd50"), {17, 16, 17}},
			{QStringLiteral("de15hd"), {5, 5, 5}}, {QStringLiteral("da26hd"), {9, 9, 8}},
			{QStringLiteral("db44hd"), {15, 15, 14}}};
		return rows.value(id);
	}

	bool isHighDensity(const QString &id)
	{
		return id.endsWith(QLatin1String("hd")) || id == QLatin1String("dd50");
	}

		///Positions on the male mating face for a D-sub: pin 1 top left
	QList<ConnectorGenerator::Pin> dsubPins(const QString &id)
	{
		QList<ConnectorGenerator::Pin> list;
		const QList<int> rows = dsubRows(id);
		const int widest = *std::max_element(rows.cbegin(), rows.cend());
		int n = 1;
		for (int r = 0; r < rows.size(); ++r) {
				//Rows are staggered by half a pitch
			const qreal start = rows.size() == 3 ? (r == 1 ? 0.5 : 0) : (widest - rows.at(r)) / 2.0;
			for (int i = 0; i < rows.at(r); ++i)
				list << ConnectorGenerator::Pin {QString::number(n++), QPointF(start + i, r)};
		}
		return list;
	}

		///The text style of the editor at index of ElementTextStyles::examples()
		///(the name of a terminal, the text of the box, the label), size points
		///when the user removed or renamed it
	TextStyles::Style textStyle(int index, qreal size)
	{
		const QString name = ElementTextStyles::examples().at(index).name;
		for (const TextStyles::Style &style : ElementTextStyles::load())
			if (style.name == name)
				return style;
		return TextStyles::Style {QString(), QETApp::diagramTextsFont(size), Qt::black};
	}
}

QJsonObject ConnectorGenerator::Settings::toJson() const
{
	return QJsonObject {
		{"variant", variant}, {"male", male}, {"symbol", symbol}, {"front_view", front_view},
		{"solder_side", solder_side}, {"side", int(side)}, {"preset", preset}, {"colours", colours},
		{"signals", QJsonArray::fromStringList(signal_names)}, {"rows", rows}, {"columns", columns},
		{"numbering", int(numbering)}, {"prefix", prefix}, {"first", first}};
}

ConnectorGenerator::Settings ConnectorGenerator::Settings::fromJson(const QJsonObject &json)
{
	Settings s;
	s.variant = json.value("variant").toString(s.variant);
	s.male = json.value("male").toBool(s.male);
	s.symbol = json.value("symbol").toBool(s.symbol);
	s.front_view = json.value("front_view").toBool(s.front_view);
	s.solder_side = json.value("solder_side").toBool(s.solder_side);
	s.side = PinSide(json.value("side").toInt(s.side));
	s.preset = json.value("preset").toInt(s.preset);
	s.colours = json.value("colours").toInt(s.colours);
	for (const QJsonValue &value : json.value("signals").toArray())
		s.signal_names << value.toString();
	s.rows = json.value("rows").toInt(s.rows);
	s.columns = json.value("columns").toInt(s.columns);
	s.numbering = Numbering(json.value("numbering").toInt(s.numbering));
	s.prefix = json.value("prefix").toString();
	s.first = json.value("first").toInt(s.first);
	return s;
}

QString ConnectorGenerator::familyName(Family family)
{
	switch (family) {
		case DSub:    return tr("D-sub (DB, HD)");
		case Modular: return tr("Modular (RJ45, RJ11, RJ12)");
		case Usb:     return tr("USB");
		case Generic: return tr("Genérico (fileiras × colunas)");
		case FamilyCount: break;
	}
	return QString();
}

QList<ConnectorGenerator::Variant> ConnectorGenerator::variants()
{
	const QString none = tr("Nenhum (preencher)");
	return {
		{QStringLiteral("db9"), DSub, QStringLiteral("DB9 (DE-9)"), {none, tr("RS-232 (DTE, como no PC)")}},
		{QStringLiteral("da15"), DSub, QStringLiteral("DA15"), {none}},
		{QStringLiteral("db25"), DSub, QStringLiteral("DB25"), {none, tr("RS-232 (DTE)")}},
		{QStringLiteral("dc37"), DSub, QStringLiteral("DC37"), {none}},
		{QStringLiteral("dd50"), DSub, QStringLiteral("DD50"), {none}},
		{QStringLiteral("de15hd"), DSub, tr("DE-15 HD (VGA)"), {none, tr("VGA")}},
		{QStringLiteral("da26hd"), DSub, QStringLiteral("DA-26 HD"), {none}},
		{QStringLiteral("db44hd"), DSub, QStringLiteral("DB-44 HD"), {none}},
		{QStringLiteral("rj45"), Modular, tr("RJ45 (8P8C)"), {none, tr("Ethernet 10/100"), tr("Ethernet Gigabit")}},
		{QStringLiteral("rj12"), Modular, tr("RJ12 (6P6C)"), {none}},
		{QStringLiteral("rj11"), Modular, tr("RJ11 (6P4C)"), {none}},
		{QStringLiteral("usb_a"), Usb, tr("USB A"), {none, tr("USB 2.0")}},
		{QStringLiteral("usb_b"), Usb, tr("USB B"), {none, tr("USB 2.0")}},
		{QStringLiteral("usb_mini"), Usb, tr("USB mini-B"), {none, tr("USB 2.0")}},
		{QStringLiteral("usb_micro"), Usb, tr("USB micro-B"), {none, tr("USB 2.0")}},
		{QStringLiteral("usb_c"), Usb, tr("USB-C (24 pinos)"), {none, tr("USB-C")}},
		{QStringLiteral("generic"), Generic, tr("Genérico"), {none}},
	};
}

const ConnectorGenerator::Variant *ConnectorGenerator::variant(const QString &id)
{
	static const QList<Variant> list = variants();
	for (const Variant &v : list)
		if (v.id == id)
			return &v;
	return nullptr;
}

/**
	@return the pins of the connector in the order of their numbers,
	placed as on the male mating face (in pitches)
*/
QList<ConnectorGenerator::Pin> ConnectorGenerator::pins(const Settings &s)
{
	const QString &id = s.variant;
	QList<Pin> list;
	if (!dsubRows(id).isEmpty())
		return dsubPins(id);
	if (id == QLatin1String("rj45") || id == QLatin1String("rj12")) {
			//Plug, contacts up, latch down: pin 1 on the left
		const int count = id == QLatin1String("rj45") ? 8 : 6;
		for (int i = 0; i < count; ++i)
			list << Pin {QString::number(i + 1), QPointF(i, 0)};
	} else if (id == QLatin1String("rj11")) {
			//The four middle positions of six
		for (int i = 0; i < 4; ++i)
			list << Pin {QString::number(i + 1), QPointF(i + 1, 0)};
	} else if (id == QLatin1String("usb_a")) {
			//Plug face, contacts up: 4 3 2 1 from the left
		for (int i = 0; i < 4; ++i)
			list << Pin {QString::number(i + 1), QPointF(3 - i, 0)};
	} else if (id == QLatin1String("usb_b")) {
			//Plug face: 1 2 over 4 3
		list << Pin {"1", QPointF(0, 0)} << Pin {"2", QPointF(1, 0)}
		     << Pin {"3", QPointF(1, 1)} << Pin {"4", QPointF(0, 1)};
	} else if (id == QLatin1String("usb_mini") || id == QLatin1String("usb_micro")) {
			//Plug face, wide side up: 5 4 3 2 1 from the left
		for (int i = 0; i < 5; ++i)
			list << Pin {QString::number(i + 1), QPointF(4 - i, 0)};
	} else if (id == QLatin1String("usb_c")) {
			//Plug face (the receptacle mirrored): A12..A1 over B1..B12
		for (int i = 0; i < 12; ++i)
			list << Pin {QStringLiteral("A%1").arg(i + 1), QPointF(11 - i, 0)};
		for (int i = 0; i < 12; ++i)
			list << Pin {QStringLiteral("B%1").arg(i + 1), QPointF(i, 1)};
	} else if (id == QLatin1String("generic")) {
		const int rows = qMax(1, s.rows), columns = qMax(1, s.columns);
		for (int i = 0; i < rows * columns; ++i) {
			const int r = s.numbering == ByColumn ? i % rows : i / columns;
			const int c = s.numbering == ByColumn ? i / rows : i % columns;
			QString name;
			if (s.numbering == LetterNumber) {
				QString letters;
				for (int n = r; n >= 0; n = n / 26 - 1)
					letters.prepend(QChar('A' + n % 26));
				name = letters + QString::number(s.first + c);
			} else {
				name = QString::number(s.first + i);
			}
			list << Pin {s.prefix + name, QPointF(c, r)};
		}
	}
	return list;
}

/**
	@return the signals of the preset and colours of s, one per pin
*/
QStringList ConnectorGenerator::presetSignals(const Settings &s)
{
	const int count = pins(s).size();
	QStringList names;
	const QString &id = s.variant;
	if (s.preset > 0) {
		if (id == QLatin1String("db9"))
			names = {"DCD", "RXD", "TXD", "DTR", "GND", "DSR", "RTS", "CTS", "RI"};
		else if (id == QLatin1String("db25")) {
			names = QStringList(25, QString());
			names[0] = QStringLiteral("SHIELD"); names[1] = QStringLiteral("TXD");
			names[2] = QStringLiteral("RXD");    names[3] = QStringLiteral("RTS");
			names[4] = QStringLiteral("CTS");    names[5] = QStringLiteral("DSR");
			names[6] = QStringLiteral("GND");    names[7] = QStringLiteral("DCD");
			names[19] = QStringLiteral("DTR");   names[21] = QStringLiteral("RI");
		} else if (id == QLatin1String("de15hd"))
			names = {"RED", "GREEN", "BLUE", "ID2", "GND", "RED_RTN", "GREEN_RTN", "BLUE_RTN",
				 "+5V", "GND", "ID0", "SDA", "HSYNC", "VSYNC", "SCL"};
		else if (id == QLatin1String("rj45") && s.preset == 1)
			names = {"TX+", "TX-", "RX+", "", "", "RX-", "", ""};
		else if (id == QLatin1String("rj45") && s.preset == 2)
			names = {"BI_DA+", "BI_DA-", "BI_DB+", "BI_DC+", "BI_DC-", "BI_DB-", "BI_DD+", "BI_DD-"};
		else if (id == QLatin1String("usb_a") || id == QLatin1String("usb_b"))
			names = {"VBUS", "D-", "D+", "GND"};
		else if (id == QLatin1String("usb_mini") || id == QLatin1String("usb_micro"))
			names = {"VBUS", "D-", "D+", "ID", "GND"};
		else if (id == QLatin1String("usb_c"))
			names = {"GND", "TX1+", "TX1-", "VBUS", "CC1", "D+", "D-", "SBU1", "VBUS", "RX2-", "RX2+", "GND",
				 "GND", "TX2+", "TX2-", "VBUS", "CC2", "D+", "D-", "SBU2", "VBUS", "RX1-", "RX1+", "GND"};
	}
	while (names.size() < count)
		names << QString();

	if (id == QLatin1String("rj45") && s.colours > 0) {
		const QStringList t568b {tr("branco-laranja"), tr("laranja"), tr("branco-verde"), tr("azul"),
					 tr("branco-azul"), tr("verde"), tr("branco-marrom"), tr("marrom")};
		QStringList colours = t568b;
		if (s.colours == 1) {
			colours.swapItemsAt(0, 2);
			colours.swapItemsAt(1, 5);
		}
		for (int i = 0; i < 8; ++i)
			names[i] = names.at(i).isEmpty() ? colours.at(i) : names.at(i) + QStringLiteral(" (") + colours.at(i) + ')';
	}
	return names.mid(0, count);
}

QString ConnectorGenerator::defaultName(const Settings &s)
{
	const Variant *v = variant(s.variant);
	QString name = v ? v->name : s.variant;
	if (s.variant == QLatin1String("generic"))
		name = tr("Conector %1 pinos").arg(pins(s).size());
	return name + (s.male ? tr(" macho") : tr(" fêmea"));
}

/**
	@brief ConnectorGenerator::build
	The symbol has its top left corner at (0, 0) and its terminals on the
	10 px grid; the face view stands 40 to the right of it, or under it
	when pins leave by the right.
*/
QDomDocument ConnectorGenerator::build(const Settings &s)
{
	Writer w;
	const QList<Pin> pin_list = pins(s);
	if (pin_list.isEmpty())
		return w.document();
	QStringList names = s.signal_names.isEmpty() ? presetSignals(s) : s.signal_names;
	while (names.size() < pin_list.size())
		names << QString();

	const TextStyles::Style number_style = textStyle(0, 7);
	const TextStyles::Style signal_style = textStyle(1, 9);
	const TextStyles::Style label_style = textStyle(2, 9);
	const QFontMetricsF signal_metrics(signal_style.font);
	const qreal half_cap = signal_metrics.capHeight() / 2;
	const QString title = defaultName(s);

	qreal view_x = 0, view_y = 10;
	if (s.symbol) {
			//Which pins leave by the left, which by the right, top to bottom
		QList<int> left, rightward;
		for (int i = 0; i < pin_list.size(); ++i) {
			if (s.side == Left || (s.side == BothSides && i < (pin_list.size() + 1) / 2))
				left << i;
			else
				rightward << i;
		}
		auto widest = [&](const QList<int> &which) {
			qreal width = 0;
			for (int i : which)
				width = qMax(width, signal_metrics.horizontalAdvance(names.at(i)));
			return width;
		};
		const qreal inner = widest(left) + widest(rightward) + (left.isEmpty() || rightward.isEmpty() ? 6 : 14);
		const qreal width = qMax(30.0, ceilToGrid(inner));
			//The leads are long enough for the numbers to stay off the terminals
		qreal number_width = 0;
		for (const Pin &pin : pin_list)
			number_width = qMax(number_width, QFontMetricsF(number_style.font).horizontalAdvance(pin.id));
		const qreal lead = qMax(10.0, ceilToGrid(number_width + 5));
		const int rows = qMax(left.size(), rightward.size());
		const qreal height = (rows + 1) * 10.0;

		w.rect(QRectF(0, 0, width, height));
		w.text(title, 0, -3, 6);
		w.labelField(0, -12 - QFontMetricsF(label_style.font).height(), label_style.name);
		for (int row = 0; row < left.size(); ++row) {
			const int i = left.at(row);
			const qreal y = (row + 1) * 10.0;
			w.line(0, y, -lead, y);
			w.terminal(pin_list.at(i).id, -lead, y, "w");
			w.text(pin_list.at(i).id, -2, y - 1.5, number_style, 1);
			w.text(names.at(i), 3, y + half_cap, signal_style);
		}
		for (int row = 0; row < rightward.size(); ++row) {
			const int i = rightward.at(row);
			const qreal y = (row + 1) * 10.0;
			w.line(width, y, width + lead, y);
			w.terminal(pin_list.at(i).id, width + lead, y, "e");
			w.text(pin_list.at(i).id, width + 2, y - 1.5, number_style);
			w.text(names.at(i), width - 3, y + half_cap, signal_style, 1);
		}
			//The face view beside the symbol, or under it when conductors
			//come from the right
		if (rightward.isEmpty())
			view_x = width + lead + 40;
		else
			view_y = height + 50;
	}

	if (s.front_view) {
			//On the female mating face and the male solder face, the pins
			//are mirrored left to right
		const bool mirrored = s.male == s.solder_side;
		qreal max_x = 0, max_y = 0;
		for (const Pin &pin : pin_list) {
			max_x = qMax(max_x, pin.position.x());
			max_y = qMax(max_y, pin.position.y());
		}
		const QString &id = s.variant;
			//RJ11 has its four contacts in the middle of six places
		if (id == QLatin1String("rj11"))
			max_x = 5;
		const bool dsub = !dsubRows(id).isEmpty();
		const Variant *v = variant(id);
		const Family family = v ? v->family : Generic;
		const bool contacts = family == Modular || family == Usb;
		const qreal step = dsub && isHighDensity(id) ? pitch * 0.82 : pitch;
			//Flat contacts in two rows (USB B and C) need room between them
		const qreal row_step = contacts && max_y > 0 ? 2 * step : step;
		const qreal span_x = max_x * step, span_y = max_y * row_step;
		const qreal ox = view_x + (dsub ? (dsubRows(id).size() == 3 ? 4.5 : 3.5) * step : 2 * step), oy = view_y;
		auto place = [&](const QPointF &p) {
			return QPointF(ox + (mirrored ? max_x - p.x() : p.x()) * step, oy + p.y() * row_step);
		};
			//The lowest point drawn, for the caption under it
		qreal bottom = oy + span_y + step;

		if (dsub) {
				//The D shell: wide at the top, narrower at the bottom,
				//whichever face is seen
			const qreal margin = step * 0.9;
			const qreal top = oy - margin, low = oy + span_y + margin;
				//Room for the numbers at the ends of a middle row
			const qreal side = margin + (dsubRows(id).size() == 3 ? step : 0);
			const qreal left_top = ox - side, right_top = ox + span_x + side;
			const qreal slant = (low - top) * 0.18;
			w.polygon(QPolygonF({{left_top, top}, {right_top, top}, {right_top - slant, low},
					     {left_top + slant, low}}));
				//The flange and its two screws
			const qreal flange = 2.6 * step;
			w.rect(QRectF(left_top - flange, top - step * 0.7, right_top - left_top + 2 * flange,
				      low - top + step * 1.4), 1.5, thin_style);
			w.circle(QPointF(left_top - flange / 2, (top + low) / 2), step * 0.55, thin_style);
			w.circle(QPointF(right_top + flange / 2, (top + low) / 2), step * 0.55, thin_style);
			bottom = low + step * 0.7;
		} else if (family == Modular) {
			const qreal positions = id == QLatin1String("rj45") ? 8 : 6;
			const qreal width = (positions + 1) * step;
			const qreal x0 = ox - step;
			w.rect(QRectF(x0, oy - step * 1.5, width, step * 3.5));
				//The latch, down
			w.rect(QRectF(x0 + width / 2 - step * 1.4, oy + step * 2, step * 2.8, step * 1.2), 0, thin_style);
			bottom = oy + step * 3.2;
		} else if (id == QLatin1String("usb_c")) {
			w.rect(QRectF(ox - step * 1.2, oy - step * 1.2, span_x + step * 2.4, span_y + step * 2.4), step * 1.1);
				//The tongue between the two rows
			w.rect(QRectF(ox - step * 0.6, oy + span_y / 2 - step * 0.3, span_x + step * 1.2, step * 0.6), 0, thin_style);
			bottom = oy + span_y + step * 1.2;
		} else if (id == QLatin1String("usb_b")) {
				//Square with the two top corners cut
			const qreal x0 = ox - step * 1.2, x1 = ox + span_x + step * 1.2;
			const qreal y0 = oy - step * 1.4, y1 = oy + span_y + step * 1.4;
			w.polygon(QPolygonF({{x0, y0 + step * 0.7}, {x0 + step * 0.7, y0}, {x1 - step * 0.7, y0},
					     {x1, y0 + step * 0.7}, {x1, y1}, {x0, y1}}));
			bottom = y1;
		} else if (id == QLatin1String("usb_a")) {
			w.rect(QRectF(ox - step * 1.5, oy - step * 1.6, span_x + step * 3, step * 3.4));
			w.rect(QRectF(ox - step, oy + step * 0.6, span_x + step * 2, step * 0.8), 0, thin_style);
			bottom = oy + step * 1.8;
		} else if (family == Usb) {
				//Mini and micro: the trapeze
			const qreal width = span_x + step * 2.4;
			const qreal x0 = ox - step * 1.2, y0 = oy - step * 1.4;
			w.polygon(QPolygonF({{x0, y0}, {x0 + width, y0}, {x0 + width, y0 + step * 1.8},
					     {x0 + width - step * 0.8, y0 + step * 2.6}, {x0 + step * 0.8, y0 + step * 2.6},
					     {x0, y0 + step * 1.8}}));
			bottom = y0 + step * 2.6;
		} else {
			w.rect(QRectF(ox - step * 0.9, oy - step * 0.9, span_x + step * 1.8, span_y + step * 1.8), 2);
			bottom = oy + span_y + step * 0.9;
		}

			//The pins (filled) or sockets (hollow), numbered above them
			//on the top half, under them on the bottom half; on the
			//middle row, too tight, only its ends are, outside
		const qreal radius = qMin(step * 0.28, 2.6);
		const qreal half = contacts ? step * 0.4 : radius;
		const TextStyles::Style small {QString(), QETApp::diagramTextsFont(4), Qt::black};
		const qreal cap = QFontMetricsF(small.font).capHeight();
		int middle_first = -1, middle_last = -1;
		for (int i = 0; i < pin_list.size(); ++i) {
			if (max_y > 0 && qAbs(pin_list.at(i).position.y() - max_y / 2) < 0.01) {
				if (middle_first < 0)
					middle_first = i;
				middle_last = i;
			}
		}
		for (const Pin &pin : pin_list) {
			const QPointF c = place(pin.position);
			if (contacts)
				w.rect(QRectF(c.x() - step * 0.18, c.y() - half, step * 0.36, 2 * half), 0,
				       s.male ? filled_style : thin_style);
			else
				w.circle(c, radius, s.male ? filled_style : thin_style);
			const qreal y = pin.position.y();
			if (max_y == 0 || y < max_y / 2 - 0.01)
				w.text(pin.id, c.x(), c.y() - half - 1, small, 0);
			else if (y > max_y / 2 + 0.01)
				w.text(pin.id, c.x(), c.y() + half + 1 + cap, small, 0);
			else if (&pin == &pin_list.at(middle_first) || &pin == &pin_list.at(middle_last)) {
				const bool left_end = (&pin == &pin_list.at(middle_first)) != mirrored;
				w.text(pin.id, c.x() + (left_end ? -1 : 1) * (radius + 0.6), c.y() + cap / 2, small, left_end ? 1 : -1);
			}
		}

		const QString view = s.solder_side ? tr("vista da solda") : tr("vista do encaixe");
		w.text(title + QStringLiteral(" – ") + view, ox - step, bottom + 9, 6);
	}
	return w.document();
}
