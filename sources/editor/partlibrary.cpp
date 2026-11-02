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
#include "partlibrary.h"

#include "../qetapp.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QPolygonF>
#include <QUuid>
#include <QtMath>

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("PartLibrary", text);
	}

	const QString line_style = QStringLiteral("line-style:normal;line-weight:normal;filling:none;color:black");
	const QString dashed_style = QStringLiteral("line-style:dashed;line-weight:normal;filling:none;color:black");
	const QString filled_style = QStringLiteral("line-style:normal;line-weight:normal;filling:black;color:black");

	QString number(qreal value)
	{
		return QString::number(qRound(value * 100.0) / 100.0);
	}

	/**
		Writes parts in an element definition, the symbols drawn
		vertically and laid down when horizontal: mirrored about the
		diagonal, the top going to the left and the blades of the contacts
		staying above their line
	*/
	class Builder
	{
		public:
			explicit Builder(bool horizontal = false) : m_horizontal(horizontal)
			{
				QDomElement definition = m_document.createElement(QStringLiteral("definition"));
				definition.setAttribute(QStringLiteral("type"), QStringLiteral("element"));
				m_document.appendChild(definition);
				m_description = m_document.createElement(QStringLiteral("description"));
				definition.appendChild(m_description);
			}

			QPointF map(qreal x, qreal y) const
			{
				return m_horizontal ? QPointF(y, x) : QPointF(x, y);
			}

			QRectF map(const QRectF &rect) const
			{
				return QRectF(map(rect.left(), rect.top()), map(rect.right(), rect.bottom())).normalized();
			}

			void line(qreal x1, qreal y1, qreal x2, qreal y2, const QString &style = line_style)
			{
				const QPointF a = map(x1, y1), b = map(x2, y2);
				QDomElement e = element(QStringLiteral("line"), style);
				e.setAttribute(QStringLiteral("x1"), number(a.x()));
				e.setAttribute(QStringLiteral("y1"), number(a.y()));
				e.setAttribute(QStringLiteral("x2"), number(b.x()));
				e.setAttribute(QStringLiteral("y2"), number(b.y()));
				e.setAttribute(QStringLiteral("end1"), QStringLiteral("none"));
				e.setAttribute(QStringLiteral("end2"), QStringLiteral("none"));
				e.setAttribute(QStringLiteral("length1"), QStringLiteral("1.5"));
				e.setAttribute(QStringLiteral("length2"), QStringLiteral("1.5"));
			}

			void rect(const QRectF &r, qreal radius = 0)
			{
				const QRectF m = map(r);
				QDomElement e = element(QStringLiteral("rect"), line_style);
				e.setAttribute(QStringLiteral("x"), number(m.x()));
				e.setAttribute(QStringLiteral("y"), number(m.y()));
				e.setAttribute(QStringLiteral("width"), number(m.width()));
				e.setAttribute(QStringLiteral("height"), number(m.height()));
				e.setAttribute(QStringLiteral("rx"), number(radius));
				e.setAttribute(QStringLiteral("ry"), number(radius));
			}

			void circle(qreal cx, qreal cy, qreal radius)
			{
				const QPointF c = map(cx, cy);
				QDomElement e = element(QStringLiteral("circle"), line_style);
				e.setAttribute(QStringLiteral("x"), number(c.x() - radius));
				e.setAttribute(QStringLiteral("y"), number(c.y() - radius));
				e.setAttribute(QStringLiteral("diameter"), number(2 * radius));
			}

				///Angles in degrees, counterclockwise from 3 o'clock
			void arc(qreal cx, qreal cy, qreal radius, qreal start, qreal span)
			{
				const QPointF c = map(cx, cy);
				QDomElement e = element(QStringLiteral("arc"), line_style);
				e.setAttribute(QStringLiteral("x"), number(c.x() - radius));
				e.setAttribute(QStringLiteral("y"), number(c.y() - radius));
				e.setAttribute(QStringLiteral("width"), number(2 * radius));
				e.setAttribute(QStringLiteral("height"), number(2 * radius));
				e.setAttribute(QStringLiteral("start"), number(m_horizontal ? 270 - start : start));
				e.setAttribute(QStringLiteral("angle"), number(m_horizontal ? -span : span));
			}

			void polygon(const QPolygonF &points, bool closed = true, bool filled = false)
			{
				QDomElement e = element(QStringLiteral("polygon"), filled ? filled_style : line_style);
				for (int i = 0; i < points.size(); ++i) {
					const QPointF p = map(points.at(i).x(), points.at(i).y());
					e.setAttribute(QStringLiteral("x%1").arg(i + 1), number(p.x()));
					e.setAttribute(QStringLiteral("y%1").arg(i + 1), number(p.y()));
				}
				if (!closed)
					e.setAttribute(QStringLiteral("closed"), QStringLiteral("false"));
			}

				///A text centred on (cx, cy), upright whatever the turn
			void text(const QString &text, qreal cx, qreal cy, qreal size)
			{
				const QFont font = QETApp::diagramTextsFont(size);
				const QFontMetricsF metrics(font);
				const QPointF c = map(cx, cy);
				QDomElement e = m_document.createElement(QStringLiteral("text"));
				e.setAttribute(QStringLiteral("text"), text);
				e.setAttribute(QStringLiteral("x"), number(c.x() - metrics.horizontalAdvance(text) / 2));
				e.setAttribute(QStringLiteral("y"), number(c.y() + metrics.capHeight() / 2));
				e.setAttribute(QStringLiteral("rotation"), 0);
				e.setAttribute(QStringLiteral("font"), font.toString());
				e.setAttribute(QStringLiteral("color"), QStringLiteral("#000000"));
				m_description.appendChild(e);
			}

				///orientation: n, s, e or w, the way the conductor leaves
			void terminal(const QString &name, qreal x, qreal y, QChar orientation)
			{
				static const QString vertical = QStringLiteral("nswe"), laid = QStringLiteral("wens");
				if (m_horizontal)
					orientation = laid.at(vertical.indexOf(orientation));
				const QPointF p = map(x, y);
				QDomElement e = m_document.createElement(QStringLiteral("terminal"));
				e.setAttribute(QStringLiteral("uuid"), QUuid::createUuid().toString());
				e.setAttribute(QStringLiteral("name"), name);
				e.setAttribute(QStringLiteral("x"), number(p.x()));
				e.setAttribute(QStringLiteral("y"), number(p.y()));
				e.setAttribute(QStringLiteral("orientation"), QString(orientation));
				e.setAttribute(QStringLiteral("type"), QStringLiteral("Generic"));
				m_description.appendChild(e);
			}

			QDomDocument document() const {return m_document;}

		private:
			QDomElement element(const QString &tag, const QString &style)
			{
				QDomElement e = m_document.createElement(tag);
				e.setAttribute(QStringLiteral("style"), style);
				e.setAttribute(QStringLiteral("antialias"), QStringLiteral("false"));
				m_description.appendChild(e);
				return e;
			}

			bool m_horizontal;
			QDomDocument m_document;
			QDomElement m_description;
	};

	PartLibrary::Param number(const QString &key, const QString &label, double value,
				  double minimum, double maximum, double step = 1)
	{
		PartLibrary::Param param;
		param.key = key;
		param.label = label;
		param.value = value;
		param.minimum = minimum;
		param.maximum = maximum;
		param.step = step;
		return param;
	}

	PartLibrary::Param text(const QString &key, const QString &label, const QString &value)
	{
		PartLibrary::Param param;
		param.key = key;
		param.label = label;
		param.kind = PartLibrary::Param::Text;
		param.value = value;
		return param;
	}

	PartLibrary::Param check(const QString &key, const QString &label, bool value)
	{
		PartLibrary::Param param;
		param.key = key;
		param.label = label;
		param.kind = PartLibrary::Param::Check;
		param.value = value;
		return param;
	}

	PartLibrary::Param choice(const QString &key, const QString &label, const QStringList &choices, int value = 0)
	{
		PartLibrary::Param param;
		param.key = key;
		param.label = label;
		param.kind = PartLibrary::Param::Choice;
		param.choices = choices;
		param.value = value;
		return param;
	}

		///A symbol between two connection points: its terminals, their names, the turn
	PartLibrary::Entry symbol(const QString &id, const QString &group, const QString &name,
				  const QString &pin_a, const QString &pin_b)
	{
		PartLibrary::Entry entry {id, group, name, {}};
		entry.params << check(QStringLiteral("terminals"), tr("Com bornes"), true)
			     << text(QStringLiteral("pin_a"), tr("Borne de cima"), pin_a);
		if (!pin_b.isNull())
			entry.params << text(QStringLiteral("pin_b"), tr("Borne de baixo"), pin_b);
		entry.params << check(QStringLiteral("horizontal"), tr("Horizontal"), false);
		return entry;
	}

	void pins(Builder &b, const QVariantMap &v)
	{
		if (!v.value(QStringLiteral("terminals")).toBool())
			return;
		b.terminal(v.value(QStringLiteral("pin_a")).toString(), 0, -20, QLatin1Char('n'));
		if (v.contains(QStringLiteral("pin_b")))
			b.terminal(v.value(QStringLiteral("pin_b")).toString(), 0, 20, QLatin1Char('s'));
	}

	void normallyOpen(Builder &b)
	{
		b.line(0, -20, 0, -8);
		b.line(0, 8, 0, 20);
		b.line(0, 8, -7, -9);
	}

	void normallyClosed(Builder &b)
	{
		b.line(0, -20, 0, -8);
		b.line(0, -8, 7, -8);
		b.line(0, 8, 0, 20);
		b.line(0, 8, 9, -10);
	}

	void diode(Builder &b)
	{
		b.line(0, -20, 0, -5);
		b.line(0, 5, 0, 20);
		b.polygon(QPolygonF({{-6, -5}, {6, -5}, {0, 5}}));
		b.line(-6, 5, 6, 5);
	}

		///An arrow from (x1, y1) to (x2, y2), its head of size head
	void arrow(Builder &b, qreal x1, qreal y1, qreal x2, qreal y2, qreal head, bool filled)
	{
		b.line(x1, y1, x2, y2);
		const qreal angle = qAtan2(y2 - y1, x2 - x1);
		const QPointF tip(x2, y2);
		const QPointF left = tip - head * QPointF(qCos(angle - 0.4), qSin(angle - 0.4));
		const QPointF right = tip - head * QPointF(qCos(angle + 0.4), qSin(angle + 0.4));
		b.polygon(QPolygonF({left, tip, right}), filled, filled);
	}
}

/**
	@return every piece, in groups
*/
QList<PartLibrary::Entry> PartLibrary::entries()
{
	const QString contacts = tr("Contatos IEC");
	const QString components = tr("Componentes IEC");
	const QString shapes = tr("Formas paramétricas");

	QList<Entry> list {
		symbol(QStringLiteral("no"), contacts, tr("Contato NA"), QStringLiteral("13"), QStringLiteral("14")),
		symbol(QStringLiteral("nc"), contacts, tr("Contato NF"), QStringLiteral("11"), QStringLiteral("12")),
		symbol(QStringLiteral("push_no"), contacts, tr("Botão NA"), QStringLiteral("13"), QStringLiteral("14")),
		symbol(QStringLiteral("estop"), contacts, tr("Emergência NF (cogumelo)"), QStringLiteral("11"), QStringLiteral("12")),
		symbol(QStringLiteral("coil"), contacts, tr("Bobina"), QStringLiteral("A1"), QStringLiteral("A2")),
		symbol(QStringLiteral("fuse"), components, tr("Fusível"), QStringLiteral("1"), QStringLiteral("2")),
		symbol(QStringLiteral("resistor"), components, tr("Resistor"), QStringLiteral("1"), QStringLiteral("2")),
		symbol(QStringLiteral("capacitor"), components, tr("Capacitor"), QStringLiteral("1"), QStringLiteral("2")),
		symbol(QStringLiteral("diode"), components, tr("Diodo"), QStringLiteral("A"), QStringLiteral("K")),
		symbol(QStringLiteral("led"), components, tr("LED"), QStringLiteral("A"), QStringLiteral("K")),
		symbol(QStringLiteral("lamp"), components, tr("Lâmpada / sinaleiro"), QStringLiteral("X1"), QStringLiteral("X2")),
		symbol(QStringLiteral("ground"), components, tr("Terra de proteção"), QStringLiteral("PE"), QString()),
	};

	Entry motor {QStringLiteral("motor"), components, tr("Motor"), {}};
	motor.params << choice(QStringLiteral("phases"), tr("Tipo"), {tr("Monofásico / CC (2 bornes)"), tr("Trifásico (U V W)")}, 1)
		     << check(QStringLiteral("terminals"), tr("Com bornes"), true)
		     << check(QStringLiteral("horizontal"), tr("Horizontal"), false);
	list << motor;

	list << Entry {QStringLiteral("polygon"), shapes, tr("Polígono regular"), {
			number(QStringLiteral("sides"), tr("Lados"), 6, 3, 24),
			number(QStringLiteral("radius"), tr("Raio"), 10, 1, 500),
			number(QStringLiteral("rotation"), tr("Rotação (graus)"), 0, -360, 360, 15)}};
	list << Entry {QStringLiteral("rounded"), shapes, tr("Retângulo arredondado"), {
			number(QStringLiteral("width"), tr("Largura"), 40, 1, 1000),
			number(QStringLiteral("height"), tr("Altura"), 20, 1, 1000),
			number(QStringLiteral("radius"), tr("Raio do canto"), 4, 0, 100)}};
	list << Entry {QStringLiteral("terminal_row"), shapes, tr("Fileira de bornes"), {
			number(QStringLiteral("count"), tr("Quantidade"), 4, 1, 64),
			number(QStringLiteral("pitch"), tr("Passo"), 10, 10, 200, 10),
			choice(QStringLiteral("side"), tr("Saída dos condutores"), {tr("Para cima"), tr("Para baixo"), tr("Para a esquerda"), tr("Para a direita")}),
			number(QStringLiteral("lead"), tr("Comprimento da perna"), 10, 0, 100, 5),
			text(QStringLiteral("prefix"), tr("Prefixo dos nomes"), QString()),
			number(QStringLiteral("first"), tr("Primeiro número"), 1, 0, 9999),
			number(QStringLiteral("increment"), tr("Incremento"), 1, -100, 100)}};
	list << Entry {QStringLiteral("arrow"), shapes, tr("Seta"), {
			number(QStringLiteral("length"), tr("Comprimento"), 30, 2, 1000),
			number(QStringLiteral("angle"), tr("Ângulo (graus)"), 0, -360, 360, 15),
			number(QStringLiteral("head"), tr("Tamanho da ponta"), 6, 1, 50),
			check(QStringLiteral("filled"), tr("Ponta cheia"), true),
			check(QStringLiteral("double"), tr("Duas pontas"), false)}};
	list << Entry {QStringLiteral("arc"), shapes, tr("Arco"), {
			number(QStringLiteral("radius"), tr("Raio"), 10, 1, 500),
			number(QStringLiteral("start"), tr("Início (graus)"), 0, -360, 360, 15),
			number(QStringLiteral("span"), tr("Abertura (graus)"), 180, -360, 360, 15)}};
	return list;
}

const PartLibrary::Entry *PartLibrary::find(const QList<Entry> &entries, const QString &id)
{
	for (const Entry &entry : entries)
		if (entry.id == id)
			return &entry;
	return nullptr;
}

QVariantMap PartLibrary::defaults(const Entry &entry)
{
	QVariantMap values;
	for (const Param &param : entry.params)
		values.insert(param.key, param.value);
	return values;
}

/**
	@return the parts of the piece id drawn with values (the defaults
	for those missing), as an element definition
*/
QDomDocument PartLibrary::build(const QString &id, const QVariantMap &given)
{
	const QList<Entry> list = entries();
	const Entry *entry = find(list, id);
	if (!entry)
		return QDomDocument();
	QVariantMap v = defaults(*entry);
	for (auto it = given.cbegin(); it != given.cend(); ++it)
		v.insert(it.key(), it.value());
	auto real = [&v](const char *key) { return v.value(QString::fromLatin1(key)).toDouble(); };

	Builder b(v.value(QStringLiteral("horizontal")).toBool());
	if (id == QLatin1String("no")) {
		normallyOpen(b);
	} else if (id == QLatin1String("nc")) {
		normallyClosed(b);
	} else if (id == QLatin1String("push_no")) {
		normallyOpen(b);
			//Push button: the actuator pushes the blade from the left
		b.line(-4, 0, -14, 0, dashed_style);
		b.line(-14, -4, -14, 4);
		b.line(-14, -4, -17, -4);
		b.line(-14, 4, -17, 4);
	} else if (id == QLatin1String("estop")) {
		normallyClosed(b);
			//Emergency stop: a mushroom head
		b.line(4, -1, -12, -1, dashed_style);
		b.line(-12, -7, -12, 5);
		b.arc(-12, -1, 6, 90, 180);
	} else if (id == QLatin1String("coil")) {
		b.line(0, -20, 0, -6);
		b.line(0, 6, 0, 20);
		b.rect(QRectF(-10, -6, 20, 12));
	} else if (id == QLatin1String("fuse")) {
		b.rect(QRectF(-4, -10, 8, 20));
		b.line(0, -20, 0, 20);
	} else if (id == QLatin1String("resistor")) {
		b.rect(QRectF(-4, -10, 8, 20));
		b.line(0, -20, 0, -10);
		b.line(0, 10, 0, 20);
	} else if (id == QLatin1String("capacitor")) {
		b.line(0, -20, 0, -3);
		b.line(0, 3, 0, 20);
		b.line(-9, -3, 9, -3);
		b.line(-9, 3, 9, 3);
	} else if (id == QLatin1String("diode")) {
		diode(b);
	} else if (id == QLatin1String("led")) {
		diode(b);
		arrow(b, 7, -2, 13, -8, 3, false);
		arrow(b, 7, 3, 13, -3, 3, false);
	} else if (id == QLatin1String("lamp")) {
		b.line(0, -20, 0, -8);
		b.line(0, 8, 0, 20);
		b.circle(0, 0, 8);
		const qreal d = 8 / M_SQRT2;
		b.line(-d, -d, d, d);
		b.line(-d, d, d, -d);
	} else if (id == QLatin1String("ground")) {
		b.line(0, -20, 0, 0);
		b.line(-8, 0, 8, 0);
		b.line(-5, 3, 5, 3);
		b.line(-2, 6, 2, 6);
	} else if (id == QLatin1String("motor")) {
		const bool three = v.value(QStringLiteral("phases")).toInt() == 1;
		const bool with_terminals = v.value(QStringLiteral("terminals")).toBool();
		b.circle(0, 0, 12);
			//Upright, M over its kind of current, laid down or not (the
			//laying down swaps x and y)
		const bool horizontal = v.value(QStringLiteral("horizontal")).toBool();
		b.text(QStringLiteral("M"), horizontal ? -2 : 0, horizontal ? 0 : -2, 9);
		b.text(three ? QStringLiteral("3~") : QStringLiteral("~"), horizontal ? 6 : 0, horizontal ? 0 : 6, 6);
		if (three) {
			const qreal side = -qSqrt(144 - 100);
			b.line(-10, -20, -10, side);
			b.line(0, -20, 0, -12);
			b.line(10, -20, 10, side);
			if (with_terminals) {
				b.terminal(QStringLiteral("U"), -10, -20, QLatin1Char('n'));
				b.terminal(QStringLiteral("V"), 0, -20, QLatin1Char('n'));
				b.terminal(QStringLiteral("W"), 10, -20, QLatin1Char('n'));
			}
		} else {
			b.line(0, -20, 0, -12);
			b.line(0, 12, 0, 20);
			if (with_terminals) {
				b.terminal(QStringLiteral("1"), 0, -20, QLatin1Char('n'));
				b.terminal(QStringLiteral("2"), 0, 20, QLatin1Char('s'));
			}
		}
		return b.document();
	} else if (id == QLatin1String("polygon")) {
		const int sides = qBound(3, int(real("sides")), 24);
		QPolygonF points;
		for (int i = 0; i < sides; ++i) {
			const qreal angle = qDegreesToRadians(real("rotation") - 90 + 360.0 * i / sides);
			points << QPointF(real("radius") * qCos(angle), real("radius") * qSin(angle));
		}
		b.polygon(points);
		return b.document();
	} else if (id == QLatin1String("rounded")) {
		b.rect(QRectF(-real("width") / 2, -real("height") / 2, real("width"), real("height")), real("radius"));
		return b.document();
	} else if (id == QLatin1String("terminal_row")) {
			//Along x for up and down, along y for left and right, the
			//legs going into the element from the terminals
		const int side = v.value(QStringLiteral("side")).toInt();
		static const char orientations[] = {'n', 's', 'w', 'e'};
		const QPointF inward[] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
		const QPointF along = side < 2 ? QPointF(1, 0) : QPointF(0, 1);
		const qreal pitch = qMax(10.0, qRound(real("pitch") / 10) * 10.0);
		for (int i = 0; i < int(real("count")); ++i) {
			const QPointF p = along * pitch * i;
			const QPointF end = p + inward[side] * real("lead");
			if (real("lead") > 0)
				b.line(p.x(), p.y(), end.x(), end.y());
			b.terminal(v.value(QStringLiteral("prefix")).toString()
				   + QString::number(int(real("first") + i * real("increment"))),
				   p.x(), p.y(), QLatin1Char(orientations[side]));
		}
		return b.document();
	} else if (id == QLatin1String("arrow")) {
		const qreal angle = qDegreesToRadians(real("angle"));
		const QPointF end(real("length") * qCos(angle), -real("length") * qSin(angle));
		const bool filled = v.value(QStringLiteral("filled")).toBool();
		arrow(b, 0, 0, end.x(), end.y(), real("head"), filled);
		if (v.value(QStringLiteral("double")).toBool())
			arrow(b, end.x(), end.y(), 0, 0, real("head"), filled);
		return b.document();
	} else if (id == QLatin1String("arc")) {
		b.arc(0, 0, real("radius"), real("start"), real("span"));
		return b.document();
	}
	pins(b, v);
	return b.document();
}
