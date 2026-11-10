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
#include "wiringcheck.h"

#include "conductorendlabeling.h"
#include "dataBase/projectdatabase.h"
#include "diagram.h"
#include "projectnumbering.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"
#include "wiringlist.h"
#include "wiringrules.h"
#include "wirejunction.h"

#include <QCollator>
#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace {
	QString tr(const char *text, const char *comment = nullptr, int n = -1)
	{
		return QCoreApplication::translate("WiringCheck", text, comment, n);
	}

	QString numberOf(Conductor *conductor)
	{
		const QString number = conductor->properties().text.trimmed();
		return ProjectNumbering::isNoWireNumber(number) ? QString() : number;
	}

	WiringCheck::Issue issue(WiringCheck::Severity severity, Conductor *conductor, const QString &text)
	{
		WiringCheck::Issue i;
		i.severity = severity;
		i.conductor = conductor;
		if (conductor) {
			i.folio = WiringList::folioLabel(conductor->diagram());
			i.folio_index = conductor->diagram() ? conductor->diagram()->folioIndex() : -1;
			const QString number = numberOf(conductor);
			i.wire = number.isEmpty()
				? QStringLiteral("%1 → %2").arg(WiringList::terminalText(conductor, true),
								WiringList::terminalText(conductor, false))
				: number;
		}
		i.text = text;
		return i;
	}
}

QString WiringCheck::severityName(Severity severity)
{
	switch (severity) {
		case Error:   return tr("Erro");
		case Warning: return tr("Aviso");
		case Info:
		default:      return tr("Informação");
	}
}

/**
	@brief WiringCheck::check
	@return the issues of the wires of project, the errors first, then
	folio by folio
*/
QList<WiringCheck::Issue> WiringCheck::check(QETProject *project)
{
	QList<Issue> issues;
	if (!project) return issues;

	QList<Conductor *> conductors;
	for (Diagram *diagram : project->diagrams())
		conductors << diagram->conductors();

		//The potentials: the wires connected together, through the
		//junctions and the folio reports
	QHash<Conductor *, int> potential_of;
	QList<QList<Conductor *>> potentials;
	for (Conductor *conductor : std::as_const(conductors)) {
		if (potential_of.contains(conductor)) continue;
		QList<Conductor *> members {conductor};
		for (Conductor *other : conductor->relatedPotentialConductors())
			if (!potential_of.contains(other)) members << other;
		for (Conductor *member : std::as_const(members))
			potential_of.insert(member, int(potentials.size()));
		potentials << members;
	}

		//Numbers
	bool numbered = project->numbersWires();
	for (Conductor *conductor : std::as_const(conductors))
		if (!numbered && !numberOf(conductor).isEmpty()) numbered = true;
	if (numbered) {
		QHash<QString, QList<int>> potentials_of_number;
		for (int p = 0; p < potentials.size(); ++p) {
			QStringList numbers;
			for (Conductor *member : potentials.at(p)) {
				const QString number = numberOf(member);
				if (number.isEmpty()) {
					issues << issue(Warning, member, tr("Fio sem número"));
				} else if (!numbers.contains(number)) {
					numbers << number;
				}
			}
			for (const QString &number : std::as_const(numbers))
				potentials_of_number[number] << p;
			if (numbers.size() > 1) {
				numbers.sort();
				issues << issue(Warning, potentials.at(p).constFirst(),
						tr("Fios ligados entre si com números diferentes: %1")
						.arg(numbers.join(QStringLiteral(", "))));
			}
		}
			//A name (0V, PE, +24V-DI) may go on several potentials, linked
			//through terminals or a supply: only a wire number is unique
		for (auto it = potentials_of_number.constBegin(); it != potentials_of_number.constEnd(); ++it) {
			if (it.value().size() < 2 || !ProjectNumbering::isWireNumber(it.key())) continue;
			for (int p : it.value())
				issues << issue(Error, potentials.at(p).constFirst(),
						tr("O número %1 está em %2 potenciais diferentes (fios não ligados entre si)")
						.arg(it.key()).arg(it.value().size()));
		}
	}

		//Colour and section, when the project gives them to most of its
		//wires: then the others were forgotten
	int colored = 0, sized = 0;
	for (Conductor *conductor : std::as_const(conductors)) {
		colored += !conductor->properties().m_wire_color.trimmed().isEmpty();
		sized += !conductor->properties().m_wire_section.trimmed().isEmpty();
	}
	const bool colors = colored * 2 > conductors.size();
	const bool sections = sized * 2 > conductors.size();
	for (Conductor *conductor : std::as_const(conductors)) {
		const bool no_color = colors && conductor->properties().m_wire_color.trimmed().isEmpty();
		const bool no_section = sections && conductor->properties().m_wire_section.trimmed().isEmpty();
		if (no_color && no_section)
			issues << issue(Info, conductor, tr("Fio sem cor e sem seção"));
		else if (no_color)
			issues << issue(Info, conductor, tr("Fio sem cor"));
		else if (no_section)
			issues << issue(Info, conductor, tr("Fio sem seção"));
	}

		//Wires per terminal
	const WiringRules::Settings rules = project->wiringRules();
	const bool master = WiringRules::masterEnabled();
	for (Diagram *diagram : project->diagrams()) {
		for (Element *element : diagram->elements()) {
			if (WireJunction::isJunction(element)) continue;
			const bool report = element->linkType() & Element::AllReport;
			const int limit = WiringRules::limit(rules, master, report);
			if (limit <= 0) continue;
			for (Terminal *terminal : element->terminals()) {
				const QList<Conductor *> wires = terminal->conductors();
				if (wires.size() <= limit) continue;
				Issue i = issue(Error, wires.constFirst(),
						tr("O borne %1 tem %2 fios; o limite é %3")
						.arg(ConductorEndLabeling::terminalCrossRefId(terminal))
						.arg(wires.size()).arg(limit));
				issues << i;
			}
		}
	}

		//End labels: one line for many, they are fixed all at once
	QList<Conductor *> labels_off;
	for (Conductor *conductor : std::as_const(conductors))
		if (ConductorEndLabeling::needsLabels(conductor))
			labels_off << conductor;
	if (labels_off.size() > 20) {
		Issue i = issue(Info, labels_off.constFirst(), QString());
		i.folio.clear();
		i.wire.clear();
		i.folio_index = -1;
		i.text = tr("%n fio(s) com a etiqueta das pontas diferente da regra do projeto: use"
			    " Fiação > Etiquetar as pontas de todos os fios", nullptr, int(labels_off.size()));
		issues << i;
	} else {
		for (Conductor *conductor : std::as_const(labels_off))
			issues << issue(Info, conductor, tr("Etiqueta das pontas diferente da regra do projeto"));
	}

		//Left out of the database
	if (project->dataBase()) {
		const int excluded = project->dataBase()->excludedConductorCount();
		if (excluded > 0) {
			Issue i;
			i.severity = Warning;
			i.text = tr("%n fio(s) fora do banco do projeto e das listas geradas: uma ponta não está"
				    " ligada a nenhum elemento", nullptr, excluded);
			issues << i;
		}
	}

	QCollator collator;
	collator.setNumericMode(true);
	std::stable_sort(issues.begin(), issues.end(), [&collator](const Issue &a, const Issue &b) {
		if (a.severity != b.severity) return a.severity < b.severity;
		if (a.folio_index != b.folio_index) return a.folio_index < b.folio_index;
		return collator.compare(a.wire, b.wire) < 0;
	});
	return issues;
}
