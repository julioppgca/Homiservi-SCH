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
#include "projectnumberingwizard.h"

#include "../diagram.h"
#include "../projectrevisions.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/independenttextitem.h"
#include "../qetproject.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <functional>
#include <numeric>

namespace
{
		///A page whose validatePage() asks a function
	class ValidatedPage : public QWizardPage
	{
		public:
			std::function<bool()> validate;
			bool validatePage() override { return !validate || validate(); }
	};

	QTableWidgetItem *readOnlyItem(const QString &text)
	{
		auto item = new QTableWidgetItem(text);
		item->setFlags(item->flags() & ~Qt::ItemIsEditable);
		return item;
	}

		///The separator of each entry of the format list; "seq" for sequential
	const QStringList format_keys {QStringLiteral("."), QStringLiteral("-"), QString(), QStringLiteral("seq")};
}

ProjectNumberingWizard::ProjectNumberingWizard(QETProject *project, bool reapply, QWidget *parent) :
	QWizard(parent),
	m_project(project)
{
	setWindowTitle(tr("Numeração do projeto"));
	setWizardStyle(QWizard::ClassicStyle);
	setButtonText(QWizard::FinishButton, tr("Aplicar"));
	setMinimumSize(820, 600);

	m_folios = project->numberingFolios();
	for (Diagram *diagram : project->diagrams())
		for (QGraphicsItem *item : diagram->items())
			if (auto text = qgraphicsitem_cast<IndependentTextItem *>(item))
				m_texts << text->toPlainText();

	QList<Element *> elements;
	const QList<ProjectNumbering::ComponentInfo> components = project->numberingComponents(&elements);
	for (int i = 0; i < components.size(); ++i)
		m_components << Component {elements.at(i), components.at(i).folio, components.at(i)};
	QList<QList<Conductor *>> wires;
	const QList<ProjectNumbering::PotentialInfo> potentials = project->numberingPotentials(&wires);
	for (int i = 0; i < potentials.size(); ++i)
		m_potentials << Potential {wires.at(i), potentials.at(i).folio, potentials.at(i)};
	m_has_revisions = !ProjectRevisions::read(project).isEmpty();
	m_reapply = reapply;

	m_settings = project->numbering();
	if (m_settings.sections.isEmpty())
		m_settings = ProjectNumbering::importFrom(m_folios, m_texts);
	m_codes = ProjectNumbering::sectionCodes(m_settings, m_folios);

	setPage(SectionsPage, sectionsPage());
	setPage(FoliosPage, foliosPage());
	setPage(ComponentsPage, componentsPage());
	setPage(WiresPage, wiresPage());
	setPage(PreviewPage, previewPage());

	fillSectionsTable();
	fillFoliosTable();
	connect(this, &QWizard::currentIdChanged, this, [this](int id) {
		if (id == FoliosPage) {
			fillFoliosTable();
		} else if (id == ComponentsPage) {
			readFormat();
			fillPrefixTable();
		} else if (id == WiresPage) {
			readFormat();
			fillWireSummary();
		} else if (id == PreviewPage) {
			readFormat();
			fillPreview();
		}
	});
	if (reapply) {
		setStartId(PreviewPage);
		readFormat();
		fillPreview();
	}
}

/**
	@return the settings to give the project, its sections starting at
	the folios in their new order
*/
ProjectNumbering::Settings ProjectNumberingWizard::settings() const
{
	ProjectNumbering::Settings settings = m_settings;
	QList<ProjectNumbering::FolioInfo> folios;
	for (int index : order())
		folios << m_folios.at(index);
	settings.starts = ProjectNumbering::startsOf(folios, orderedCodes());
	return settings;
}

QList<int> ProjectNumberingWizard::order() const
{
	if (m_group && !m_group->isHidden() && m_group->isChecked())
		return ProjectNumbering::groupedOrder(m_settings, m_codes);
	QList<int> identity(m_codes.size());
	std::iota(identity.begin(), identity.end(), 0);
	return identity;
}

QStringList ProjectNumberingWizard::titles() const
{
	QStringList titles;
	for (const ProjectNumbering::FolioInfo &folio : m_folios)
		titles << (m_settings.enabled && m_settings.strip_titles
			   ? ProjectNumbering::strippedTitle(folio.title) : folio.title);
	return titles;
}

QStringList ProjectNumberingWizard::labels() const
{
	if (m_settings.enabled)
		return ProjectNumbering::folioLabels(m_settings, orderedCodes());
	QStringList labels;
	for (int index : order())
		labels << m_folios.at(index).folio;
	return labels;
}

QWizardPage *ProjectNumberingWizard::sectionsPage()
{
	auto page = new ValidatedPage;
	page->setTitle(tr("Seções"));
	page->setSubTitle(tr("As seções agrupam as folhas: \"02 – Alimentação e distribuição\". "
			     "O número da seção abre o número de cada folha dela: 02.01, 02.02..."));

	m_sections_table = new QTableWidget(0, 2, page);
	m_sections_table->setHorizontalHeaderLabels({tr("Número"), tr("Nome")});
	m_sections_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_sections_table->horizontalHeader()->setStretchLastSection(true);
	m_sections_table->verticalHeader()->hide();
	m_sections_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_sections_table->setSelectionMode(QAbstractItemView::SingleSelection);

	auto add = new QPushButton(tr("Adicionar"), page);
	auto remove = new QPushButton(tr("Remover"), page);
	auto up = new QPushButton(tr("Subir"), page);
	auto down = new QPushButton(tr("Descer"), page);
	auto import = new QPushButton(tr("Ler do projeto"), page);
	import->setToolTip(tr("Lê as seções dos números das folhas (02.04) ou do começo dos títulos "
			      "(\"02.04 – ...\"), e os nomes de um texto que liste \"02 – Nome\""));

	connect(add, &QPushButton::clicked, this, [this]() {
		readSectionsTable();
		int next = 1;
		for (const ProjectNumbering::Section &section : std::as_const(m_settings.sections))
			next = qMax(next, section.code.toInt() + 1);
		m_settings.sections << ProjectNumbering::Section {
			QString::number(next).rightJustified(m_settings.section_digits, QLatin1Char('0')), QString()};
		fillSectionsTable();
		m_sections_table->setCurrentCell(m_sections_table->rowCount() - 1, 1);
		m_sections_table->editItem(m_sections_table->currentItem());
	});
	connect(remove, &QPushButton::clicked, this, [this]() {
		const int row = m_sections_table->currentRow();
		if (row < 0) return;
		readSectionsTable();
		m_settings.sections.removeAt(row);
		fillSectionsTable();
	});
	auto move = [this](int delta) {
		const int row = m_sections_table->currentRow();
		readSectionsTable();
		if (row < 0 || row + delta < 0 || row + delta >= m_settings.sections.size()) return;
		m_settings.sections.move(row, row + delta);
		fillSectionsTable();
		m_sections_table->setCurrentCell(row + delta, 0);
	};
	connect(up, &QPushButton::clicked, this, [move]() { move(-1); });
	connect(down, &QPushButton::clicked, this, [move]() { move(1); });
	connect(import, &QPushButton::clicked, this, &ProjectNumberingWizard::importSections);

	auto buttons = new QVBoxLayout;
	for (QPushButton *button : QList<QPushButton *> {add, remove, up, down})
		buttons->addWidget(button);
	buttons->addStretch();
	buttons->addWidget(import);

	auto layout = new QHBoxLayout(page);
	layout->addWidget(m_sections_table, 1);
	layout->addLayout(buttons);

	page->validate = [this]() {
		readSectionsTable();
		QStringList codes;
		for (const ProjectNumbering::Section &section : std::as_const(m_settings.sections)) {
			if (codes.contains(section.code)) {
				QMessageBox::warning(this, windowTitle(),
						     tr("Duas seções têm o número %1.").arg(section.code));
				return false;
			}
			codes << section.code;
		}
		if (codes.isEmpty()) {
			QMessageBox::warning(this, windowTitle(), tr("Crie ao menos uma seção."));
			return false;
		}
		return true;
	};
	return page;
}

QWizardPage *ProjectNumberingWizard::foliosPage()
{
	auto page = new QWizardPage;
	page->setTitle(tr("Folhas"));
	page->setSubTitle(tr("Escolha a seção de cada folha e o formato do número. Selecione várias "
			     "linhas para mudar a seção de todas de uma vez."));

	m_enabled = new QCheckBox(tr("Numerar as folhas automaticamente (o número é recalculado "
				     "ao inserir, mover ou excluir uma folha)"), page);
	m_format = new QComboBox(page);
	m_format->addItem(tr("Seção.Página – 02.04"));
	m_format->addItem(tr("Seção-Página – 02-04"));
	m_format->addItem(tr("Seção e página juntos – 0204"));
	m_format->addItem(tr("Sequencial – 1, 2, 3..."));
	m_section_digits = new QSpinBox(page);
	m_section_digits->setRange(1, 4);
	m_page_digits = new QSpinBox(page);
	m_page_digits->setRange(1, 4);
	m_strip_titles = new QCheckBox(tr("Tirar o número do começo dos títulos "
					  "(\"02.04 – Fonte 24 Vcc\" vira \"Fonte 24 Vcc\")"), page);

	m_enabled->setChecked(m_settings.enabled);
	m_format->setCurrentIndex(m_settings.format == ProjectNumbering::Sequential
				  ? 3 : qMax(0, int(format_keys.indexOf(m_settings.separator))));
	m_section_digits->setValue(m_settings.section_digits);
	m_page_digits->setValue(m_settings.page_digits);
	m_strip_titles->setChecked(m_settings.strip_titles);

	auto changed = [this]() { readFormat(); updateNewNumbers(); };
	connect(m_enabled, &QCheckBox::toggled, this, changed);
	connect(m_format, &QComboBox::currentIndexChanged, this, changed);
	connect(m_section_digits, &QSpinBox::valueChanged, this, changed);
	connect(m_page_digits, &QSpinBox::valueChanged, this, changed);
	connect(m_strip_titles, &QCheckBox::toggled, this, changed);

	auto digits = new QHBoxLayout;
	digits->addWidget(new QLabel(tr("Dígitos da seção:"), page));
	digits->addWidget(m_section_digits);
	digits->addSpacing(16);
	digits->addWidget(new QLabel(tr("da página:"), page));
	digits->addWidget(m_page_digits);
	digits->addStretch();

	auto form = new QFormLayout;
	form->addRow(m_enabled);
	form->addRow(tr("Formato:"), m_format);
	form->addRow(digits);
	form->addRow(m_strip_titles);

	m_folios_table = new QTableWidget(0, 4, page);
	m_folios_table->setHorizontalHeaderLabels({tr("Folha"), tr("Título"), tr("Seção"), tr("Novo número")});
	m_folios_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_folios_table->verticalHeader()->hide();
	m_folios_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_folios_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_folios_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

	auto layout = new QVBoxLayout(page);
	layout->addLayout(form);
	layout->addWidget(m_folios_table, 1);
	return page;
}

/**
	@return the components whose tag changes and their new tag, with the
	folios in their new order and numbers
*/
QList<QPair<Element *, QString>> ProjectNumberingWizard::componentChanges() const
{
	const QHash<QString, QString> changes = ProjectNumbering::componentLabels(m_settings, components(), tagScope());
	QList<QPair<Element *, QString>> list;
	for (const Component &component : m_components)
		if (changes.contains(component.info.id))
			list << qMakePair(component.element, changes.value(component.info.id));
	return list;
}

/**
	@return the components as ProjectNumbering takes them: their folio in
	its new place, with its new number and section
*/
QList<ProjectNumbering::ComponentInfo> ProjectNumberingWizard::components() const
{
	const QList<int> new_order = order();
	const QStringList new_labels = labels();
	const QStringList codes = orderedCodes();
	QList<ProjectNumbering::ComponentInfo> list;
	for (const Component &component : m_components) {
		ProjectNumbering::ComponentInfo info = component.info;
		info.folio = int(new_order.indexOf(component.folio));
		info.folio_label = new_labels.value(info.folio);
		info.section = codes.value(info.folio);
		list << info;
	}
	return list;
}

ProjectNumbering::TagScope ProjectNumberingWizard::tagScope() const
{
	return m_complete_only && m_complete_only->isChecked() ? ProjectNumbering::CompleteOnly
							       : ProjectNumbering::RenumberAll;
}

QWizardPage *ProjectNumberingWizard::componentsPage()
{
	auto page = new QWizardPage;
	page->setTitle(tr("Componentes"));
	page->setSubTitle(tr("A tag de um componente é o prefixo (-KA, -F, -Q...) seguido de um número. "
			     "Os contatos mostram a tag da bobina e acompanham."));

	m_tag_mode = new QComboBox(page);
	m_tag_mode->addItem(tr("Não mexer nas tags"));
	m_tag_mode->addItem(tr("Sequencial por tipo – -KA01, -KA02..."));
	m_tag_mode->addItem(tr("Sequencial por tipo em cada seção – -KA0201 (seção 02)"));
	m_tag_mode->addItem(tr("Sequencial por tipo em cada seção, compacto – -KA21 (seção 02)"));
	m_tag_mode->addItem(tr("Pela posição – -KA0204.3 (folha 02.04, coluna 3)"));
		//Never configured: the style the tags already follow
	int digits = m_settings.tag_digits;
	const ProjectNumbering::TagMode guessed = ProjectNumbering::guessTagMode(components(), &digits);
	if (m_project->numbering().tag_mode == ProjectNumbering::TagsUnchanged
	    && guessed != ProjectNumbering::TagsUnchanged) {
		m_settings.tag_mode = guessed;
		m_settings.tag_digits = digits;
	}
	m_tag_mode->setCurrentIndex(m_settings.tag_mode);
	m_tag_digits = new QSpinBox(page);
	m_tag_digits->setRange(1, 4);
	m_tag_digits->setValue(m_settings.tag_digits);
	m_terminals = new QCheckBox(tr("Numerar os bornes de cada régua na ordem do desenho "
				       "(-X1:1, -X1:2...; -X1:PE e -X1:L1 ficam)"), page);
	m_terminals->setChecked(m_settings.renumber_terminals);

	auto scope = new QGroupBox(tr("Ao aplicar"), page);
	m_renumber_all = new QRadioButton(tr("Renumerar todas as tags"), scope);
	m_complete_only = new QRadioButton(tr("Só completar as tags sem número "
					      "(um componente com \"-KA\" ganha o próximo número livre)"), scope);
		//A project already tagged keeps its tags unless asked: some may be
		//numbered on purpose out of the rule
	(m_reapply || guessed != ProjectNumbering::TagsUnchanged ? m_complete_only : m_renumber_all)->setChecked(true);
	auto scope_layout = new QVBoxLayout(scope);
	scope_layout->addWidget(m_renumber_all);
	scope_layout->addWidget(m_complete_only);

	auto changed = [this]() { readTagSettings(); fillPrefixTable(); };
	connect(m_tag_mode, &QComboBox::currentIndexChanged, this, changed);
	connect(m_tag_digits, &QSpinBox::valueChanged, this, changed);
	connect(m_terminals, &QCheckBox::toggled, this, changed);
	connect(m_renumber_all, &QRadioButton::toggled, this, changed);

	m_prefix_table = new QTableWidget(0, 4, page);
	m_prefix_table->setHorizontalHeaderLabels({tr("Prefixo"), tr("Componentes"), tr("Hoje"), tr("Depois")});
	m_prefix_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	m_prefix_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
	m_prefix_table->verticalHeader()->hide();
	m_prefix_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_other_labels = new QLabel(page);
	m_other_labels->setWordWrap(true);

	auto form = new QFormLayout;
	form->addRow(tr("Tags:"), m_tag_mode);
	form->addRow(tr("Dígitos do número:"), m_tag_digits);
	form->addRow(m_terminals);

	auto layout = new QVBoxLayout(page);
	layout->addLayout(form);
	layout->addWidget(scope);
	layout->addWidget(m_prefix_table, 1);
	layout->addWidget(m_other_labels);
	return page;
}

void ProjectNumberingWizard::readTagSettings()
{
	if (!m_tag_mode) return;
	m_settings.tag_mode = ProjectNumbering::TagMode(m_tag_mode->currentIndex());
	m_settings.tag_digits = m_tag_digits->value();
	m_settings.renumber_terminals = m_terminals->isChecked();
	m_tag_digits->setEnabled(m_settings.tag_mode != ProjectNumbering::TagsUnchanged
				 && m_settings.tag_mode != ProjectNumbering::TagsByPosition);
}

/**
	@brief ProjectNumberingWizard::fillPrefixTable
	The prefixes of the project, how many components have each and a few
	of their tags before and after
*/
void ProjectNumberingWizard::fillPrefixTable()
{
	readTagSettings();
	const QList<ProjectNumbering::ComponentInfo> list = components();
	const QHash<QString, QString> changes = ProjectNumbering::componentLabels(m_settings, list, tagScope());

	QStringList prefixes;
	QHash<QString, QStringList> before, after;
	int others = 0;
	QStringList other_examples;
	for (const ProjectNumbering::ComponentInfo &info : list) {
		QString prefix = info.terminal ? info.label.section(QLatin1Char(':'), 0, 0) + QLatin1Char(':')
					       : ProjectNumbering::tagPrefix(info.label);
		if (info.terminal && !info.label.contains(QLatin1Char(':'))) prefix.clear();
		if (prefix.isEmpty()) {
			if (!info.label.trimmed().isEmpty()) {
				++others;
				if (other_examples.size() < 4 && !other_examples.contains(info.label))
					other_examples << info.label;
			}
			continue;
		}
		if (!prefixes.contains(prefix)) prefixes << prefix;
		before[prefix] << info.label;
		after[prefix] << changes.value(info.id, info.label);
	}
	prefixes.sort();

	auto examples = [](const QStringList &labels) {
		QStringList shown = labels.mid(0, 4);
		return shown.join(QStringLiteral(", ")) + (labels.size() > 4 ? QStringLiteral("...") : QString());
	};
	m_prefix_table->setRowCount(int(prefixes.size()));
	for (int row = 0; row < prefixes.size(); ++row) {
		const QString &prefix = prefixes.at(row);
		m_prefix_table->setItem(row, 0, readOnlyItem(prefix));
		m_prefix_table->setItem(row, 1, readOnlyItem(QString::number(before.value(prefix).size())));
		m_prefix_table->setItem(row, 2, readOnlyItem(examples(before.value(prefix))));
		m_prefix_table->setItem(row, 3, readOnlyItem(examples(after.value(prefix))));
	}
	m_prefix_table->resizeColumnToContents(0);
	m_prefix_table->resizeColumnToContents(1);
	m_other_labels->setText(others
				? tr("%1 componentes têm uma identificação que não é prefixo + número (%2) "
				     "e ficam como estão.").arg(others).arg(other_examples.join(QStringLiteral(", ")))
				: QString());
}

QList<QPair<QList<Conductor *>, QString>> ProjectNumberingWizard::wireChanges() const
{
	const QHash<QString, QString> changes = wireNumbers(potentials());
	QList<QPair<QList<Conductor *>, QString>> list;
	for (const Potential &potential : m_potentials)
		if (changes.contains(potential.info.id))
			list << qMakePair(potential.wires, changes.value(potential.info.id));
	return list;
}

/**
	@return the potentials as ProjectNumbering takes them: the folio of
	their first wire in its new place, with its new number
*/
QList<ProjectNumbering::PotentialInfo> ProjectNumberingWizard::potentials() const
{
	const QList<int> new_order = order();
	const QStringList new_labels = labels();
	QList<ProjectNumbering::PotentialInfo> list;
	for (const Potential &potential : m_potentials) {
		ProjectNumbering::PotentialInfo info = potential.info;
		info.folio = int(new_order.indexOf(potential.folio));
		info.folio_label = new_labels.value(info.folio);
		list << info;
	}
	return list;
}

/**
	@return the new number of every potential whose wires change, by id:
	ProjectNumbering::wireNumbers(), and a numbered potential with wires
	left without number gets its number on all of them
*/
QHash<QString, QString> ProjectNumberingWizard::wireNumbers(const QList<ProjectNumbering::PotentialInfo> &list) const
{
	QHash<QString, QString> changes = ProjectNumbering::wireNumbers(m_settings, list, wireScope());
	if (m_settings.wire_mode == ProjectNumbering::WiresUnchanged) return changes;
	for (int i = 0; i < m_potentials.size() && i < list.size(); ++i) {
		const ProjectNumbering::PotentialInfo &info = list.at(i);
		if (changes.contains(info.id) || info.number.trimmed().isEmpty()) continue;
		for (Conductor *wire : m_potentials.at(i).wires)
			if (wire->properties().text.trimmed().isEmpty()) {
				changes.insert(info.id, info.number.trimmed());
				break;
			}
	}
	return changes;
}

ProjectNumbering::TagScope ProjectNumberingWizard::wireScope() const
{
	return m_wires_complete && m_wires_complete->isChecked() ? ProjectNumbering::CompleteOnly
								 : ProjectNumbering::RenumberAll;
}

QWizardPage *ProjectNumberingWizard::wiresPage()
{
	auto page = new QWizardPage;
	page->setTitle(tr("Fios"));
	page->setSubTitle(tr("Os fios ligados entre si formam um potencial e têm o mesmo número. "
			     "Um potencial com nome (0V, +24V-DI, 1L1, X1.2) fica como está."));

	m_wire_mode = new QComboBox(page);
	m_wire_mode->addItem(tr("Não mexer nos números dos fios"));
	m_wire_mode->addItem(tr("Sequencial no projeto – 001, 002..."));
	m_wire_mode->addItem(tr("Por folha – 02.04-01, 02.04-02..."));
	m_wire_mode->addItem(tr("Pela coluna onde o fio começa – 02.04-3"));
	m_wire_mode->setCurrentIndex(m_settings.wire_mode);
	m_wire_digits = new QSpinBox(page);
	m_wire_digits->setRange(1, 4);
	m_wire_digits->setValue(m_settings.wire_digits);
	m_wire_compact = new QCheckBox(tr("Folha sem o separador (0204 em vez de 02.04)"), page);
	m_wire_compact->setChecked(m_settings.wire_compact_folio);
	m_wire_separator = new QComboBox(page);
	m_wire_separator->addItem(tr("hífen – 02.04-01"), QStringLiteral("-"));
	m_wire_separator->addItem(tr("ponto – 02.04.01"), QStringLiteral("."));
	m_wire_separator->addItem(tr("nada – 02.0401"), QString());
	m_wire_separator->setCurrentIndex(qMax(0, m_wire_separator->findData(m_settings.wire_separator)));

	bool has_numbers = false;
	for (const Potential &potential : std::as_const(m_potentials))
		has_numbers |= ProjectNumbering::isWireNumber(potential.info.number);
	auto scope = new QGroupBox(tr("Ao aplicar"), page);
	m_wires_all = new QRadioButton(tr("Renumerar todos os fios numerados"), scope);
	m_wires_complete = new QRadioButton(tr("Só numerar os fios sem número"), scope);
	(m_reapply || has_numbers ? m_wires_complete : m_wires_all)->setChecked(true);
	auto scope_layout = new QVBoxLayout(scope);
	scope_layout->addWidget(m_wires_all);
	scope_layout->addWidget(m_wires_complete);

	m_wires_summary = new QLabel(page);
	m_wires_summary->setWordWrap(true);
	m_wires_summary->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	auto changed = [this]() { readWireSettings(); fillWireSummary(); };
	connect(m_wire_mode, &QComboBox::currentIndexChanged, this, changed);
	connect(m_wire_digits, &QSpinBox::valueChanged, this, changed);
	connect(m_wire_compact, &QCheckBox::toggled, this, changed);
	connect(m_wire_separator, &QComboBox::currentIndexChanged, this, changed);
	connect(m_wires_all, &QRadioButton::toggled, this, changed);

	auto form = new QFormLayout;
	form->addRow(tr("Fios de comando:"), m_wire_mode);
	form->addRow(tr("Dígitos do número:"), m_wire_digits);
	form->addRow(tr("Entre a folha e o número:"), m_wire_separator);
	form->addRow(m_wire_compact);

	auto layout = new QVBoxLayout(page);
	layout->addLayout(form);
	layout->addWidget(scope);
	layout->addWidget(m_wires_summary, 1);
	return page;
}

void ProjectNumberingWizard::readWireSettings()
{
	if (!m_wire_mode) return;
	m_settings.wire_mode = ProjectNumbering::WireMode(m_wire_mode->currentIndex());
	m_settings.wire_digits = m_wire_digits->value();
	m_settings.wire_compact_folio = m_wire_compact->isChecked();
	m_settings.wire_separator = m_wire_separator->currentData().toString();
	const bool by_folio = m_settings.wire_mode == ProjectNumbering::WiresByFolio
			|| m_settings.wire_mode == ProjectNumbering::WiresByColumn;
	m_wire_digits->setEnabled(m_settings.wire_mode == ProjectNumbering::WiresSequential
				  || m_settings.wire_mode == ProjectNumbering::WiresByFolio);
	m_wire_compact->setEnabled(by_folio);
	m_wire_separator->setEnabled(by_folio);
}

/**
	@brief ProjectNumberingWizard::fillWireSummary
	How many potentials have a name, a number or none, and what the
	settings give them
*/
void ProjectNumberingWizard::fillWireSummary()
{
	readWireSettings();
	const QList<ProjectNumbering::PotentialInfo> list = potentials();
	const QHash<QString, QString> changes = wireNumbers(list);
	int named = 0, numbered = 0, empty = 0;
	QStringList names, numbers, examples;
	for (const ProjectNumbering::PotentialInfo &info : list) {
		const QString number = info.number.trimmed();
		if (number.isEmpty()) ++empty;
		else if (ProjectNumbering::isWireNumber(number)) {
			++numbered;
			if (numbers.size() < 5 && !numbers.contains(number)) numbers << number;
		} else {
			++named;
			if (names.size() < 6 && !names.contains(number)) names << number;
		}
		if (changes.contains(info.id) && examples.size() < 6)
			examples << QStringLiteral("%1 → %2").arg(number.isEmpty() ? tr("(sem número)") : number,
								  changes.value(info.id));
	}
	QString text = tr("%1 potenciais: %2 com nome (%3) ficam como estão; %4 com número (%5); %6 sem número.")
			.arg(list.size()).arg(named).arg(names.join(QStringLiteral(", ")))
			.arg(numbered).arg(numbers.join(QStringLiteral(", "))).arg(empty);
	text += QStringLiteral("\n\n");
	text += changes.isEmpty() ? tr("Nenhum número de fio muda.")
				  : tr("%1 potenciais mudam de número, por exemplo: %2")
				    .arg(changes.size()).arg(examples.join(QStringLiteral(", ")));
	m_wires_summary->setText(text);
}

QWizardPage *ProjectNumberingWizard::previewPage()
{
	auto page = new QWizardPage;
	page->setTitle(tr("Aplicar"));
	page->setSubTitle(tr("O que muda nas folhas. Tudo é desfeito com um único Ctrl+Z."));

	m_summary = new QLabel(page);
	m_summary->setWordWrap(true);
	m_group = new QCheckBox(tr("Agrupar as folhas por seção (move as folhas para a ordem das seções)"), page);
	m_group->setChecked(true);
	connect(m_group, &QCheckBox::toggled, this, &ProjectNumberingWizard::fillPreview);

	m_revision_warning = new QLabel(page);
	m_revision_warning->setWordWrap(true);
	m_revision_warning->setStyleSheet(QStringLiteral("color: #b00020;"));
	m_revision_warning->setText(tr("O projeto já tem revisões: renumerar tags muda identificações que podem "
				       "já estar no painel montado e no programa do CLP. Prefira abrir uma nova revisão."));

	m_preview_table = new QTableWidget(0, 5, page);
	m_preview_table->setHorizontalHeaderLabels({tr("Posição"), tr("Número atual"), tr("Novo número"),
						    tr("Título atual"), tr("Novo título")});
	m_preview_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
	m_preview_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
	m_preview_table->verticalHeader()->hide();
	m_preview_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

	m_components_table = new QTableWidget(0, 3, page);
	m_components_table->setHorizontalHeaderLabels({tr("Folha"), tr("Tag atual"), tr("Nova tag")});
	m_components_table->horizontalHeader()->setStretchLastSection(true);
	m_components_table->verticalHeader()->hide();
	m_components_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

	m_preview_tabs = new QTabWidget(page);
	m_preview_tabs->addTab(m_preview_table, tr("Folhas"));
	m_preview_tabs->addTab(m_components_table, tr("Componentes"));
	m_wires_table = new QTableWidget(0, 4, page);
	m_wires_table->setHorizontalHeaderLabels({tr("Folha"), tr("Número atual"), tr("Novo número"), tr("Fios")});
	m_wires_table->horizontalHeader()->setStretchLastSection(true);
	m_wires_table->verticalHeader()->hide();
	m_wires_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_preview_tabs->addTab(m_wires_table, tr("Fios"));

	auto layout = new QVBoxLayout(page);
	layout->addWidget(m_summary);
	layout->addWidget(m_revision_warning);
	layout->addWidget(m_group);
	layout->addWidget(m_preview_tabs, 1);
	return page;
}

/**
	@brief ProjectNumberingWizard::importSections
	Read the sections, and the section of every folio, from the numbers and
	the titles of the folios
*/
void ProjectNumberingWizard::importSections()
{
	const ProjectNumbering::Settings imported = ProjectNumbering::importFrom(m_folios, m_texts);
	m_settings.sections = imported.sections;
	m_settings.separator = imported.separator;
	m_settings.section_digits = imported.section_digits;
	m_settings.page_digits = imported.page_digits;
	m_codes = ProjectNumbering::sectionCodes(imported, m_folios);
	fillSectionsTable();
}

void ProjectNumberingWizard::fillSectionsTable()
{
	m_sections_table->setRowCount(int(m_settings.sections.size()));
	for (int row = 0; row < m_settings.sections.size(); ++row) {
		m_sections_table->setItem(row, 0, new QTableWidgetItem(m_settings.sections.at(row).code));
		m_sections_table->setItem(row, 1, new QTableWidgetItem(m_settings.sections.at(row).name));
	}
}

/**
	@brief ProjectNumberingWizard::readSectionsTable
	Take the sections from their table. A folio whose section is gone goes
	to the section of the folio before it.
*/
void ProjectNumberingWizard::readSectionsTable()
{
	QList<ProjectNumbering::Section> sections;
	for (int row = 0; row < m_sections_table->rowCount(); ++row) {
		const QString code = m_sections_table->item(row, 0) ? m_sections_table->item(row, 0)->text().trimmed() : QString();
		const QString name = m_sections_table->item(row, 1) ? m_sections_table->item(row, 1)->text().trimmed() : QString();
		if (!code.isEmpty())
			sections << ProjectNumbering::Section {code, name};
	}
		//A renamed section keeps its folios
	for (int row = 0; row < m_settings.sections.size() && row < sections.size(); ++row) {
		const QString old_code = m_settings.sections.at(row).code;
		if (old_code != sections.at(row).code)
			for (QString &code : m_codes)
				if (code == old_code) code = sections.at(row).code;
	}
	m_settings.sections = sections;

	QString previous = sections.isEmpty() ? QString() : sections.first().code;
	for (QString &code : m_codes) {
		if (m_settings.sectionIndex(code) < 0) code = previous;
		previous = code;
	}
}

QString ProjectNumberingWizard::sectionText(const ProjectNumbering::Section &section) const
{
	return section.name.isEmpty() ? section.code
				      : QStringLiteral("%1 – %2").arg(section.code, section.name);
}

void ProjectNumberingWizard::fillFoliosTable()
{
	m_filling = true;
	m_folios_table->setRowCount(int(m_folios.size()));
	for (int row = 0; row < m_folios.size(); ++row) {
		m_folios_table->setItem(row, 0, readOnlyItem(m_folios.at(row).folio));
		m_folios_table->setItem(row, 1, readOnlyItem(m_folios.at(row).title));
		auto combo = new QComboBox(m_folios_table);
		for (const ProjectNumbering::Section &section : std::as_const(m_settings.sections))
			combo->addItem(sectionText(section), section.code);
		combo->setCurrentIndex(qMax(0, combo->findData(m_codes.value(row))));
		connect(combo, &QComboBox::currentIndexChanged, this, [this, row, combo]() {
			if (!m_filling) setFolioSection(row, combo->currentData().toString());
		});
		m_folios_table->setCellWidget(row, 2, combo);
		m_folios_table->setItem(row, 3, readOnlyItem(QString()));
	}
	m_folios_table->resizeColumnToContents(0);
	m_folios_table->resizeColumnToContents(2);
	m_filling = false;
	updateNewNumbers();
}

/**
	@brief ProjectNumberingWizard::setFolioSection
	Put the folio of row in the section code, and the other selected
	folios with it when row is selected
*/
void ProjectNumberingWizard::setFolioSection(int row, const QString &code)
{
	QList<int> rows {row};
	const QModelIndexList selected = m_folios_table->selectionModel()->selectedRows();
	bool row_selected = false;
	for (const QModelIndex &index : selected)
		row_selected |= index.row() == row;
	if (row_selected)
		for (const QModelIndex &index : selected)
			if (index.row() != row) rows << index.row();

	m_filling = true;
	for (int r : std::as_const(rows)) {
		m_codes[r] = code;
		if (auto combo = qobject_cast<QComboBox *>(m_folios_table->cellWidget(r, 2)))
			combo->setCurrentIndex(qMax(0, combo->findData(code)));
	}
	m_filling = false;
	updateNewNumbers();
}

void ProjectNumberingWizard::readFormat()
{
	if (!m_enabled) return;
	m_settings.enabled = m_enabled->isChecked();
	const QString key = format_keys.value(m_format->currentIndex());
	m_settings.format = key == QLatin1String("seq") ? ProjectNumbering::Sequential
							: ProjectNumbering::SectionPage;
	if (m_settings.format == ProjectNumbering::SectionPage)
		m_settings.separator = key;
	m_settings.section_digits = m_section_digits->value();
	m_settings.page_digits = m_page_digits->value();
	m_settings.strip_titles = m_strip_titles->isChecked();
	m_section_digits->setEnabled(m_settings.format == ProjectNumbering::SectionPage);
}

void ProjectNumberingWizard::updateNewNumbers()
{
	const QStringList labels = m_settings.enabled
			? ProjectNumbering::folioLabels(m_settings, m_codes) : QStringList();
	for (int row = 0; row < m_folios_table->rowCount(); ++row)
		if (QTableWidgetItem *item = m_folios_table->item(row, 3))
			item->setText(m_settings.enabled ? labels.value(row) : tr("(como está)"));
}

QStringList ProjectNumberingWizard::orderedCodes() const
{
	QStringList codes;
	for (int index : order())
		codes << m_codes.at(index);
	return codes;
}

/**
	@brief ProjectNumberingWizard::fillPreview
	List the folios whose number, title or place changes
*/
void ProjectNumberingWizard::fillPreview()
{
	m_group->setVisible(m_settings.enabled && !ProjectNumbering::isGrouped(m_settings, m_codes));

	const QList<int> new_order = order();
	const QStringList new_labels = labels();
	const QStringList new_titles = titles();

	m_preview_table->setRowCount(0);
	int numbers = 0, titles_changed = 0, moved = 0;
	for (int position = 0; position < new_order.size(); ++position) {
		const int index = new_order.at(position);
		const ProjectNumbering::FolioInfo &folio = m_folios.at(index);
		const bool number_changes = new_labels.value(position) != folio.folio;
		const bool title_changes = new_titles.value(index) != folio.title;
		const bool moves = index != position;
		numbers += number_changes;
		titles_changed += title_changes;
		moved += moves;
		if (!number_changes && !title_changes && !moves) continue;

		const int row = m_preview_table->rowCount();
		m_preview_table->insertRow(row);
		m_preview_table->setItem(row, 0, readOnlyItem(moves ? tr("%1 (era %2)").arg(position + 1).arg(index + 1)
								    : QString::number(position + 1)));
		m_preview_table->setItem(row, 1, readOnlyItem(folio.folio));
		m_preview_table->setItem(row, 2, readOnlyItem(new_labels.value(position)));
		m_preview_table->setItem(row, 3, readOnlyItem(folio.title));
		m_preview_table->setItem(row, 4, readOnlyItem(new_titles.value(index)));
		QFont bold = m_preview_table->font();
		bold.setBold(true);
		if (number_changes) m_preview_table->item(row, 2)->setFont(bold);
		if (title_changes) m_preview_table->item(row, 4)->setFont(bold);
	}
	m_preview_table->resizeColumnsToContents();

	readTagSettings();
	const QList<ProjectNumbering::ComponentInfo> list = components();
	const QHash<QString, QString> changes = ProjectNumbering::componentLabels(m_settings, list, tagScope());
	QList<int> changed;
	for (int i = 0; i < list.size(); ++i)
		if (changes.contains(list.at(i).id)) changed << i;
	std::stable_sort(changed.begin(), changed.end(), [&](int a, int b) {
		if (list.at(a).folio != list.at(b).folio) return list.at(a).folio < list.at(b).folio;
		return changes.value(list.at(a).id) < changes.value(list.at(b).id);
	});
	m_components_table->setRowCount(int(changed.size()));
	for (int row = 0; row < changed.size(); ++row) {
		const ProjectNumbering::ComponentInfo &info = list.at(changed.at(row));
		m_components_table->setItem(row, 0, readOnlyItem(info.folio_label));
		m_components_table->setItem(row, 1, readOnlyItem(info.label));
		m_components_table->setItem(row, 2, readOnlyItem(changes.value(info.id)));
	}
	m_components_table->resizeColumnsToContents();
	m_preview_tabs->setTabText(1, tr("Componentes (%1)").arg(changed.size()));
	m_preview_tabs->setTabText(0, tr("Folhas (%1)").arg(m_preview_table->rowCount()));
	m_revision_warning->setVisible(m_has_revisions && tagScope() == ProjectNumbering::RenumberAll
				       && !changed.isEmpty());
	readWireSettings();
	const QList<ProjectNumbering::PotentialInfo> wire_list = potentials();
	const QHash<QString, QString> wire_changes = wireNumbers(wire_list);
	QList<int> wires_changed;
	for (int i = 0; i < wire_list.size(); ++i)
		if (wire_changes.contains(wire_list.at(i).id)) wires_changed << i;
	std::stable_sort(wires_changed.begin(), wires_changed.end(), [&](int a, int b) {
		const int fa = wire_list.at(a).folio, fb = wire_list.at(b).folio;
		if (fa != fb) return fa < fb;
		return wire_changes.value(wire_list.at(a).id) < wire_changes.value(wire_list.at(b).id);
	});
	m_wires_table->setRowCount(int(wires_changed.size()));
	for (int row = 0; row < wires_changed.size(); ++row) {
		const ProjectNumbering::PotentialInfo &info = wire_list.at(wires_changed.at(row));
		m_wires_table->setItem(row, 0, readOnlyItem(info.folio_label));
		m_wires_table->setItem(row, 1, readOnlyItem(info.number == wire_changes.value(info.id)
							     ? tr("%1, com fios sem número").arg(info.number)
							     : info.number));
		m_wires_table->setItem(row, 2, readOnlyItem(wire_changes.value(info.id)));
		m_wires_table->setItem(row, 3, readOnlyItem(QString::number(m_potentials.at(wires_changed.at(row)).wires.size())));
	}
	m_wires_table->resizeColumnsToContents();
	m_preview_tabs->setTabText(2, tr("Fios (%1)").arg(wires_changed.size()));

	QString tags_text = changed.isEmpty() ? QString()
				  : tr(" %1 tags de componentes mudam.").arg(changed.size());
	if (!wires_changed.isEmpty())
		tags_text += tr(" %1 potenciais de fios mudam de número.").arg(wires_changed.size());

	if (!m_settings.enabled)
		m_summary->setText(tr("A numeração automática das folhas está desligada: as seções ficam guardadas "
				      "no projeto e os números das folhas não mudam.") + tags_text);
	else if (!numbers && !titles_changed && !moved && changed.isEmpty() && wires_changed.isEmpty())
		m_summary->setText(tr("Nada muda: as %1 folhas e os componentes já estão numerados assim.")
				   .arg(new_order.size()));
	else
		m_summary->setText(tr("%1 folhas mudam de número, %2 títulos mudam e %3 folhas mudam de lugar, "
				      "de %4 folhas.").arg(numbers).arg(titles_changed).arg(moved).arg(new_order.size())
				   + tags_text);
}
