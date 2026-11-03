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

				///A text read from the bottom up, its letters centred on x,
				///starting at y
			void uprightText(const QString &text, qreal x, qreal y, const TextStyles::Style &style)
			{
				if (text.isEmpty())
					return;
				QDomElement e = m_document.createElement(QStringLiteral("text"));
				e.setAttribute(QStringLiteral("text"), text);
				e.setAttribute(QStringLiteral("x"), number(x + QFontMetricsF(style.font).capHeight() / 2));
				e.setAttribute(QStringLiteral("y"), number(y));
				e.setAttribute(QStringLiteral("rotation"), 270);
				e.setAttribute(QStringLiteral("font"), style.font.toString());
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
			{QStringLiteral("db44hd"), {15, 15, 14}},
				//Honda MR, numbered from the bottom row (FANUC manuals, Fig. A (aj))
			{QStringLiteral("mr20"), {7, 6, 7}}, {QStringLiteral("mr50"), {18, 14, 18}}};
		return rows.value(id);
	}

	bool isHondaMr(const QString &id)
	{
		return id == QLatin1String("mr20") || id == QLatin1String("mr50");
	}

		///The 20-pin interface (Honda PCR, Hirose FI40): two rows at 1.27 mm
	bool isPcr(const QString &id)
	{
		return id == QLatin1String("honda20") || id == QLatin1String("fi40_15");
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
			//The Honda MR count from the bottom row: 1 to 7 under 8 to 13
			//under 14 to 20 on the plug
		const bool from_bottom = isHondaMr(id);
		for (int k = 0; k < rows.size(); ++k) {
			const int r = from_bottom ? rows.size() - 1 - k : k;
				//Rows are staggered by half a pitch
			qreal start = rows.size() == 3 ? (r == 1 ? 0.5 : 0) : (widest - rows.at(r)) / 2.0;
			if (from_bottom && r == 1) {
				start = (widest - rows.at(r)) / 2.0;
				if (start == std::floor(start))
					start -= 0.5;
			}
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

		///A FANUC connector: its signals as the FANUC diagrams draw them
	struct FanucPreset
	{
		const char *variant;
		QString name;
			///The connector names it stands for, the first given by default
		QStringList titles;
		QList<QPair<QString, QString>> pins;
	};

		///The FANUC connectors known, checked against the diagrams of a
		///0i-TF / βi SVSP machine; the pins without signal are left out
	QList<FanucPreset> fanucPresets()
	{
		using P = QList<QPair<QString, QString>>;
		const P serial {{"1", "SIN"}, {"2", "*SIN"}, {"3", "SOUT"}, {"4", "*SOUT"},
				{"12", "0V"}, {"14", "0V"}, {"16", "0V"}};
		const P io_link {{"1", "SIN"}, {"2", "*SIN"}, {"3", "SOUT"}, {"4", "*SOUT"},
				 {"11", "0V"}, {"12", "0V"}, {"13", "0V"}, {"14", "0V"}};
		const P keys {{"1", "KEY00"}, {"11", "KEY01"}, {"2", "KEY02"}, {"12", "KEY03"},
			      {"3", "KEY04"}, {"13", "KEY05"}, {"4", "KEY06"}, {"14", "KEY07"},
			      {"5", "COM00"}, {"15", "COM01"}, {"6", "COM02"}, {"16", "COM03"},
			      {"7", "COM04"}, {"17", "COM05"}, {"8", "COM06"}, {"18", "COM07"},
			      {"9", "COM08"}, {"19", "COM09"}, {"10", "COM10"}, {"20", "COM11"}};
		const P coder {{"1", "PR"}, {"2", "*PR"}, {"5", "PA"}, {"6", "*PA"}, {"7", "PB"}, {"8", "*PB"},
			       {"9", "5V"}, {"18", "5V"}, {"20", "5V"}, {"12", "0V"}, {"14", "0V"}, {"16", "0V"}};
		return QList<FanucPreset> {
			{"honda20", tr("JD1A / JD1B – I/O Link"), {"JD1A", "JD1B"}, io_link},
			{"honda20", tr("JD51A – I/O Link i"), {"JD51A"},
			 {{"1", "SIN1"}, {"2", "*SIN1"}, {"3", "SOUT1"}, {"4", "*SOUT1"},
			  {"11", "0V"}, {"12", "0V"}, {"13", "0V"}, {"14", "0V"}}},
			{"honda20", tr("JA41, JA7A / JA7B – spindle serial"), {"JA41", "JA7A", "JA7B"}, serial},
			{"honda20,fi40_15", tr("JF1 / JF2 / JF3 – encoder do servo"), {"JF1", "JF2", "JF3"},
			 {{"5", "RD"}, {"6", "*RD"}, {"9", "5V"}, {"20", "5V"}, {"12", "0V"}, {"14", "0V"},
			  {"7", "6V"}, {"16", "FG"}}},
			{"honda20", tr("JYA2 – sensor do motor do spindle"), {"JYA2"},
			 {{"1", "MZ"}, {"2", "*MZ"}, {"5", "MA"}, {"6", "*MA"}, {"7", "MB"}, {"8", "*MB"},
			  {"9", "5V"}, {"18", "5V"}, {"20", "5V"}, {"12", "0V"}, {"14", "0V"}, {"16", "0V"},
			  {"13", "THR1"}, {"15", "THR2"}, {"10", "SS"}}},
			{"honda20", tr("JYA3 / JYA4 – position coder"), {"JYA3", "JYA4"}, coder},
			{"honda20", tr("JY1 – sinais analógicos do spindle"), {"JY1"},
			 {{"1", "OVR1"}, {"2", "OVR2"}, {"16", "LM"}, {"17", "SM"}, {"18", "OM"}, {"19", "OM"}, {"20", "0V"}}},
			{"honda20", tr("JD36A / JD36B – RS-232"), {"JD36A", "JD36B"},
			 {{"1", "RD"}, {"2", "0V"}, {"3", "DR"}, {"4", "0V"}, {"5", "CS"}, {"6", "0V"}, {"7", "CD"},
			  {"8", "0V"}, {"10", "24V"}, {"11", "SD"}, {"12", "0V"}, {"13", "ER"}, {"14", "0V"},
			  {"15", "RS"}, {"16", "0V"}, {"19", "24V"}}},
			{"honda20", tr("JA40 – entradas rápidas (HDI)"), {"JA40"},
			 {{"1", "HDI0"}, {"2", "0V"}, {"11", "HDI1"}, {"12", "0V"}, {"3", "HDI2"}, {"4", "0V"},
			  {"13", "HDI3"}, {"14", "0V"}, {"6", "HDI4"}, {"5", "0V"}, {"10", "0V"}, {"15", "HDI5"},
			  {"16", "0V"}, {"17", "HDI6"}, {"18", "0V"}, {"19", "HDI7"}, {"20", "0V"}}},
			{"honda20", tr("CNK1 – matriz de teclas do painel"), {"CNK1"}, keys},
			{"hif3ba20", tr("JA2 – teclado MDI"), {"JA2"},
			 {{"A1", "KEY00"}, {"B1", "KEY02"}, {"A2", "KEY01"}, {"B2", "KEY03"}, {"A3", "KEY04"},
			  {"B3", "KEY06"}, {"A4", "KEY05"}, {"B4", "KEY07"}, {"B5", "COM00"}, {"A5", "COM02"},
			  {"A6", "COM01"}, {"B6", "COM03"}, {"A7", "COM04"}, {"B7", "COM06"}, {"A8", "COM05"},
			  {"B8", "COM07"}, {"A9", "COM08"}, {"B9", "COM10"}, {"A10", "COM09"}, {"B10", "COM11"}}},
			{"tyco8", tr("CXA2A / CXA2B – 24 V, ESP, MIFA"), {"CXA2A", "CXA2B"},
			 {{"A1", "24V"}, {"B1", "24V"}, {"A2", "0V"}, {"B2", "0V"}, {"A3", "MIFA"}, {"B3", "BATL"},
			  {"A4", "*ESP"}, {"B4", "XMIFA"}}},
			{"tyco2", tr("CXA2C – 24 V"), {"CXA2C"}, {{"A1", "24V"}, {"A2", "0V"}}},
			{"tyco4", tr("CZ2L / CZ2M / CZ2N – potência do motor"), {"CZ2L", "CZ2M", "CZ2N", "CZ2"},
			 {{"B1", "U"}, {"A1", "V"}, {"B2", "W"}, {"A2", "GND"}}},
			{"tyco3", tr("CX1A / CX1B – 200 V"), {"CX1A", "CX1B"}, {{"1", "200R"}, {"2", "200S"}, {"3", "GND"}}},
			{"tyco3", tr("CX4 – parada de emergência"), {"CX4"}, {{"1", "GND"}, {"2", "ESP"}, {"3", "24V"}}},
			{"tyco3", tr("CX3 – contato do MCC"), {"CX3"}, {{"1", "MCCOFF3"}, {"3", "MCCOFF4"}}},
			{"tyco3", tr("CP1, CP1A, CP2, CA64 – 24 V"), {"CP1", "CP1A", "CP2", "CA64"}, {{"1", "24V"}, {"2", "0V"}}},
				//The αi-B amplifiers, read from B-65412EN (9.3.1, 9.3.3, 13.3)
			{"tyco4", tr("CXA2D – entrada 24 V da αiPS-B"), {"CXA2D"},
			 {{"A1", "24V"}, {"B1", "24V"}, {"A2", "0V"}, {"B2", "0V"}}},
			{"tyco3", tr("CX48 – detecção da tensão de entrada"), {"CX48"}, {{"1", "L1"}, {"2", "L2"}, {"3", "L3"}}},
			{"tyco6", tr("CX37 – contato de falta de energia"), {"CX37"},
			 {{"A1", "PF1"}, {"B1", "PF2"}, {"A3", "PF1"}, {"B3", "PF2"}}},
			{"honda20", tr("JYA3 – position coder αi (αiSP-B)"), {"JYA3"},
			 {{"1", "PZ"}, {"2", "*PZ"}, {"5", "PA"}, {"6", "*PA"}, {"7", "PB"}, {"8", "*PB"},
			  {"9", "5V"}, {"11", "24V"}, {"12", "0V"}, {"13", "SCCOM"}, {"14", "0V"}, {"15", "EXTSC"},
			  {"16", "0V"}, {"18", "5V"}, {"20", "5V"}}},
			{"honda20", tr("JYA4 – position coder S (αiSP-B)"), {"JYA4"},
			 {{"1", "PZ"}, {"2", "*PZ"}, {"5", "PA"}, {"6", "*PA"}, {"7", "PB"}, {"8", "*PB"},
			  {"9", "5V"}, {"10", "SS"}, {"12", "0V"}, {"14", "0V"}, {"16", "0V"}, {"18", "5V"}, {"20", "5V"}}},
			{"tyco12", tr("Sensor αiM – conector do motor do spindle"), {"αiM"},
			 {{"A1", "+5V"}, {"A2", "VA"}, {"B2", "RA"}, {"A3", "VB"}, {"B3", "RB"}, {"A5", "SS"},
			  {"B5", "0V"}, {"A6", "THR1"}, {"B6", "THR2"}}},
				//The I/O units and the safety operator's panel, read from the
				//tables of B-64693EN (8.2.4.1, 8.8.4.1, 8.9.3, 8.12.3)
			{"mr50", tr("CB150 – módulo de I/O (24 DI / 16 DO)"), {"CB150"},
			 {{"1", "DOCOM"}, {"2", "Yn+1.0"}, {"3", "Yn+1.1"}, {"4", "Yn+1.2"}, {"5", "Yn+1.3"}, {"6", "Yn+1.4"},
			  {"7", "Yn+1.5"}, {"8", "Yn+1.6"}, {"9", "Yn+1.7"}, {"10", "Xm+2.0"}, {"11", "Xm+2.1"}, {"12", "Xm+2.2"},
			  {"13", "Xm+2.3"}, {"14", "Xm+2.4"}, {"15", "Xm+2.5"}, {"16", "Xm+2.6"}, {"17", "Xm+2.7"}, {"18", "+24V"},
			  {"19", "0V"}, {"20", "0V"}, {"21", "0V"}, {"22", "0V"}, {"23", "0V"}, {"24", "DICOM0"},
			  {"25", "Xm+1.0"}, {"26", "Xm+1.1"}, {"27", "Xm+1.2"}, {"28", "Xm+1.3"}, {"29", "Xm+1.4"}, {"30", "Xm+1.5"},
			  {"31", "Xm+1.6"}, {"32", "Xm+1.7"}, {"33", "DOCOM"}, {"34", "Yn+0.0"}, {"35", "Yn+0.1"}, {"36", "Yn+0.2"},
			  {"37", "Yn+0.3"}, {"38", "Yn+0.4"}, {"39", "Yn+0.5"}, {"40", "Yn+0.6"}, {"41", "Yn+0.7"}, {"42", "Xm+0.0"},
			  {"43", "Xm+0.1"}, {"44", "Xm+0.2"}, {"45", "Xm+0.3"}, {"46", "Xm+0.4"}, {"47", "Xm+0.5"}, {"48", "Xm+0.6"},
			  {"49", "Xm+0.7"}, {"50", "+24V"}}},
			{"mr50", tr("CM51 – entradas do painel (Xm+0 a 5)"), {"CM51"},
			 {{"1", "Xm+0.0"}, {"2", "Xm+0.3"}, {"3", "Xm+0.6"}, {"4", "Xm+1.1"}, {"5", "Xm+1.4"}, {"6", "Xm+1.7"},
			  {"7", "Xm+2.2"}, {"8", "Xm+2.5"}, {"9", "Xm+2.7"}, {"10", "Xm+3.2"}, {"11", "Xm+3.5"}, {"12", "Xm+4.0"},
			  {"13", "Xm+4.3"}, {"14", "Xm+4.6"}, {"15", "Xm+5.1"}, {"16", "Xm+5.4"}, {"17", "Xm+5.6"}, {"18", "24V"},
			  {"19", "Xm+0.1"}, {"20", "Xm+0.4"}, {"21", "Xm+0.7"}, {"22", "Xm+1.2"}, {"23", "Xm+1.5"}, {"24", "Xm+2.0"},
			  {"25", "Xm+2.3"}, {"26", "Xm+3.0"}, {"27", "Xm+3.3"}, {"28", "Xm+3.6"}, {"29", "Xm+4.1"}, {"30", "Xm+4.4"},
			  {"31", "Xm+4.7"}, {"32", "Xm+5.2"}, {"33", "DICOM1"}, {"34", "Xm+0.2"}, {"35", "Xm+0.5"}, {"36", "Xm+1.0"},
			  {"37", "Xm+1.3"}, {"38", "Xm+1.6"}, {"39", "Xm+2.1"}, {"40", "Xm+2.4"}, {"41", "Xm+2.6"}, {"42", "Xm+3.1"},
			  {"43", "Xm+3.4"}, {"44", "Xm+3.7"}, {"45", "Xm+4.2"}, {"46", "Xm+4.5"}, {"47", "Xm+5.0"}, {"48", "Xm+5.3"},
			  {"49", "Xm+5.5"}, {"50", "Xm+5.7"}}},
			{"mr50", tr("CM52 – entradas do painel (Xm+6 a 11)"), {"CM52"},
			 {{"1", "Xm+6.0"}, {"2", "Xm+6.3"}, {"3", "Xm+6.6"}, {"4", "Xm+7.1"}, {"5", "Xm+7.4"}, {"6", "Xm+7.7"},
			  {"7", "Xm+8.2"}, {"8", "Xm+8.5"}, {"9", "Xm+8.7"}, {"10", "Xm+9.2"}, {"11", "Xm+9.5"}, {"12", "Xm+10.0"},
			  {"13", "Xm+10.3"}, {"14", "Xm+10.6"}, {"15", "Xm+11.1"}, {"16", "Xm+11.4"}, {"17", "Xm+11.6"}, {"18", "24V"},
			  {"19", "Xm+6.1"}, {"20", "Xm+6.4"}, {"21", "Xm+6.7"}, {"22", "Xm+7.2"}, {"23", "Xm+7.5"}, {"24", "Xm+8.0"},
			  {"25", "Xm+8.3"}, {"26", "Xm+9.0"}, {"27", "Xm+9.3"}, {"28", "Xm+9.6"}, {"29", "Xm+10.1"}, {"30", "Xm+10.4"},
			  {"31", "Xm+10.7"}, {"32", "Xm+11.2"}, {"33", "0V"}, {"34", "Xm+6.2"}, {"35", "Xm+6.5"}, {"36", "Xm+7.0"},
			  {"37", "Xm+7.3"}, {"38", "Xm+7.6"}, {"39", "Xm+8.1"}, {"40", "Xm+8.4"}, {"41", "Xm+8.6"}, {"42", "Xm+9.1"},
			  {"43", "Xm+9.4"}, {"44", "Xm+9.7"}, {"45", "Xm+10.2"}, {"46", "Xm+10.5"}, {"47", "Xm+11.0"}, {"48", "Xm+11.3"},
			  {"49", "Xm+11.5"}, {"50", "Xm+11.7"}}},
			{"mr50", tr("CMB3 – saídas do painel (Yn+0 a 5)"), {"CMB3"},
			 {{"1", "Yn+0.0"}, {"2", "Yn+0.3"}, {"3", "Yn+0.6"}, {"4", "Yn+1.1"}, {"5", "Yn+1.4"}, {"6", "Yn+1.7"},
			  {"7", "Yn+2.2"}, {"8", "Yn+2.5"}, {"9", "Yn+2.7"}, {"10", "Yn+3.2"}, {"11", "Yn+3.5"}, {"12", "Yn+4.0"},
			  {"13", "Yn+4.3"}, {"14", "Yn+4.6"}, {"15", "Yn+5.1"}, {"16", "Yn+5.4"}, {"17", "DOCOM"}, {"18", "DICOM2"},
			  {"19", "Yn+0.1"}, {"20", "Yn+0.4"}, {"21", "Yn+0.7"}, {"22", "Yn+1.2"}, {"23", "Yn+1.5"}, {"24", "Yn+2.0"},
			  {"25", "Yn+2.3"}, {"26", "Yn+3.0"}, {"27", "Yn+3.3"}, {"28", "Yn+3.6"}, {"29", "Yn+4.1"}, {"30", "Yn+4.4"},
			  {"31", "Yn+4.7"}, {"32", "Yn+5.2"}, {"33", "0V"}, {"34", "Yn+0.2"}, {"35", "Yn+0.5"}, {"36", "Yn+1.0"},
			  {"37", "Yn+1.3"}, {"38", "Yn+1.6"}, {"39", "Yn+2.1"}, {"40", "Yn+2.4"}, {"41", "Yn+2.6"}, {"42", "Yn+3.1"},
			  {"43", "Yn+3.4"}, {"44", "Yn+3.7"}, {"45", "Yn+4.2"}, {"46", "Yn+4.5"}, {"47", "Yn+5.0"}, {"48", "Yn+5.3"},
			  {"49", "Yn+5.5"}, {"50", "DOCOM"}}},
			{"mr20", tr("CMB4 – saídas do painel (Yn+5 a 7)"), {"CMB4"},
			 {{"1", "Yn+6.1"}, {"2", "Yn+6.4"}, {"3", "Yn+6.7"}, {"4", "Yn+7.2"}, {"5", "Yn+7.5"}, {"6", "Yn+5.6"},
			  {"7", "0V"}, {"8", "Yn+6.2"}, {"9", "Yn+6.5"}, {"10", "Yn+7.0"}, {"11", "Yn+7.3"}, {"12", "Yn+7.6"},
			  {"13", "Yn+5.7"}, {"14", "Yn+6.0"}, {"15", "Yn+6.3"}, {"16", "Yn+6.6"}, {"17", "Yn+7.1"}, {"18", "Yn+7.4"},
			  {"19", "Yn+7.7"}, {"20", "DOCOM"}}},
			{"hif3ba50", tr("CB104 – I/O do armário (Xm+0, 1, 2 / Yn+0, 1)"), {"CB104"},
			 {{"A1", "0V"}, {"B1", "+24V"}, {"A2", "Xm+0.0"}, {"B2", "Xm+0.1"}, {"A3", "Xm+0.2"}, {"B3", "Xm+0.3"},
			  {"A4", "Xm+0.4"}, {"B4", "Xm+0.5"}, {"A5", "Xm+0.6"}, {"B5", "Xm+0.7"}, {"A6", "Xm+1.0"}, {"B6", "Xm+1.1"},
			  {"A7", "Xm+1.2"}, {"B7", "Xm+1.3"}, {"A8", "Xm+1.4"}, {"B8", "Xm+1.5"}, {"A9", "Xm+1.6"}, {"B9", "Xm+1.7"},
			  {"A10", "Xm+2.0"}, {"B10", "Xm+2.1"}, {"A11", "Xm+2.2"}, {"B11", "Xm+2.3"}, {"A12", "Xm+2.4"}, {"B12", "Xm+2.5"},
			  {"A13", "Xm+2.6"}, {"B13", "Xm+2.7"}, {"A16", "Yn+0.0"}, {"B16", "Yn+0.1"}, {"A17", "Yn+0.2"}, {"B17", "Yn+0.3"},
			  {"A18", "Yn+0.4"}, {"B18", "Yn+0.5"}, {"A19", "Yn+0.6"}, {"B19", "Yn+0.7"}, {"A20", "Yn+1.0"}, {"B20", "Yn+1.1"},
			  {"A21", "Yn+1.2"}, {"B21", "Yn+1.3"}, {"A22", "Yn+1.4"}, {"B22", "Yn+1.5"}, {"A23", "Yn+1.6"}, {"B23", "Yn+1.7"},
			  {"A24", "DOCOM"}, {"B24", "DOCOM"}, {"A25", "DOCOM"}, {"B25", "DOCOM"}}},
			{"hif3ba50", tr("CB105 – I/O do armário (Xm+3, 8, 9 / Yn+2, 3)"), {"CB105"},
			 {{"A1", "0V"}, {"B1", "+24V"}, {"A2", "Xm+3.0"}, {"B2", "Xm+3.1"}, {"A3", "Xm+3.2"}, {"B3", "Xm+3.3"},
			  {"A4", "Xm+3.4"}, {"B4", "Xm+3.5"}, {"A5", "Xm+3.6"}, {"B5", "Xm+3.7"}, {"A6", "Xm+8.0"}, {"B6", "Xm+8.1"},
			  {"A7", "Xm+8.2"}, {"B7", "Xm+8.3"}, {"A8", "Xm+8.4"}, {"B8", "Xm+8.5"}, {"A9", "Xm+8.6"}, {"B9", "Xm+8.7"},
			  {"A10", "Xm+9.0"}, {"B10", "Xm+9.1"}, {"A11", "Xm+9.2"}, {"B11", "Xm+9.3"}, {"A12", "Xm+9.4"}, {"B12", "Xm+9.5"},
			  {"A13", "Xm+9.6"}, {"B13", "Xm+9.7"}, {"A16", "Yn+2.0"}, {"B16", "Yn+2.1"}, {"A17", "Yn+2.2"}, {"B17", "Yn+2.3"},
			  {"A18", "Yn+2.4"}, {"B18", "Yn+2.5"}, {"A19", "Yn+2.6"}, {"B19", "Yn+2.7"}, {"A20", "Yn+3.0"}, {"B20", "Yn+3.1"},
			  {"A21", "Yn+3.2"}, {"B21", "Yn+3.3"}, {"A22", "Yn+3.4"}, {"B22", "Yn+3.5"}, {"A23", "Yn+3.6"}, {"B23", "Yn+3.7"},
			  {"A24", "DOCOM"}, {"B24", "DOCOM"}, {"A25", "DOCOM"}, {"B25", "DOCOM"}}},
			{"hif3ba50", tr("CB106 – I/O do armário (Xm+4, 5, 6 / Yn+4, 5)"), {"CB106"},
			 {{"A1", "0V"}, {"B1", "+24V"}, {"A2", "Xm+4.0"}, {"B2", "Xm+4.1"}, {"A3", "Xm+4.2"}, {"B3", "Xm+4.3"},
			  {"A4", "Xm+4.4"}, {"B4", "Xm+4.5"}, {"A5", "Xm+4.6"}, {"B5", "Xm+4.7"}, {"A6", "Xm+5.0"}, {"B6", "Xm+5.1"},
			  {"A7", "Xm+5.2"}, {"B7", "Xm+5.3"}, {"A8", "Xm+5.4"}, {"B8", "Xm+5.5"}, {"A9", "Xm+5.6"}, {"B9", "Xm+5.7"},
			  {"A10", "Xm+6.0"}, {"B10", "Xm+6.1"}, {"A11", "Xm+6.2"}, {"B11", "Xm+6.3"}, {"A12", "Xm+6.4"}, {"B12", "Xm+6.5"},
			  {"A13", "Xm+6.6"}, {"B13", "Xm+6.7"}, {"A14", "COM4"}, {"A16", "Yn+4.0"}, {"B16", "Yn+4.1"}, {"A17", "Yn+4.2"},
			  {"B17", "Yn+4.3"}, {"A18", "Yn+4.4"}, {"B18", "Yn+4.5"}, {"A19", "Yn+4.6"}, {"B19", "Yn+4.7"}, {"A20", "Yn+5.0"},
			  {"B20", "Yn+5.1"}, {"A21", "Yn+5.2"}, {"B21", "Yn+5.3"}, {"A22", "Yn+5.4"}, {"B22", "Yn+5.5"}, {"A23", "Yn+5.6"},
			  {"B23", "Yn+5.7"}, {"A24", "DOCOM"}, {"B24", "DOCOM"}, {"A25", "DOCOM"}, {"B25", "DOCOM"}}},
			{"hif3ba50", tr("CB107 – I/O do armário (Xm+7, 10, 11 / Yn+6, 7)"), {"CB107"},
			 {{"A1", "0V"}, {"B1", "+24V"}, {"A2", "Xm+7.0"}, {"B2", "Xm+7.1"}, {"A3", "Xm+7.2"}, {"B3", "Xm+7.3"},
			  {"A4", "Xm+7.4"}, {"B4", "Xm+7.5"}, {"A5", "Xm+7.6"}, {"B5", "Xm+7.7"}, {"A6", "Xm+10.0"}, {"B6", "Xm+10.1"},
			  {"A7", "Xm+10.2"}, {"B7", "Xm+10.3"}, {"A8", "Xm+10.4"}, {"B8", "Xm+10.5"}, {"A9", "Xm+10.6"}, {"B9", "Xm+10.7"},
			  {"A10", "Xm+11.0"}, {"B10", "Xm+11.1"}, {"A11", "Xm+11.2"}, {"B11", "Xm+11.3"}, {"A12", "Xm+11.4"}, {"B12", "Xm+11.5"},
			  {"A13", "Xm+11.6"}, {"B13", "Xm+11.7"}, {"A16", "Yn+6.0"}, {"B16", "Yn+6.1"}, {"A17", "Yn+6.2"}, {"B17", "Yn+6.3"},
			  {"A18", "Yn+6.4"}, {"B18", "Yn+6.5"}, {"A19", "Yn+6.6"}, {"B19", "Yn+6.7"}, {"A20", "Yn+7.0"}, {"B20", "Yn+7.1"},
			  {"A21", "Yn+7.2"}, {"B21", "Yn+7.3"}, {"A22", "Yn+7.4"}, {"B22", "Yn+7.5"}, {"A23", "Yn+7.6"}, {"B23", "Yn+7.7"},
			  {"A24", "DOCOM"}, {"B24", "DOCOM"}, {"A25", "DOCOM"}, {"B25", "DOCOM"}}},
			{"hif3ba10", tr("CM65 – DI do painel de segurança"), {"CM65"},
			 {{"B2", "X1m+0.5"}, {"A3", "X1m+0.1"}, {"B3", "X1m+0.3"}, {"A4", "+24V"}, {"B4", "X1m+0.4"}, {"A5", "X1m+0.2"},
			  {"B5", "X1m+0.0"}}},
			{"hif3ba10", tr("CM66 – DI do painel de segurança"), {"CM66"},
			 {{"B2", "X1m+1.3"}, {"A3", "X1m+0.7"}, {"B3", "X1m+1.1"}, {"A4", "+24V"}, {"B4", "X1m+1.2"}, {"A5", "X1m+1.0"},
			  {"B5", "X1m+0.6"}}},
			{"tyco10", tr("CM67 – liga/desliga, chave, ESP"), {"CM67"},
			 {{"A1", "EON"}, {"B1", "EOFF"}, {"A2", "COM1"}, {"B2", "COM2"}, {"A3", "Xm+1.4"}, {"B3", "KEYCOM"},
			  {"A4", "*ESP"}, {"B4", "ESPCM1"}, {"A5", "TR1"}, {"B5", "TR2"}}},
			{"tyco20", tr("CA157 – relés e DI de segurança"), {"CA157"},
			 {{"A1", "EON"}, {"B1", "EOFF"}, {"A2", "COM1"}, {"B2", "COM2"}, {"A3", "*ESP"}, {"B3", "ESPCM1"},
			  {"A4", "TR1"}, {"B4", "TR2"}, {"A5", "TR3"}, {"B5", "TR4"}, {"A6", "TR5"}, {"B6", "TR6"},
			  {"A7", "X2m+0.0"}, {"B7", "X2m+0.1"}, {"A8", "X2m+0.2"}, {"B8", "X2m+0.3"}, {"A9", "X2m+0.4"}, {"B9", "X2m+0.5"},
			  {"A10", "X2m+0.6"}, {"B10", "X2m+0.7"}}},
			{"tyco20", tr("CM68 – DI/DO do painel de segurança"), {"CM68"},
			 {{"A1", "+24V"}, {"B1", "X1m+1.5"}, {"A2", "X1m+1.6"}, {"B2", "X1m+1.7"}, {"A3", "X1m+2.0"}, {"B3", "X1m+2.1"},
			  {"A4", "X1m+2.2"}, {"B4", "X1m+2.3"}, {"A5", "X1m+2.4"}, {"B5", "X1m+2.5"}, {"A6", "TR3"}, {"B6", "TR4"},
			  {"A7", "TR5"}, {"B7", "TR6"}, {"A8", "Y1n+5.3"}, {"B8", "Y1n+5.7"}, {"A9", "Y1n+6.3"}, {"B9", "Y1n+6.7"},
			  {"A10", "DOCOM"}, {"B10", "0V"}}},
			{"tyco20", tr("CM69 – DI/DO do painel de segurança"), {"CM69"},
			 {{"A1", "+24V"}, {"B1", "X1m+2.6"}, {"A2", "X1m+2.7"}, {"B2", "X1m+3.0"}, {"A3", "X1m+3.1"}, {"B3", "X1m+3.2"},
			  {"A4", "X1m+3.3"}, {"B4", "X1m+3.4"}, {"A5", "X1m+3.5"}, {"B5", "X1m+3.6"}, {"A6", "X1m+3.7"}, {"B6", "DICOM"},
			  {"A8", "Y1n+7.3"}, {"B8", "Y1n+7.4"}, {"A9", "Y1n+7.5"}, {"B9", "Y1n+7.6"}, {"A10", "DOCOM"}, {"B10", "0V"}}},
			{"honda20,fi40_15", tr("JA3 – geradores de pulsos manuais"), {"JA3"},
			 {{"1", "HA1"}, {"2", "HB1"}, {"3", "HA2"}, {"4", "HB2"}, {"5", "HA3"}, {"6", "HB3"},
			  {"9", "+5V"}, {"12", "0V"}, {"14", "0V"}, {"16", "0V"}, {"18", "+5V"}, {"20", "+5V"}}},
			{"honda20", tr("JA58 – MPG pendente"), {"JA58"},
			 {{"1", "HA1"}, {"2", "HB1"}, {"3", "X1m+2.2"}, {"4", "X1m+2.3"}, {"5", "X1m+2.4"}, {"6", "X1m+2.5"},
			  {"7", "Y1n+5.3"}, {"8", "X1m+2.1"}, {"9", "+5V"}, {"10", "+24V"}, {"11", "X1m+1.5"}, {"12", "0V"},
			  {"13", "X1m+1.6"}, {"14", "0V"}, {"15", "X1m+1.7"}, {"16", "0V"}, {"17", "X1m+2.0"}, {"18", "+5V"},
			  {"19", "+24V"}, {"20", "+5V"}}},
		};
	}

		///The FANUC connector chosen by the preset of s, nullptr if none
	const FanucPreset *fanucPreset(const ConnectorGenerator::Settings &s)
	{
		static const QList<FanucPreset> list = fanucPresets();
		int index = 0;
		for (const FanucPreset &preset : list)
			if (QString::fromLatin1(preset.variant).split(',').contains(s.variant) && ++index == s.preset)
				return &preset;
		return nullptr;
	}
}

QJsonObject ConnectorGenerator::Settings::toJson() const
{
	return QJsonObject {
		{"variant", variant}, {"male", male}, {"symbol", symbol}, {"front_view", front_view},
		{"solder_side", solder_side}, {"side", int(side)}, {"preset", preset}, {"colours", colours},
		{"signals", QJsonArray::fromStringList(signal_names)}, {"rows", rows}, {"columns", columns},
		{"numbering", int(numbering)}, {"prefix", prefix}, {"first", first},
		{"style", int(style)}, {"top", top}, {"used_only", used_only}, {"title", title}};
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
	s.style = Style(json.value("style").toInt(s.style));
	s.top = json.value("top").toBool(s.top);
	s.used_only = json.value("used_only").toBool(s.used_only);
	s.title = json.value("title").toString();
	return s;
}

QString ConnectorGenerator::familyName(Family family)
{
	switch (family) {
		case DSub:    return tr("D-sub (DB, HD)");
		case Modular: return tr("Modular (RJ45, RJ11, RJ12)");
		case Usb:     return tr("USB");
		case Fanuc:   return tr("FANUC (Honda, Hirose, Tyco)");
		case Generic: return tr("Genérico (fileiras × colunas)");
		case FamilyCount: break;
	}
	return QString();
}

QList<ConnectorGenerator::Variant> ConnectorGenerator::variants()
{
	const QString none = tr("Nenhum (preencher)");
	QList<Variant> list {
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
		{QStringLiteral("honda20"), Fanuc, tr("Honda PCR-E20 / Hirose FI40 (20 pinos)"), {none}},
		{QStringLiteral("fi40_15"), Fanuc, tr("Hirose FI40B-2015S (15 pinos, pulsecoder)"), {none}},
		{QStringLiteral("mr20"), Fanuc, tr("Honda MR-20 (20 pinos)"), {none}},
		{QStringLiteral("mr50"), Fanuc, tr("Honda MR-50 (50 pinos)"), {none}},
		{QStringLiteral("hif3ba10"), Fanuc, tr("Hirose HIF3BA-10 (A1–B5)"), {none}},
		{QStringLiteral("hif3ba20"), Fanuc, tr("Hirose HIF3BA-20 (A1–B10)"), {none}},
		{QStringLiteral("hif3ba50"), Fanuc, tr("Hirose HIF3BA-50 (A1–B25)"), {none}},
		{QStringLiteral("tyco20"), Fanuc, tr("Tyco D-3000 2×10 (A1–B10)"), {none}},
		{QStringLiteral("tyco12"), Fanuc, tr("Tyco D-3000 2×6 (A1–B6)"), {none}},
		{QStringLiteral("tyco10"), Fanuc, tr("Tyco D-3000 2×5 (A1–B5)"), {none}},
		{QStringLiteral("tyco8"), Fanuc, tr("Tyco 2×4 (A1–B4)"), {none}},
		{QStringLiteral("tyco6"), Fanuc, tr("Tyco D-2100 2×3 (A1–B3)"), {none}},
		{QStringLiteral("tyco4"), Fanuc, tr("Tyco 2×2 (A1–B2)"), {none}},
		{QStringLiteral("tyco3"), Fanuc, tr("Tyco 3 pinos (1–3)"), {none}},
		{QStringLiteral("tyco2"), Fanuc, tr("Tyco 2 pinos (A1, A2)"), {none}},
		{QStringLiteral("generic"), Generic, tr("Genérico"), {none}},
	};
	for (Variant &v : list)
		for (const FanucPreset &preset : fanucPresets())
			if (QString::fromLatin1(preset.variant).split(',').contains(v.id))
				v.presets << preset.name;
	return list;
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
	} else if (isPcr(id)) {
			//Two rows, 1 to 10 over 11 to 20 (the cable connector shows
			//10...1 over 20...11, FANUC manuals Fig. A (v), (w)); the
			//FI40B-2015S has no 11, 13, 15, 17 and 19
		for (int i = 0; i < 20; ++i)
			if (id == QLatin1String("honda20") || i < 10 || i % 2 == 1)
				list << Pin {QString::number(i + 1), i < 10 ? QPointF(i, 0) : QPointF(i - 10, 1)};
	} else if (id.startsWith(QLatin1String("hif3ba")) || id == QLatin1String("tyco20")
		   || id == QLatin1String("tyco12") || id == QLatin1String("tyco10") || id == QLatin1String("tyco8")
		   || id == QLatin1String("tyco6") || id == QLatin1String("tyco4")) {
			//Row A over row B, in the order A1, B1, A2, B2...; the cable
			//connector shows A01 on the left (Hirose, Fig. A (al))
		const int columns = id == QLatin1String("hif3ba10") || id == QLatin1String("tyco10") ? 5
				  : id == QLatin1String("hif3ba20") || id == QLatin1String("tyco20") ? 10
				  : id == QLatin1String("hif3ba50") ? 25 : id == QLatin1String("tyco12") ? 6
				  : id == QLatin1String("tyco8") ? 4 : id == QLatin1String("tyco6") ? 3 : 2;
		const bool hirose = id.startsWith(QLatin1String("hif3ba"));
		for (int i = 0; i < columns; ++i) {
			const qreal x = hirose ? columns - 1 - i : i;
			list << Pin {QStringLiteral("A%1").arg(i + 1), QPointF(x, 0)}
			     << Pin {QStringLiteral("B%1").arg(i + 1), QPointF(x, 1)};
		}
	} else if (id == QLatin1String("tyco3")) {
		for (int i = 0; i < 3; ++i)
			list << Pin {QString::number(i + 1), QPointF(i, 0)};
	} else if (id == QLatin1String("tyco2")) {
		list << Pin {QStringLiteral("A1"), QPointF(0, 0)} << Pin {QStringLiteral("A2"), QPointF(1, 0)};
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
	if (const FanucPreset *preset = fanucPreset(s)) {
		const QList<Pin> list = pins(s);
		for (const auto &pin : preset->pins)
			for (int i = 0; i < list.size(); ++i)
				if (list.at(i).id == pin.first)
					names[i] = pin.second;
	}

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
	name += s.male ? tr(" macho") : tr(" fêmea");
	return s.title.isEmpty() ? name : s.title + QStringLiteral(" – ") + name;
}

/**
	@return the connector names the preset of s stands for (JD1A,
	JD1B...), empty when it is not one of the FANUC connectors
*/
QStringList ConnectorGenerator::presetTitles(const Settings &s)
{
	const FanucPreset *preset = fanucPreset(s);
	return preset ? preset->titles : QStringList();
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
	if (s.symbol && s.style == FanucStyle) {
			//The pins drawn, in the order of the FANUC diagrams
		QList<int> shown;
		if (const FanucPreset *preset = fanucPreset(s)) {
			for (const auto &pin : preset->pins)
				for (int i = 0; i < pin_list.size(); ++i)
					if (pin_list.at(i).id == pin.first && !shown.contains(i))
						shown << i;
		}
		for (int i = 0; i < pin_list.size(); ++i)
			if (!shown.contains(i) && (!s.used_only || !names.at(i).isEmpty()))
				shown << i;
		if (s.used_only)
			shown.erase(std::remove_if(shown.begin(), shown.end(), [&](int i) {return names.at(i).isEmpty();}),
				    shown.end());

			//The signals as small as the numbers, as FANUC draws them
		const TextStyles::Style &upright_style = number_style;
		const QFontMetricsF number_metrics(number_style.font);
		const QFontMetricsF upright_metrics(upright_style.font);
		qreal number_length = 0, signal_length = 0;
		for (int i : shown) {
			number_length = qMax(number_length, number_metrics.horizontalAdvance(QStringLiteral("(%1)").arg(pin_list.at(i).id)));
			signal_length = qMax(signal_length, upright_metrics.horizontalAdvance(names.at(i)));
		}
			//Inward from the edge of the unit (y = 0): the numbers, the
			//signals, the bracket and the name of the connector; the
			//terminals outside. dir is 1 when the unit is under its edge.
		const qreal dir = s.top ? 1 : -1;
		const qreal last = qMax(0, int(shown.size()) - 1) * 10.0;
		const qreal signals_end = 3 + number_length + 3 + signal_length;
		const qreal bracket = dir * (signals_end + 3);
		for (int k = 0; k < shown.size(); ++k) {
			const int i = shown.at(k);
			const qreal x = k * 10.0;
			const QString number = QStringLiteral("(%1)").arg(pin_list.at(i).id);
			w.line(x, 0, x, -dir * 10);
			w.terminal(pin_list.at(i).id, x, -dir * 10, s.top ? "n" : "s");
			w.rect(QRectF(x - 1.2, s.top ? -2.5 : 0, 2.4, 2.5), 0, filled_style);
				//Read from the bottom up: the start is the lower end
			const qreal number_width = number_metrics.horizontalAdvance(number);
			const qreal signal_width = upright_metrics.horizontalAdvance(names.at(i));
			if (s.top) {
				w.uprightText(number, x, 3 + number_width, number_style);
				w.uprightText(names.at(i), x, signals_end, upright_style);
			} else {
				w.uprightText(number, x, -3, number_style);
				w.uprightText(names.at(i), x, -signals_end + signal_width, upright_style);
			}
		}
		w.line(-4, bracket, last + 4, bracket, thin_style);
		w.line(-4, bracket, -4, bracket - dir * 2, thin_style);
		w.line(last + 4, bracket, last + 4, bracket - dir * 2, thin_style);
		const QString name = s.title.isEmpty() ? defaultName(s) : s.title;
		const qreal name_height = QFontMetricsF(signal_style.font).capHeight();
		w.text(name, last / 2, s.top ? bracket + name_height + 3 : bracket - 3, signal_style, 0);
			//The label of the unit (-N131) at the left of the pins
		w.labelField(-40, s.top ? 2 : -14, label_style.name);
		view_x = last + 50;
		view_y = s.top ? 10 : bracket;
	} else if (s.symbol) {
			//Which pins leave by the left, which by the right, top to bottom
		QList<int> shown, left, rightward;
		for (int i = 0; i < pin_list.size(); ++i)
			if (!s.used_only || !names.at(i).isEmpty())
				shown << i;
		for (int k = 0; k < shown.size(); ++k) {
			if (s.side == Left || (s.side == BothSides && k < (shown.size() + 1) / 2))
				left << shown.at(k);
			else
				rightward << shown.at(k);
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
		const bool contacts = family == Modular || family == Usb || (family == Fanuc && !dsub);
		const qreal step = dsub && isHighDensity(id) ? pitch * 0.82 : pitch;
			//Flat contacts in two rows (USB B and C) need room between them;
			//the rows of the PCR are 1.7 mm apart for a pitch of 1.27 mm
		const qreal row_step = isPcr(id) ? 1.34 * step
				     : (family == Modular || family == Usb) && max_y > 0 ? 2 * step : step;
			//Half the width and height of a flat contact
		const qreal contact_w = isPcr(id) ? 0.25 : family == Fanuc ? 0.3 : 0.18;
		const qreal contact_h = isPcr(id) ? 0.16 : family == Fanuc ? 0.3 : 0.4;
			//FANUC draws the Honda MR jack upside down (Fig. A (aj)): its
			//face is turned over, not mirrored
		const bool flipped = mirrored && isHondaMr(id);
		const qreal span_x = max_x * step, span_y = max_y * row_step;
		const qreal ox = view_x + (dsub ? (dsubRows(id).size() == 3 ? 4.5 : 3.5) * step : 2 * step), oy = view_y;
		auto place = [&](const QPointF &p) {
			if (flipped)
				return QPointF(ox + p.x() * step, oy + (max_y - p.y()) * row_step);
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
			if (flipped)
				w.polygon(QPolygonF({{left_top + slant, top}, {right_top - slant, top}, {right_top, low},
						     {left_top, low}}));
			else
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
		} else if (isPcr(id)) {
				//The shell of the 20-pin interface (Fig. A (v), (w)): a
				//trapeze with rounded corners, 16.25 mm over 13.35 mm and
				//5.5 mm high, in a flange of 19.2 by 9.2 mm
			const QPointF c(ox + span_x / 2, oy + span_y / 2);
			const qreal top = c.y() - 2.15 * step, low = c.y() + 2.15 * step;
			const qreal wide = 6.4 * step, narrow = 5.25 * step, r = 0.6 * step;
			w.polygon(QPolygonF({{c.x() - wide + r, top}, {c.x() + wide - r, top}, {c.x() + wide, top + r},
					     {c.x() + narrow + r * 0.3, low - r}, {c.x() + narrow - r, low},
					     {c.x() - narrow + r, low}, {c.x() - narrow - r * 0.3, low - r},
					     {c.x() - wide, top + r}}));
			w.rect(QRectF(c.x() - 7.55 * step, c.y() - 3.6 * step, 15.1 * step, 7.2 * step), 0, thin_style);
			bottom = c.y() + 3.6 * step;
		} else if (id.startsWith(QLatin1String("hif3ba"))) {
				//The body, its polarity key at the top and the mark of A01
				//(Fig. A (al))
			const QRectF body(ox - step * 0.8, oy - step * 1.4, span_x + step * 1.6, span_y + step * 2.8);
			w.rect(body);
			w.rect(QRectF(body.center().x() - step, body.top() - step * 0.5, 2 * step, step * 0.5), 0, thin_style);
			for (const Pin &pin : pin_list) {
				if (pin.id != QLatin1String("A1"))
					continue;
				const QPointF a = place(pin.position);
				w.polygon(QPolygonF({{a.x() - step * 0.4, body.top() - step * 1.2}, {a.x() + step * 0.4, body.top() - step * 1.2},
						     {a.x(), body.top() - step * 0.4}}), filled_style);
			}
			bottom = body.bottom();
		} else if (family == Fanuc) {
				//Tyco housings: the body and its latch on top
			const QRectF body(ox - step * 1.2, oy - step * 1.2, span_x + step * 2.4, span_y + step * 2.4);
			w.rect(body, 1);
			w.rect(QRectF(body.center().x() - step * 0.8, body.top() - step * 0.6, step * 1.6, step * 0.6), 0, thin_style);
			bottom = body.bottom();
		} else {
			w.rect(QRectF(ox - step * 0.9, oy - step * 0.9, span_x + step * 1.8, span_y + step * 1.8), 2);
			bottom = oy + span_y + step * 0.9;
		}

			//The pins (filled) or sockets (hollow), numbered above them
			//on the top half, under them on the bottom half; on the
			//middle row, too tight, only its ends are, outside
		const qreal radius = qMin(step * 0.28, 2.6);
		const qreal half = contacts ? step * contact_h : radius;
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
				w.rect(QRectF(c.x() - step * contact_w, c.y() - half, 2 * step * contact_w, 2 * half), 0,
				       s.male ? filled_style : thin_style);
			else
				w.circle(c, radius, s.male ? filled_style : thin_style);
			const qreal y = flipped ? max_y - pin.position.y() : pin.position.y();
			if (max_y == 0 || y < max_y / 2 - 0.01)
				w.text(pin.id, c.x(), c.y() - half - 1, small, 0);
			else if (y > max_y / 2 + 0.01)
				w.text(pin.id, c.x(), c.y() + half + 1 + cap, small, 0);
			else if (&pin == &pin_list.at(middle_first) || &pin == &pin_list.at(middle_last)) {
				const bool left_end = (&pin == &pin_list.at(middle_first)) != (mirrored && !flipped);
				w.text(pin.id, c.x() + (left_end ? -1 : 1) * (radius + 0.6), c.y() + cap / 2, small, left_end ? 1 : -1);
			}
		}

		const QString view = s.solder_side ? tr("vista da solda") : tr("vista do encaixe");
		w.text(title + QStringLiteral(" – ") + view, ox - step, bottom + 9, 6);
	}
	return w.document();
}
