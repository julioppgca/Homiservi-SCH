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
#include "wiringlist.h"

#include "conductorendlabeling.h"
#include "diagram.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetmessagebox.h"
#include "qetproject.h"
#include "wirejunction.h"

#include <QCollator>
#include <QSet>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QSaveFile>
#include <QTextDocument>
#include <QTextStream>

#include <algorithm>

namespace {
	Element *reportOf(Terminal *terminal)
	{
		Element *element = terminal ? terminal->parentElement() : nullptr;
		return element && (element->linkType() & Element::AllReport) ? element : nullptr;
	}

		///The wire on the other side of the folio report at at_report (its
		///only wire), and in far its end away from the report; nullptr when
		///the report is not such a one-to-one continuation
	Conductor *across(Terminal *at_report, Terminal **far)
	{
		Element *report = reportOf(at_report);
		if (!report || report->linkedElements().size() != 1) return nullptr;
		Element *other = report->linkedElements().first();
		QList<Conductor *> wires;
		for (Terminal *terminal : other->terminals()) wires << terminal->conductors();
		if (wires.size() != 1) return nullptr;
		Conductor *wire = wires.first();
		Terminal *end = wire->terminal1 && wire->terminal1->parentElement() == other
				? wire->terminal2 : wire->terminal1;
		if (!end || reportOf(end)) return nullptr;
		*far = end;
		return wire;
	}

	QString tr(const char *text)
	{
		return QCoreApplication::translate("WiringList", text);
	}

	QString csvCell(QString text)
	{
		text.replace(QLatin1Char('"'), QLatin1String("\"\""));
		return QLatin1Char('"') + text + QLatin1Char('"');
	}

	QString html(const QString &text)
	{
		return text.toHtmlEscaped();
	}

		///Where a file of the project goes by default: next to it
	QString defaultPath(QETProject *project, const QString &suffix)
	{
		if (!project || project->filePath().isEmpty()) return QString();
		const QFileInfo info(project->filePath());
		return info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + suffix;
	}

	bool write(QWidget *parent, const QString &title, const QString &path, const QString &text, bool bom)
	{
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QET::QetMessageBox::critical(parent, title, tr("Não foi possível abrir o arquivo para escrita."));
			return false;
		}
		QTextStream out(&file);
		out.setEncoding(QStringConverter::Utf8);
		out.setGenerateByteOrderMark(bom);	// accents in Excel
		out << text;
		if (!file.commit()) {
			QET::QetMessageBox::critical(parent, title, tr("Não foi possível gravar o arquivo."));
			return false;
		}
		return true;
	}
}

/**
	@return the number of diagram as its title block shows it (02.04), or
	its position when it has none
*/
QString WiringList::folioLabel(Diagram *diagram)
{
	if (!diagram) return QString();
	const QString folio = diagram->border_and_titleblock.finalfolio().trimmed();
	return folio.isEmpty() ? QString::number(diagram->folioIndex() + 1) : folio;
}

/**
	@return the terminal at the end of conductor at terminal1 (end1) or at
	terminal2: -K1:A1, or the label alone for a terminal without a name
*/
QString WiringList::terminalText(Conductor *conductor, bool end1)
{
	return terminalText(conductor ? (end1 ? conductor->terminal1 : conductor->terminal2) : nullptr);
}

QString WiringList::terminalText(Terminal *terminal)
{
	if (!terminal) return QString();
	if (WireJunction::isJunction(terminal)) return tr("(junção)");
	Element *element = terminal->parentElement();
		//A folio report is no terminal of the panel: where the wire goes on
	if (element && (element->linkType() & Element::AllReport))
		return ConductorEndLabeling::terminalCrossRefId(terminal);
	const QString label = element ? element->actualLabel() : QString();
	const QString name = terminal->name();
	if (name.isEmpty() || name == QLatin1String("_"))
		return label;
	return label.isEmpty() ? name : QStringLiteral("%1:%2").arg(label, name);
}

WiringList::Wire WiringList::wire(Conductor *conductor)
{
	Wire w;
	if (!conductor) return w;
	w.conductor = conductor;
	Diagram *diagram = conductor->diagram();
	w.folio = folioLabel(diagram);
	w.folio_index = diagram ? diagram->folioIndex() : 0;
	const ConductorProperties p = conductor->properties();
	w.number = p.text == QLatin1String("_") ? QString() : p.text;
	w.from = terminalText(conductor, true);
	w.to = terminalText(conductor, false);
	w.from_label = ConductorEndLabeling::shownLabel(conductor, true);
	w.to_label = ConductorEndLabeling::shownLabel(conductor, false);
	w.function = p.m_function;
	w.color = p.m_wire_color;
	w.section = p.m_wire_section;
	w.cable = p.m_cable;
	w.voltage = p.m_tension_protocol;
	w.bus = p.m_bus;
	return w;
}

/**
	@return the wires of project, folio by folio, by number in a folio
*/
QList<WiringList::Wire> WiringList::wires(QETProject *project)
{
	QList<Wire> list;
	if (!project) return list;
	QSet<Element *> done;
	QSet<Conductor *> continued;
	QCollator collator;
	collator.setNumericMode(true);
	for (Diagram *diagram : project->diagrams()) {
		QList<Conductor *> conductors = diagram->conductors();
		std::sort(conductors.begin(), conductors.end(), [&collator](Conductor *a, Conductor *b) {
			return collator.compare(a->properties().text, b->properties().text) < 0;
		});
		for (Conductor *conductor : std::as_const(conductors)) {
			Terminal *joint = WireJunction::isJunction(conductor->terminal1) ? conductor->terminal1
					: WireJunction::isJunction(conductor->terminal2) ? conductor->terminal2
					: nullptr;
			if (!joint) {
				if (continued.contains(conductor)) continue;
					//A wire drawn in two halves on two folios, joined by
					//folio reports (a terminal strip and the circuit it
					//feeds): one wire of the panel, from end to end
				Terminal *at_report = reportOf(conductor->terminal1) ? conductor->terminal1
						: reportOf(conductor->terminal2) ? conductor->terminal2 : nullptr;
				Terminal *far = nullptr;
				Conductor *other = at_report && !(reportOf(conductor->terminal1) && reportOf(conductor->terminal2))
						? across(at_report, &far) : nullptr;
				Wire w = wire(conductor);
				if (other) {
					continued << other;
					const bool end1 = at_report == conductor->terminal1;
					const QString text = terminalText(far);
					const QString label = ConductorEndLabeling::shownLabel(other, other->terminal1 == far);
					if (end1) { w.from = text; w.from_label = label; }
					else { w.to = text; w.to_label = label; }
					const Wire o = wire(other);
					if (w.cable.isEmpty()) w.cable = o.cable;
					if (w.color.isEmpty()) w.color = o.color;
					if (w.section.isEmpty()) w.section = o.section;
				}
				list << w;
				continue;
			}
				//The wires of a junction, once: the wires of the panel
				//between the terminals it joins, one after the other
			if (done.contains(joint->parentElement())) continue;
			QList<Element *> junctions;
			const QList<WireJunction::End> ends = WireJunction::ends(joint, &junctions);
			for (Element *junction : std::as_const(junctions)) done << junction;
			if (ends.size() < 2) {
				if (!ends.isEmpty()) list << wire(ends.first().conductor);
				continue;
			}
			for (int i = 0 ; i + 1 < ends.size() ; ++i) {
				const WireJunction::End &from = ends.at(i);
				const WireJunction::End &to = ends.at(i + 1);
				Wire w = wire(from.conductor);
				w.from = terminalText(from.terminal);
				w.to = terminalText(to.terminal);
				if (ends.size() == 2) {
						//The labels drawn at the two ends
					w.from_label = ConductorEndLabeling::shownLabel(from.conductor,
							from.conductor->terminal1 == from.terminal);
					w.to_label = ConductorEndLabeling::shownLabel(to.conductor,
							to.conductor->terminal1 == to.terminal);
				} else {
					const PotentialRules::EndLabels rule = ConductorEndLabeling::rule(from.conductor);
					w.from_label = ConductorEndLabeling::label(from.terminal, to.terminal, from.conductor, rule);
					w.to_label = ConductorEndLabeling::label(to.terminal, from.terminal, to.conductor, rule);
				}
				list << w;
			}
		}
	}
	return list;
}

QStringList WiringList::csvHeader()
{
	return {tr("Folha"), tr("Nº do fio"), tr("Origem"), tr("Destino"),
		tr("Etiqueta da origem"), tr("Etiqueta do destino"), tr("Função"), tr("Cor"),
		tr("Seção (mm²)"), tr("Cabo"), tr("Tensão / protocolo"), tr("Barramento")};
}

/**
	@return wires as CSV: semicolons, what Excel expects with a Brazilian
	locale, every cell quoted
*/
QString WiringList::toCsv(const QList<Wire> &wires)
{
	QString text;
	QTextStream out(&text);
	QStringList header;
	for (const QString &title : csvHeader()) header << csvCell(title);
	out << header.join(QLatin1Char(';')) << '\n';
	for (const Wire &w : wires) {
		QStringList line;
		for (const QString &cell : {w.folio, w.number, w.from, w.to, w.from_label, w.to_label,
					    w.function, w.color, w.section, w.cable, w.voltage, w.bus})
			line << csvCell(cell);
		out << line.join(QLatin1Char(';')) << '\n';
	}
	return text;
}

/**
	@return the tag list to print: a row per end of wire, with the label
	to print on its ferrule
*/
QString WiringList::tagListHtml(const QString &title, const QList<Wire> &wires)
{
	QString text;
	QTextStream out(&text);
	out << "<html><head><style>"
	       "body { font-family: sans-serif; font-size: 9pt; }"
	       "h2 { font-size: 12pt; }"
	       "table { border-collapse: collapse; width: 100%; }"
	       "th, td { border-bottom: 1px solid #bbbbbb; padding: 3px 6px; text-align: left; }"
	       "th { border-bottom: 2px solid #000000; color: #555555; }"
	       "td.tag { font-weight: bold; }"
	       "</style></head><body>";
	out << "<h2>" << html(tr("Lista de marcação de fios")) << " — " << html(title) << "</h2>";
	out << "<table><thead><tr>";
	for (const char *column : {"Folha", "Nº do fio", "Ponta ligada em", "Etiqueta",
				   "Cor", "Seção (mm²)", "Função"})
		out << "<th>" << html(tr(column)) << "</th>";
	out << "</tr></thead><tbody>";
	for (const Wire &w : wires) {
		for (bool from : {true, false}) {
			const QString &label = from ? w.from_label : w.to_label;
			if (label.isEmpty()) continue;
			out << "<tr><td>" << html(w.folio) << "</td><td>" << html(w.number) << "</td><td>"
			    << html(from ? w.from : w.to) << "</td><td class=\"tag\">" << html(label) << "</td><td>"
			    << html(w.color) << "</td><td>" << html(w.section) << "</td><td>"
			    << html(w.function) << "</td></tr>";
		}
	}
	out << "</tbody></table></body></html>";
	return text;
}

/**
	@return the labels to print, one per line, for the software of a
	label printer
*/
QString WiringList::tagListCsv(const QList<Wire> &wires)
{
	QString text;
	QTextStream out(&text);
	for (const Wire &w : wires)
		for (const QString &label : {w.from_label, w.to_label})
			if (!label.isEmpty()) out << label << '\n';
	return text;
}

/**
	@brief WiringList::saveCsv
	Ask where and write wires as CSV
*/
bool WiringList::saveCsv(QWidget *parent, QETProject *project, const QList<Wire> &wires)
{
	const QString title = tr("Exportar a lista de fiação");
	if (wires.isEmpty()) {
		QET::QetMessageBox::information(parent, title, tr("O projeto não tem fios."));
		return false;
	}
	const QString path = QFileDialog::getSaveFileName(parent, title, defaultPath(project, tr("-fiacao.csv")),
							  tr("Arquivos CSV (*.csv)"));
	return !path.isEmpty() && write(parent, title, path, toCsv(wires), true);
}

void WiringList::printTagList(QWidget *parent, QETProject *project)
{
	const QList<Wire> list = wires(project);
	if (list.isEmpty()) {
		QET::QetMessageBox::information(parent, tr("Lista de marcação de fios"), tr("O projeto não tem fios."));
		return;
	}
	QTextDocument document;
	document.setHtml(tagListHtml(project->title(), list));
	QPrinter printer(QPrinter::HighResolution);
	printer.setPageOrientation(QPageLayout::Portrait);
	QPrintPreviewDialog preview(&printer, parent);
	preview.setWindowTitle(tr("Lista de marcação de fios"));
	preview.resize(1000, 700);
	QObject::connect(&preview, &QPrintPreviewDialog::paintRequested, &preview,
			 [&document](QPrinter *p) { document.print(p); });
	preview.exec();
}

bool WiringList::saveTagListCsv(QWidget *parent, QETProject *project)
{
	const QString title = tr("Exportar etiquetas para a impressora");
	const QList<Wire> list = wires(project);
	if (list.isEmpty()) {
		QET::QetMessageBox::information(parent, title, tr("O projeto não tem fios."));
		return false;
	}
	const QString path = QFileDialog::getSaveFileName(parent, title, defaultPath(project, tr("-etiquetas.csv")),
							  tr("Arquivos CSV (*.csv)"));
	return !path.isEmpty() && write(parent, title, path, tagListCsv(list), false);
}
