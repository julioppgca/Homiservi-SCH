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
#include "projectindexdialog.h"

#include "../dataBase/projectdatabase.h"
#include "../dataBase/ui/summaryquerywidget.h"
#include "../diagram.h"
#include "../factory/qetgraphicstablefactory.h"
#include "../factory/ui/addtabledialog.h"
#include "../qetproject.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableView>
#include <QVBoxLayout>

namespace
{
	const QString settings_group = QStringLiteral("homiservi/index/");
}

ProjectIndexDialog::ProjectIndexDialog(Diagram *current, QWidget *parent) :
	QDialog(parent),
	m_current(current)
{
	setWindowTitle(tr("Índice do projeto"));
	resize(980, 640);
	QETProject *project = current ? current->project() : nullptr;
	const bool has_sections = ProjectIndex::hasSections(project);

	QSettings settings;
	for (ProjectIndex::Kind kind : {ProjectIndex::Folios, ProjectIndex::Sections}) {
		const QStringList saved = settings.value(settings_group + (kind == ProjectIndex::Folios
					? QStringLiteral("folio_columns") : QStringLiteral("section_columns"))).toStringList();
		m_chosen.insert(kind, saved.isEmpty() ? ProjectIndex::defaultColumns(kind) : saved);
	}

		//What
	auto type_box = new QGroupBox(tr("Tipo"), this);
	m_folios = new QRadioButton(tr("Índice de folhas – folha, título, localização..."), type_box);
	m_sections = new QRadioButton(tr("Índice de seções – código, nome e folhas de cada seção"), type_box);
	m_sections->setEnabled(has_sections);
	if (!has_sections)
		m_sections->setToolTip(tr("O projeto não tem seções: configure-as em Projeto > Numeração"));
	(has_sections && settings.value(settings_group + QStringLiteral("kind")).toInt() == ProjectIndex::Sections
	 ? m_sections : m_folios)->setChecked(true);
	auto type_layout = new QVBoxLayout(type_box);
	type_layout->addWidget(m_folios);
	type_layout->addWidget(m_sections);

		//Columns
	auto columns_box = new QGroupBox(tr("Colunas (marque e ordene)"), this);
	m_columns = new QListWidget(columns_box);
	auto up = new QPushButton(tr("Subir"), columns_box);
	auto down = new QPushButton(tr("Descer"), columns_box);
	auto column_buttons = new QVBoxLayout;
	column_buttons->addWidget(up);
	column_buttons->addWidget(down);
	column_buttons->addStretch();
	auto columns_layout = new QHBoxLayout(columns_box);
	columns_layout->addWidget(m_columns, 1);
	columns_layout->addLayout(column_buttons);

		//Options
	auto options_box = new QGroupBox(tr("Opções"), this);
	m_grouped = new QCheckBox(tr("Agrupar por seção (uma linha de título antes de cada seção)"), options_box);
	m_grouped->setChecked(has_sections && settings.value(settings_group + QStringLiteral("grouped"), true).toBool());
	m_grouped->setEnabled(has_sections);
	m_adjust = new QCheckBox(tr("Ajustar a tabela à folha"), options_box);
	m_adjust->setChecked(true);
	m_more_folios = new QCheckBox(tr("Continuar em folhas novas se não couber"), options_box);
	m_more_folios->setChecked(true);
	auto options_layout = new QVBoxLayout(options_box);
	options_layout->addWidget(m_grouped);
	options_layout->addWidget(m_adjust);
	options_layout->addWidget(m_more_folios);

		//Where
	auto where_box = new QGroupBox(tr("Onde"), this);
	m_new_folio = new QRadioButton(has_sections ? tr("Numa folha nova, no fim da primeira seção")
						    : tr("Numa folha nova, depois da folha atual"), where_box);
	m_current_folio = new QRadioButton(tr("Na folha atual (%1)")
					   .arg(current ? current->border_and_titleblock.finalfolio() : QString()), where_box);
	m_new_folio->setChecked(true);
	m_current_folio->setEnabled(current);
	m_name = new QLineEdit(where_box);
	auto where_layout = new QFormLayout(where_box);
	where_layout->addRow(m_new_folio);
	where_layout->addRow(m_current_folio);
	where_layout->addRow(tr("Nome da tabela:"), m_name);

	auto left = new QVBoxLayout;
	left->addWidget(type_box);
	left->addWidget(columns_box, 1);
	left->addWidget(options_box);
	left->addWidget(where_box);

		//Preview
	auto preview_box = new QGroupBox(tr("Prévia"), this);
	m_preview_label = new QLabel(preview_box);
	m_preview = new QTableView(preview_box);
	m_preview->verticalHeader()->hide();
	m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_preview_model = new QSqlQueryModel(this);
	m_preview->setModel(m_preview_model);
	auto preview_layout = new QVBoxLayout(preview_box);
	preview_layout->addWidget(m_preview_label);
	preview_layout->addWidget(m_preview, 1);

	auto body = new QHBoxLayout;
	body->addLayout(left, 2);
	body->addWidget(preview_box, 3);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Inserir"));
	auto advanced = buttons->addButton(tr("Avançado..."), QDialogButtonBox::ActionRole);
	advanced->setToolTip(tr("Abre a janela de tabela do QElectroTech com esta consulta: aparência"
				" (margens, fontes) ou uma consulta própria"));
	connect(buttons, &QDialogButtonBox::accepted, this, &ProjectIndexDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &ProjectIndexDialog::reject);
	connect(advanced, &QPushButton::clicked, this, &ProjectIndexDialog::advanced);

	auto layout = new QVBoxLayout(this);
	layout->addLayout(body, 1);
	layout->addWidget(buttons);

	connect(m_folios, &QRadioButton::toggled, this, [this]() { fillColumns(); });
	connect(m_columns, &QListWidget::itemChanged, this, [this]() {
		if (m_filling) return;
		m_chosen[kind()] = checkedColumns();
		updatePreview();
	});
	connect(m_grouped, &QCheckBox::toggled, this, &ProjectIndexDialog::updatePreview);
	connect(up, &QPushButton::clicked, this, [this]() { moveColumn(-1); });
	connect(down, &QPushButton::clicked, this, [this]() { moveColumn(1); });

	fillColumns();
}

ProjectIndex::Kind ProjectIndexDialog::kind() const
{
	return m_sections->isChecked() ? ProjectIndex::Sections : ProjectIndex::Folios;
}

/**
	@brief ProjectIndexDialog::fillColumns
	List the columns of the kind of index chosen: the checked ones first,
	in their order, then the others
*/
void ProjectIndexDialog::fillColumns()
{
	m_filling = true;
	m_columns->clear();
	const ProjectIndex::Kind k = kind();
	const QStringList chosen = m_chosen.value(k);
	QList<ProjectIndex::Column> ordered;
	for (const QString &key : chosen)
		for (const ProjectIndex::Column &column : ProjectIndex::columns(k))
			if (column.key == key) ordered << column;
	for (const ProjectIndex::Column &column : ProjectIndex::columns(k))
		if (!chosen.contains(column.key)) ordered << column;
	for (const ProjectIndex::Column &column : std::as_const(ordered)) {
		auto item = new QListWidgetItem(column.title, m_columns);
		item->setData(Qt::UserRole, column.key);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(chosen.contains(column.key) ? Qt::Checked : Qt::Unchecked);
		if (k == ProjectIndex::Folios && column.key == QLatin1String("section"))
			item->setToolTip(tr("Não aparece quando o índice é agrupado por seção"));
	}
	m_filling = false;
	m_grouped->setVisible(k == ProjectIndex::Folios);
	m_name->setText(ProjectIndex::defaultName(k));
	updatePreview();
}

QStringList ProjectIndexDialog::checkedColumns() const
{
	QStringList keys;
	for (int row = 0; row < m_columns->count(); ++row)
		if (m_columns->item(row)->checkState() == Qt::Checked)
			keys << m_columns->item(row)->data(Qt::UserRole).toString();
	return keys;
}

void ProjectIndexDialog::moveColumn(int delta)
{
	const int row = m_columns->currentRow();
	if (row < 0 || row + delta < 0 || row + delta >= m_columns->count()) return;
	m_filling = true;
	QListWidgetItem *item = m_columns->takeItem(row);
	m_columns->insertItem(row + delta, item);
	m_columns->setCurrentRow(row + delta);
	m_filling = false;
	m_chosen[kind()] = checkedColumns();
	updatePreview();
}

QString ProjectIndexDialog::currentQuery() const
{
	return ProjectIndex::query(kind(), m_chosen.value(kind()), m_grouped->isChecked());
}

/**
	@brief ProjectIndexDialog::updatePreview
	Run the query of the index on the database of the project
*/
void ProjectIndexDialog::updatePreview()
{
	if (!m_current || !m_current->project()) return;
	QSqlQuery query = m_current->project()->dataBase()->newQuery(currentQuery());
	query.exec();
	m_preview_model->setQuery(std::move(query));
	m_preview->resizeColumnsToContents();
	m_preview_label->setText(tr("%n linha(s); a tabela acompanha o projeto quando as folhas ou as seções mudam.",
				    nullptr, m_preview_model->rowCount()));
}

/**
	@return the folio the index goes on, a new one when asked
*/
Diagram *ProjectIndexDialog::targetFolio()
{
	if (m_current_folio->isChecked())
		return m_current;
	return ProjectIndex::addIndexFolio(m_current->project(), kind(), m_current);
}

void ProjectIndexDialog::saveChoices() const
{
	QSettings settings;
	settings.setValue(settings_group + QStringLiteral("kind"), int(kind()));
	settings.setValue(settings_group + QStringLiteral("folio_columns"), m_chosen.value(ProjectIndex::Folios));
	settings.setValue(settings_group + QStringLiteral("section_columns"), m_chosen.value(ProjectIndex::Sections));
	if (m_grouped->isEnabled())
		settings.setValue(settings_group + QStringLiteral("grouped"), m_grouped->isChecked());
}

void ProjectIndexDialog::accept()
{
	if (!m_current) return;
	saveChoices();
	Diagram *folio = targetFolio();
	ProjectIndex::insert(folio, currentQuery(), m_name->text().trimmed().isEmpty()
			     ? ProjectIndex::defaultName(kind()) : m_name->text().trimmed(),
			     m_adjust->isChecked(), m_more_folios->isChecked());
	QDialog::accept();
}

/**
	@brief ProjectIndexDialog::advanced
	The table dialog of QElectroTech on this query: the table it makes goes
	where this dialog says
*/
void ProjectIndexDialog::advanced()
{
	if (!m_current) return;
	auto content = new SummaryQueryWidget();
	content->setQuery(currentQuery());
	AddTableDialog dialog(content, this);
	dialog.setWindowTitle(tr("Índice do projeto – avançado"));
	dialog.setTableName(m_name->text().trimmed());
	dialog.setAdjustTableToFolio(m_adjust->isChecked());
	dialog.setAddNewTableToNewDiagram(m_more_folios->isChecked());
	if (dialog.exec() != QDialog::Accepted) return;
	saveChoices();
	QetGraphicsTableFactory::create(targetFolio(), &dialog);
	QDialog::accept();
}
