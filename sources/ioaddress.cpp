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
#include "ioaddress.h"

#include "diagram.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/elementtextitemgroup.h"
#include "qetgraphicsitem/terminal.h"

#include <QPointer>
#include <QRegularExpression>
#include <QTimer>

namespace
{
		///%{Xm+0.0}, %{Yn+1.7}, and the byte alone: %{Xm+11}
	const QRegularExpression &placeholder()
	{
		static const QRegularExpression re(QStringLiteral("%\\{([XY])([mn])\\+(\\d+)(?:\\.([0-7]))?\\}"));
		return re;
	}

	bool baseOf(const QString &value, int *base)
	{
		bool ok = false;
		*base = value.trimmed().toInt(&ok);
		return ok && *base >= 0;
	}

	bool hasOwnBase(const Element *element)
	{
		const DiagramContext dc = element->elementInformations();
		return !dc.value(QETInformation::ELMT_IO_X_BASE).toString().trimmed().isEmpty()
			|| !dc.value(QETInformation::ELMT_IO_Y_BASE).toString().trimmed().isEmpty();
	}

		///The terminals wired to \a terminal, through folio reports too
	QList<Terminal *> wiredTo(Terminal *terminal)
	{
		QList<Terminal *> wired;
		const QList<Conductor *> conductors = terminal->conductors();
		for (Conductor *c : conductors)
		{
			Terminal *other = c->terminal1 == terminal ? c->terminal2 : c->terminal1;
			if (!other || !other->parentElement())
				continue;
			Element *element = other->parentElement();
			if ((element->linkType() & Element::AllReport) && !element->linkedElements().isEmpty())
			{
				Element *report = element->linkedElements().first();
				if (report->terminals().isEmpty())
					continue;
				Terminal *report_terminal = report->terminals().first();
				const QList<Conductor *> next = report_terminal->conductors();
				for (Conductor *n : next)
				{
					Terminal *far = n->terminal1 == report_terminal ? n->terminal2 : n->terminal1;
					if (far && far->parentElement())
						wired << far;
				}
			}
			else
				wired << other;
		}
		return wired;
	}

		///The element \a element takes its base from, nullptr if none
	Element *source(Element *element, const QList<Element *> &with_base)
	{
		const QList<Terminal *> terminals = element->terminals();
		for (Terminal *t : terminals)
		{
			if (t->name().isEmpty())
				continue;
			const QList<Terminal *> wired = wiredTo(t);
			for (Terminal *other : wired)
				if (other->name() == t->name() && other->parentElement() != element
						&& hasOwnBase(other->parentElement()))
					return other->parentElement();
		}

		const QString label = element->actualLabel();
		if (!label.isEmpty())
			for (Element *other : with_base)
				if (other != element && other->actualLabel() == label)
					return other;
		return nullptr;
	}

	QList<DynamicElementTextItem *> allTexts(const Element *element)
	{
		QList<DynamicElementTextItem *> texts = element->dynamicTextItems();
		const QList<ElementTextItemGroup *> groups = element->textGroups();
		for (ElementTextItemGroup *group : groups)
			texts << group->texts();
		return texts;
	}

	QList<QPointer<QETProject>> &pending()
	{
		static QList<QPointer<QETProject>> list;
		return list;
	}
}

bool IoAddress::contains(const QString &text)
{
	return text.contains(QStringLiteral("%{X")) || text.contains(QStringLiteral("%{Y"))
		? placeholder().match(text).hasMatch() : false;
}

QString IoAddress::replace(const QString &text, const DiagramContext &dc)
{
	if (!contains(text))
		return text;

	QString result;
	int last = 0;
	QRegularExpressionMatchIterator it = placeholder().globalMatch(text);
	while (it.hasNext())
	{
		const QRegularExpressionMatch m = it.next();
		result += text.mid(last, m.capturedStart() - last);
		last = m.capturedEnd();

		const QString area = m.captured(1);
		const int byte = m.captured(3).toInt();
		const QString bit = m.captured(4);
		int base = 0;
		if (baseOf(dc.value(area == QLatin1String("X") ? QETInformation::ELMT_IO_X_BASE
							       : QETInformation::ELMT_IO_Y_BASE).toString(), &base))
			result += area + QString::number(base + byte) + (bit.isEmpty() ? QString() : QStringLiteral(".") + bit);
		else
			result += m.captured(0).mid(2, m.capturedLength() - 3);
	}
	result += text.mid(last);
	return result;
}

DiagramContext IoAddress::context(const Element *element)
{
	if (!element)
		return DiagramContext();
	DiagramContext dc = element->elementInformations();
	const QPair<QString, QString> inherited = element->inheritedIoBase();
	if (dc.value(QETInformation::ELMT_IO_X_BASE).toString().trimmed().isEmpty() && !inherited.first.isEmpty())
		dc.addValue(QETInformation::ELMT_IO_X_BASE, inherited.first);
	if (dc.value(QETInformation::ELMT_IO_Y_BASE).toString().trimmed().isEmpty() && !inherited.second.isEmpty())
		dc.addValue(QETInformation::ELMT_IO_Y_BASE, inherited.second);
	return dc;
}

void IoAddress::refresh(QETProject *project)
{
	if (!project)
		return;

	QList<Element *> addressed, with_base;
	const QList<Diagram *> diagrams = project->diagrams();
	for (Diagram *d : diagrams)
	{
		const QList<Element *> elements = d->elements();
		for (Element *e : elements)
		{
			if (e->hasIoAddressTexts())
				addressed << e;
			if (hasOwnBase(e))
				with_base << e;
		}
	}

	for (Element *e : std::as_const(addressed))
	{
		QPair<QString, QString> inherited;
		if (Element *from = hasOwnBase(e) ? nullptr : source(e, with_base))
		{
			const DiagramContext dc = from->elementInformations();
			inherited = {dc.value(QETInformation::ELMT_IO_X_BASE).toString().trimmed(),
				     dc.value(QETInformation::ELMT_IO_Y_BASE).toString().trimmed()};
		}
		e->setInheritedIoBase(inherited);
		const QList<DynamicElementTextItem *> texts = allTexts(e);
		for (DynamicElementTextItem *text : texts)
			text->refreshIoAddresses();
	}
}

void IoAddress::scheduleRefresh(QETProject *project)
{
	if (!project)
		return;
	for (const QPointer<QETProject> &p : std::as_const(pending()))
		if (p == project)
			return;
	pending() << project;
	QTimer::singleShot(0, project, [project]()
	{
		pending().removeAll(QPointer<QETProject>(project));
		pending().removeAll(QPointer<QETProject>());
		refresh(project);
	});
}
