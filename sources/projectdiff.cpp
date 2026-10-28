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
#include "projectdiff.h"

#include "diagram.h"
#include "diagramposition.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetinformation.h"
#include "qetproject.h"

#include <QCoreApplication>
#include <QHash>

#include <algorithm>

namespace {
	QString tr(const char *text, const char *comment = nullptr, int n = -1)
	{
		return QCoreApplication::translate("ProjectDiff", text, comment, n);
	}

	QString arrow(const QString &before, const QString &after)
	{
		const QString empty = tr("(vazio)");
		return QStringLiteral("%1 → %2").arg(before.isEmpty() ? empty : before,
						      after.isEmpty() ? empty : after);
	}

	void fillFolio(ProjectDiff::Change &change, Diagram *diagram, const QPointF &pos)
	{
		change.folio_uuid = diagram->uuid();
		change.folio_index = diagram->folioIndex();
		change.folio_title = diagram->title();
		change.pos = pos;
		change.cell = diagram->border_and_titleblock.convertPosition(pos).toString();
	}

	QString elementName(Element *element)
	{
		const QString label = element->actualLabel();
		return label.isEmpty() ? element->name()
				       : QStringLiteral("%1 (%2)").arg(label, element->name());
	}

	QString terminalName(Terminal *terminal)
	{
		Element *element = terminal->parentElement();
		const QString label = element ? element->actualLabel() : QString();
		const QString name = terminal->name();
		if (name.isEmpty() || name == QLatin1String("_"))
			return label.isEmpty() ? tr("(sem rótulo)") : label;
		return QStringLiteral("%1:%2").arg(label, name);
	}

	QString conductorName(Conductor *conductor)
	{
		const QString text = conductor->properties().text;
		const QString ends = QStringLiteral("%1 – %2").arg(
					terminalName(conductor->terminal1),
					terminalName(conductor->terminal2));
		return text.isEmpty() ? tr("Fio %1").arg(ends)
				      : tr("Fio %1: %2").arg(text, ends);
	}

	QStringList elementDetails(Element *before, Element *after)
	{
		QStringList details;
		if (before->location().path() != after->location().path())
			details << tr("Símbolo: %1").arg(arrow(before->name(), after->name()));

		const DiagramContext info_before = before->elementInformations();
		const DiagramContext info_after = after->elementInformations();
		QStringList keys = info_before.keys() + info_after.keys();
		keys.removeDuplicates();
		std::sort(keys.begin(), keys.end());
			//The label first: it is what the user reads on the folio
		if (keys.removeOne(QStringLiteral("label")))
			keys.prepend(QStringLiteral("label"));
		for (const QString &key : std::as_const(keys)) {
			const QString value_before = info_before.value(key).toString();
			const QString value_after = info_after.value(key).toString();
			if (value_before != value_after) {
				details << QStringLiteral("%1: %2").arg(
						   QETInformation::translatedInfoKey(key),
						   arrow(value_before, value_after));
			}
		}

		if (before->rotation() != after->rotation())
			details << tr("Rotação: %1° → %2°").arg(before->rotation()).arg(after->rotation());
		if (before->hasHorizontalMirror() != after->hasHorizontalMirror()
		    || before->hasVerticalMirror() != after->hasVerticalMirror())
			details << tr("Espelhamento alterado");
		return details;
	}

	QStringList conductorDetails(Conductor *before, Conductor *after)
	{
		const ConductorProperties p_before = before->properties();
		const ConductorProperties p_after = after->properties();
		QStringList details;
		auto compare = [&details](const QString &what, const QString &b, const QString &a) {
			if (b != a) details << QStringLiteral("%1: %2").arg(what, arrow(b, a));
		};
		compare(tr("Número"), p_before.text, p_after.text);
		compare(tr("Função"), p_before.m_function, p_after.m_function);
		compare(tr("Cor do fio"), p_before.m_wire_color, p_after.m_wire_color);
		compare(tr("Seção"), p_before.m_wire_section, p_after.m_wire_section);
		compare(tr("Cabo"), p_before.m_cable, p_after.m_cable);
		compare(tr("Barramento"), p_before.m_bus, p_after.m_bus);
		compare(tr("Tensão / protocolo"), p_before.m_tension_protocol, p_after.m_tension_protocol);
		if (p_before.color != p_after.color)
			compare(tr("Cor no desenho"), p_before.color.name(), p_after.color.name());
		return details;
	}
}

/**
	@brief ProjectDiff::conductorKey
	@param conductor
	@return what identifies a conductor in every version of a project:
	the two terminals it joins, in a stable order
*/
QString ProjectDiff::conductorKey(Conductor *conductor)
{
	auto end = [](Terminal *terminal) {
		if (!terminal) return QString();
		const QString element = terminal->parentElement()
				? terminal->parentElement()->uuid().toString()
				: QString();
		const QString id = terminal->uuid().isNull() ? terminal->name()
							     : terminal->uuid().toString();
		return element + QLatin1Char('/') + id;
	};
	QStringList ends {end(conductor->terminal1), end(conductor->terminal2)};
	ends.sort();
	return ends.join(QLatin1Char('|'));
}

/**
	@brief ProjectDiff::compare
	@param before : the old version
	@param after : the new version
	@return what changed from before to after, sorted by folio
*/
QList<ProjectDiff::Change> ProjectDiff::compare(QETProject *before, QETProject *after)
{
	QList<Change> changes;
	if (!before || !after) {
		return changes;
	}

		//Pair the folios: by uuid, then by title
	const QList<Diagram *> folios_before = before->diagrams();
	const QList<Diagram *> folios_after = after->diagrams();
	QHash<Diagram *, Diagram *> pair;	// after -> before
	QList<Diagram *> unpaired_before = folios_before;
	for (Diagram *folio : folios_after) {
		for (Diagram *candidate : std::as_const(unpaired_before)) {
			if (candidate->uuid() == folio->uuid()) {
				pair.insert(folio, candidate);
				unpaired_before.removeOne(candidate);
				break;
			}
		}
	}
	for (Diagram *folio : folios_after) {
		if (pair.contains(folio)) continue;
		for (Diagram *candidate : std::as_const(unpaired_before)) {
			if (candidate->title() == folio->title()) {
				pair.insert(folio, candidate);
				unpaired_before.removeOne(candidate);
				break;
			}
		}
	}

	QHash<Diagram *, Diagram *> pair_back;	// before -> after
	for (auto it = pair.cbegin() ; it != pair.cend() ; ++it) {
		pair_back.insert(it.value(), it.key());
	}

		//Folios
	for (Diagram *folio : folios_after) {
		if (Diagram *old_folio = pair.value(folio)) {
			if (old_folio->title() != folio->title()) {
				Change change;
				change.item = Change::Folio;
				fillFolio(change, folio, QPointF());
				change.name = tr("Fólio");
				change.details << tr("Título: %1").arg(arrow(old_folio->title(), folio->title()));
				changes << change;
			}
		} else {
			Change change;
			change.kind = Change::Added;
			change.item = Change::Folio;
			fillFolio(change, folio, QPointF());
			change.name = tr("Fólio adicionado");
			change.details << tr("%n elemento(s)", nullptr, folio->elements().size());
			changes << change;
		}
	}
	for (Diagram *old_folio : std::as_const(unpaired_before)) {
		Change change;
		change.kind = Change::Removed;
		change.item = Change::Folio;
		fillFolio(change, old_folio, QPointF());
		change.name = tr("Fólio removido");
		change.details << tr("%n elemento(s)", nullptr, old_folio->elements().size());
		changes << change;
	}

		//Elements, wherever they are: an element can move to another folio.
		//Those of an added or removed folio are reported with the folio.
	QHash<QUuid, Element *> elements_before;
	for (Diagram *folio : folios_before) {
		if (!pair_back.contains(folio)) continue;
		for (Element *element : folio->elements())
			elements_before.insert(element->uuid(), element);
	}
	for (Diagram *folio : folios_after) {
		if (!pair.contains(folio)) continue;
		for (Element *element : folio->elements()) {
			Element *old_element = elements_before.take(element->uuid());
			Change change;
			change.item = Change::Element;
			change.element_uuid = element->uuid();
			change.name = elementName(element);
			fillFolio(change, folio, element->pos());
			if (!old_element) {
				change.kind = Change::Added;
				changes << change;
				continue;
			}
			change.details = elementDetails(old_element, element);
			Diagram *old_folio = old_element->diagram();
			if (pair_back.value(old_folio) != folio) {
				change.details.prepend(tr("Movido do fólio %1 (%2)")
						       .arg(old_folio->folioIndex() + 1)
						       .arg(old_folio->title()));
			} else if (old_element->pos() != element->pos()) {
				const QString old_cell = old_folio->border_and_titleblock
						.convertPosition(old_element->pos()).toString();
				change.details << (old_cell == change.cell
						   ? tr("Posição: deslocado em %1").arg(change.cell)
						   : tr("Posição: %1").arg(arrow(old_cell, change.cell)));
			}
			if (!change.details.isEmpty())
				changes << change;
		}
	}
	for (Element *old_element : std::as_const(elements_before)) {
		Change change;
		change.kind = Change::Removed;
		change.item = Change::Element;
		change.element_uuid = old_element->uuid();
		change.name = elementName(old_element);
			//Reported on the folio of the new version, where it was
		fillFolio(change, pair_back.value(old_element->diagram()), old_element->pos());
		changes << change;
	}

		//Conductors, by the terminals they join
	QHash<QString, Conductor *> conductors_before;
	for (Diagram *folio : folios_before) {
		if (!pair_back.contains(folio)) continue;
		for (Conductor *conductor : folio->conductors())
			conductors_before.insert(conductorKey(conductor), conductor);
	}
	for (Diagram *folio : folios_after) {
		if (!pair.contains(folio)) continue;
		for (Conductor *conductor : folio->conductors()) {
			const QString key = conductorKey(conductor);
			Conductor *old_conductor = conductors_before.take(key);
			Change change;
			change.item = Change::Conductor;
			change.conductor_key = key;
			change.name = conductorName(conductor);
			fillFolio(change, folio, conductor->sceneBoundingRect().center());
			if (!old_conductor) {
				change.kind = Change::Added;
				changes << change;
				continue;
			}
			change.details = conductorDetails(old_conductor, conductor);
			if (!change.details.isEmpty())
				changes << change;
		}
	}
	for (Conductor *old_conductor : std::as_const(conductors_before)) {
		Change change;
		change.kind = Change::Removed;
		change.item = Change::Conductor;
		change.conductor_key = conductorKey(old_conductor);
		change.name = conductorName(old_conductor);
		fillFolio(change, pair_back.value(old_conductor->diagram()),
			  old_conductor->sceneBoundingRect().center());
		changes << change;
	}

	std::stable_sort(changes.begin(), changes.end(),
			 [](const Change &a, const Change &b) {
		if (a.folio_index != b.folio_index) return a.folio_index < b.folio_index;
		if (a.item != b.item) return a.item < b.item;
		return a.kind < b.kind;
	});
	return changes;
}
