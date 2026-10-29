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
#include "componentwizard.h"

#include "../properties/elementdata.h"
#include "../qetapp.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QRectF>
#include <QUuid>

#include <cmath>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("ComponentWizard", text);
	}

	/**
		Writes the parts of the element and keeps the box around them, to
		put the label beside the drawing.
	*/
	class Drawing
	{
		public:
			Drawing() :
				m_pin_font(QETApp::diagramTextsFont(6))
			{
				QDomElement definition = m_document.createElement(QStringLiteral("definition"));
				definition.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
				m_document.appendChild(definition);
				m_description = m_document.createElement(QStringLiteral("description"));
				definition.appendChild(m_description);
			}

			void line(qreal x1, qreal y1, qreal x2, qreal y2, bool dashed = false)
			{
				QDomElement e = m_document.createElement(QStringLiteral("line"));
				e.setAttribute(QStringLiteral("x1"), x1);
				e.setAttribute(QStringLiteral("y1"), y1);
				e.setAttribute(QStringLiteral("x2"), x2);
				e.setAttribute(QStringLiteral("y2"), y2);
				e.setAttribute(QStringLiteral("end1"), QStringLiteral("none"));
				e.setAttribute(QStringLiteral("end2"), QStringLiteral("none"));
				e.setAttribute(QStringLiteral("length1"), QStringLiteral("1.5"));
				e.setAttribute(QStringLiteral("length2"), QStringLiteral("1.5"));
				e.setAttribute(QStringLiteral("style"), style(dashed));
				e.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
				m_description.appendChild(e);
				grow(QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized());
			}

			void rect(qreal x, qreal y, qreal width, qreal height)
			{
				QDomElement e = m_document.createElement(QStringLiteral("rect"));
				e.setAttribute(QStringLiteral("x"), x);
				e.setAttribute(QStringLiteral("y"), y);
				e.setAttribute(QStringLiteral("width"), width);
				e.setAttribute(QStringLiteral("height"), height);
				e.setAttribute(QStringLiteral("rx"), 0);
				e.setAttribute(QStringLiteral("ry"), 0);
				e.setAttribute(QStringLiteral("style"), style(false));
				e.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
				m_description.appendChild(e);
				grow(QRectF(x, y, width, height));
			}

			void circle(qreal cx, qreal cy, qreal diameter)
			{
				QDomElement e = m_document.createElement(QStringLiteral("circle"));
				e.setAttribute(QStringLiteral("x"), cx - diameter / 2);
				e.setAttribute(QStringLiteral("y"), cy - diameter / 2);
				e.setAttribute(QStringLiteral("diameter"), diameter);
				e.setAttribute(QStringLiteral("style"), style(false));
				e.setAttribute(QStringLiteral("antialias"), QStringLiteral("true"));
				m_description.appendChild(e);
				grow(QRectF(cx - diameter / 2, cy - diameter / 2, diameter, diameter));
			}

				///A text whose baseline starts at (x, y)
			void text(const QString &text, qreal x, qreal y, const QFont &font)
			{
				if (text.isEmpty())
					return;
				QDomElement e = m_document.createElement(QStringLiteral("text"));
				e.setAttribute(QStringLiteral("text"), text);
				e.setAttribute(QStringLiteral("x"), QString::number(x, 'f', 1));
				e.setAttribute(QStringLiteral("y"), QString::number(y, 'f', 1));
				e.setAttribute(QStringLiteral("rotation"), 0);
				e.setAttribute(QStringLiteral("font"), font.toString());
				e.setAttribute(QStringLiteral("color"), QStringLiteral("#000000"));
				m_description.appendChild(e);
				const QFontMetricsF metrics(font);
				grow(QRectF(x, y - metrics.capHeight(), metrics.horizontalAdvance(text),
					    metrics.capHeight()));
			}

				///A text centred on (x, y)
			void centredText(const QString &text, qreal x, qreal y, const QFont &font)
			{
				const QFontMetricsF metrics(font);
				this->text(text, x - metrics.horizontalAdvance(text) / 2,
					   y + metrics.capHeight() / 2, font);
			}

			void terminal(const QString &name, int x, int y, const char *orientation)
			{
				QDomElement e = m_document.createElement(QStringLiteral("terminal"));
				e.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
				e.setAttribute(QStringLiteral("name"), name);
				e.setAttribute(QStringLiteral("x"), x);
				e.setAttribute(QStringLiteral("y"), y);
				e.setAttribute(QStringLiteral("orientation"), QString::fromLatin1(orientation));
				e.setAttribute(QStringLiteral("type"), QStringLiteral("Generic"));
				m_description.appendChild(e);
			}

				///The marking of a terminal written beside its lead
			void pinName(const QString &name, qreal x, qreal y)
			{
				if (m_pin_names)
					text(name, x, y, m_pin_font);
			}

			void label()
			{
				const QFont font = QETApp::diagramTextsFont(9);
				QDomElement e = m_document.createElement(QStringLiteral("dynamic_text"));
				e.setAttribute(QStringLiteral("x"), std::ceil(m_bounds.right()) + 6);
				e.setAttribute(QStringLiteral("y"), std::floor(m_bounds.top()));
				e.setAttribute(QStringLiteral("z"), 100);
				e.setAttribute(QStringLiteral("text_width"), -1);
				e.setAttribute(QStringLiteral("Halignment"), QStringLiteral("AlignLeft"));
				e.setAttribute(QStringLiteral("Valignment"), QStringLiteral("AlignTop"));
				e.setAttribute(QStringLiteral("frame"), QStringLiteral("false"));
				e.setAttribute(QStringLiteral("rotation"), 0);
				e.setAttribute(QStringLiteral("text_from"), QStringLiteral("ElementInfo"));
				e.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
				e.setAttribute(QStringLiteral("font"), font.toString());
				e.appendChild(m_document.createElement(QStringLiteral("text")));
				QDomElement info = m_document.createElement(QStringLiteral("info_name"));
				info.appendChild(m_document.createTextNode(QStringLiteral("label")));
				e.appendChild(info);
				m_description.appendChild(e);
			}

			void setPinNames(bool show) { m_pin_names = show; }
			QDomDocument document() const { return m_document; }

		private:
			static QString style(bool dashed)
			{
				return QStringLiteral("line-style:%1;line-weight:normal;filling:none;color:black")
						.arg(dashed ? QStringLiteral("dashed") : QStringLiteral("normal"));
			}

			void grow(const QRectF &rect) { m_bounds = m_bounds.isNull() ? rect : m_bounds.united(rect); }

			QDomDocument m_document;
			QDomElement m_description;
			QFont m_pin_font;
			QRectF m_bounds;
			bool m_pin_names = true;
	};

	/**
		A vertical contact between a terminal at y -20 (top) and one at
		y 20 (bottom), the moving blade fixed at the bottom.
		@return the middle of the blade, where the mechanical link starts
	*/
	QPointF contact(Drawing &d, int x, bool nc, bool breaker,
			const QString &top, const QString &bottom)
	{
		d.line(x, -20, x, -10);
		d.line(x, 10, x, 20);
		QPointF middle;
		if (nc) {
			d.line(x, -10, x + 8, -10);
			d.line(x, 10, x + 8, -12);
			middle = QPointF(x + 4, -1);
		} else {
			d.line(x, 10, x - 8, -8);
			middle = QPointF(x - 4, 1);
		}
		if (breaker) {
			d.line(x - 3, -13, x + 3, -7);
			d.line(x - 3, -7, x + 3, -13);
		}
		d.terminal(top, x, -20, "n");
		d.terminal(bottom, x, 20, "s");
		d.pinName(top, x + 2, -13);
		d.pinName(bottom, x + 2, 19);
		return middle;
	}

		///The push button head left of the contact, linked to its blade
	void pushHead(Drawing &d, const QPointF &blade)
	{
		d.line(blade.x(), blade.y(), -20, blade.y(), true);
		d.line(-20, blade.y() - 6, -20, blade.y() + 6);
		d.line(-20, blade.y() - 6, -23, blade.y() - 6);
		d.line(-20, blade.y() + 6, -23, blade.y() + 6);
	}
}

QString ComponentWizard::kindName(Kind kind)
{
	switch (kind) {
		case Coil:          return tr("Bobina de contator ou relé (mestre)");
		case NoContact:     return tr("Contato auxiliar NA (escravo)");
		case NcContact:     return tr("Contato auxiliar NF (escravo)");
		case PowerContacts: return tr("Contatos de potência (escravo)");
		case Breaker:       return tr("Disjuntor (mestre)");
		case PushButtonNo:  return tr("Botoeira NA");
		case PushButtonNc:  return tr("Botoeira NF");
		case Lamp:          return tr("Sinaleiro");
		case Motor:         return tr("Motor trifásico");
		case PowerSupply:   return tr("Fonte CA/CC");
		case TerminalBlock: return tr("Borne de régua");
		case KindCount:     break;
	}
	return QString();
}

QString ComponentWizard::defaultName(Kind kind, int poles)
{
	switch (kind) {
		case Coil:          return tr("Bobina de contator");
		case NoContact:     return tr("Contato auxiliar NA");
		case NcContact:     return tr("Contato auxiliar NF");
		case PowerContacts: return tr("Contatos de potência %1P").arg(poles);
		case Breaker:       return tr("Disjuntor %1P").arg(poles);
		case PushButtonNo:  return tr("Botoeira NA");
		case PushButtonNc:  return tr("Botoeira NF");
		case Lamp:          return tr("Sinaleiro");
		case Motor:         return tr("Motor trifásico");
		case PowerSupply:   return tr("Fonte CA/CC");
		case TerminalBlock: return tr("Borne");
		case KindCount:     break;
	}
	return QString();
}

/**
	@return the usual letter of the label (IEC 81346): K, Q, S, H, M, G, X;
	empty for a slave, which shows the label of its master
*/
QString ComponentWizard::defaultPrefix(Kind kind)
{
	switch (kind) {
		case Coil:          return QStringLiteral("K");
		case Breaker:       return QStringLiteral("Q");
		case PushButtonNo:
		case PushButtonNc:  return QStringLiteral("S");
		case Lamp:          return QStringLiteral("H");
		case Motor:         return QStringLiteral("M");
		case PowerSupply:   return QStringLiteral("G");
		case TerminalBlock: return QStringLiteral("X");
		default:            return QString();
	}
}

bool ComponentWizard::hasPoles(Kind kind)
{
	return kind == PowerContacts || kind == Breaker;
}

bool ComponentWizard::hasEarth(Kind kind)
{
	return kind == Motor || kind == PowerSupply;
}

/**
	@return the names of the terminals, in the order build() writes them
*/
QStringList ComponentWizard::terminalNames(const Settings &settings)
{
	QStringList names;
	switch (settings.kind) {
		case Coil:         names << QStringLiteral("A1") << QStringLiteral("A2"); break;
		case NoContact:
		case PushButtonNo: names << QStringLiteral("13") << QStringLiteral("14"); break;
		case NcContact:    names << QStringLiteral("21") << QStringLiteral("22"); break;
		case PushButtonNc: names << QStringLiteral("11") << QStringLiteral("12"); break;
		case PowerContacts:
		case Breaker:
			for (int pole = 0; pole < settings.poles; ++pole)
				names << QString::number(2 * pole + 1) << QString::number(2 * pole + 2);
			break;
		case Lamp:  names << QStringLiteral("X1") << QStringLiteral("X2"); break;
		case Motor:
			names << QStringLiteral("U1") << QStringLiteral("V1") << QStringLiteral("W1");
			if (settings.earth)
				names << QStringLiteral("PE");
			break;
		case PowerSupply:
			names << QStringLiteral("L") << QStringLiteral("N");
			if (settings.earth)
				names << QStringLiteral("PE");
			names << QStringLiteral("+") << QStringLiteral("-");
			break;
		case TerminalBlock: names << QString() << QString(); break;
		case KindCount: break;
	}
	return names;
}

/**
	@return what the element becomes, as written in its properties
*/
QString ComponentWizard::typeDescription(Kind kind)
{
	switch (kind) {
		case Coil:          return tr("mestre, bobina: os contatos escravos mostram a referência cruzada");
		case Breaker:       return tr("mestre, proteção: os contatos auxiliares se ligam a ele");
		case NoContact:     return tr("escravo, contato simples NA");
		case NcContact:     return tr("escravo, contato simples NF");
		case PowerContacts: return tr("escravo, contato de potência NA");
		case TerminalBlock: return tr("borne: entra no editor de régua de bornes");
		default:            return tr("simples");
	}
}

/**
	@brief ComponentWizard::build
	The symbol is drawn around (0, 0), the hotspot of the element; every
	terminal is on the 10 px grid.
	@return an element definition holding the parts only
*/
QDomDocument ComponentWizard::build(const Settings &settings)
{
	Drawing d;
	d.setPinNames(settings.pin_names);
	const QStringList names = terminalNames(settings);

	switch (settings.kind) {
		case Coil:
			d.rect(-10, -5, 20, 10);
			d.line(0, -20, 0, -5);
			d.line(0, 5, 0, 20);
			d.terminal(names.at(0), 0, -20, "n");
			d.terminal(names.at(1), 0, 20, "s");
			d.pinName(names.at(0), 3, -9);
			d.pinName(names.at(1), 3, 16);
			break;

		case NoContact:
		case NcContact:
			contact(d, 0, settings.kind == NcContact, false, names.at(0), names.at(1));
			break;

		case PowerContacts:
		case Breaker: {
			QPointF first, last;
			for (int pole = 0; pole < settings.poles; ++pole) {
				last = contact(d, pole * 20, false, settings.kind == Breaker,
					       names.at(2 * pole), names.at(2 * pole + 1));
				if (pole == 0)
					first = last;
			}
			if (settings.poles > 1)
				d.line(first.x(), first.y(), last.x(), last.y(), true);
			break;
		}

		case PushButtonNo:
		case PushButtonNc:
			pushHead(d, contact(d, 0, settings.kind == PushButtonNc, false,
					   names.at(0), names.at(1)));
			break;

		case Lamp: {
			const qreal diagonal = 8 / std::sqrt(2.0);
			d.circle(0, 0, 16);
			d.line(-diagonal, -diagonal, diagonal, diagonal);
			d.line(-diagonal, diagonal, diagonal, -diagonal);
			d.line(0, -20, 0, -8);
			d.line(0, 8, 0, 20);
			d.terminal(names.at(0), 0, -20, "n");
			d.terminal(names.at(1), 0, 20, "s");
			d.pinName(names.at(0), 2, -13);
			d.pinName(names.at(1), 2, 19);
			break;
		}

		case Motor: {
			d.circle(0, 0, 40);
			for (int i = 0; i < 3; ++i) {
				const int x = (i - 1) * 10;
				d.line(x, -30, x, -std::sqrt(400.0 - x * x));
				d.terminal(names.at(i), x, -30, "n");
			}
				//The names above the leads would be too close: inside the circle
			for (int i = 0; i < 3; ++i)
				d.centredText(names.at(i), (i - 1) * 12, -11, QETApp::diagramTextsFont(5));
			d.centredText(QStringLiteral("M"), 0, 1, QETApp::diagramTextsFont(12));
			d.centredText(QStringLiteral("3~"), 0, 12, QETApp::diagramTextsFont(7));
			if (settings.earth) {
				d.line(20, 0, 30, 0);
				d.terminal(names.at(3), 30, 0, "e");
				d.pinName(names.at(3), 22, -2);
			}
			break;
		}

		case PowerSupply: {
			d.rect(-30, -20, 60, 40);
			d.line(-30, 20, 30, -20);
			d.centredText(QStringLiteral("~"), -18, -9, QETApp::diagramTextsFont(10));
			d.centredText(QStringLiteral("="), 18, 9, QETApp::diagramTextsFont(10));
			int index = 0;
			QList<int> inputs = {-20, 0};
			if (settings.earth)
				inputs << 20;
			for (int x : inputs) {
				d.line(x, -30, x, -20);
				d.terminal(names.at(index), x, -30, "n");
				d.pinName(names.at(index), x + 2, -23);
				++index;
			}
			for (int x : {-10, 10}) {
				d.line(x, 20, x, 30);
				d.terminal(names.at(index), x, 30, "s");
				d.pinName(names.at(index), x + 2, 29);
				++index;
			}
			break;
		}

		case TerminalBlock:
			d.circle(0, 0, 6);
			d.line(0, -10, 0, -3);
			d.line(0, 3, 0, 10);
			d.terminal(names.at(0), 0, -10, "n");
			d.terminal(names.at(1), 0, 10, "s");
			break;

		case KindCount:
			break;
	}

	if (settings.label_field)
		d.label();
	return d.document();
}

/**
	@brief ComponentWizard::applyData
	Give data the type matching the kind of component, the name of the
	element in the language of the application and the prefix of its label.
*/
void ComponentWizard::applyData(const Settings &settings, ElementData &data)
{
	switch (settings.kind) {
		case Coil:
			data.m_type = ElementData::Master;
			data.m_master_type = ElementData::Coil;
			break;
		case Breaker:
			data.m_type = ElementData::Master;
			data.m_master_type = ElementData::Protection;
			break;
		case NoContact:
		case NcContact:
			data.m_type = ElementData::Slave;
			data.m_slave_type = ElementData::SSimple;
			data.m_slave_state = settings.kind == NcContact ? ElementData::NC : ElementData::NO;
			data.m_contact_count = 1;
			break;
		case PowerContacts:
			data.m_type = ElementData::Slave;
			data.m_slave_type = ElementData::Power;
			data.m_slave_state = ElementData::NO;
			data.m_contact_count = settings.poles;
			break;
		case TerminalBlock:
			data.m_type = ElementData::Terminal;
			break;
		default:
			data.m_type = ElementData::Simple;
			break;
	}

	if (!settings.name.isEmpty())
		data.m_names_list.addName(QETApp::langFromSetting().left(2), settings.name);
	if (data.m_type != ElementData::Slave && !settings.prefix.isEmpty())
		data.m_informations.addValue(QStringLiteral("label"), settings.prefix);
}
