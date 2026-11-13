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
#include "projectrevisionsdialog.h"

#include "../projectnumbering.h"

#include "../diagram.h"
#include "../qetproject.h"

#include <QCheckBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>

namespace {
	enum Column { Edition, Description, ModifiedBy, ApprovedBy, Date, Folios, ColumnCount };
		///Data of the edition cell: the folios, the content kept, a new revision
	enum Role { FoliosRole = Qt::UserRole, SnapshotRole, NewRole };
}

ProjectRevisionsDialog::ProjectRevisionsDialog(QETProject *project, Diagram *current, QWidget *parent) :
	QDialog(parent),
	m_project(project),
	m_current(current),
	m_hashes(ProjectRevisions::folioHashes(project))
{
	setWindowTitle(tr("Revisões do projeto"));

	auto info = new QLabel(tr("A revisão mais recente fica no alto. Cada folha mostra no quadro de revisões"
				  " da legenda as revisões que a alteraram (as %1 mais recentes), e a edição da"
				  " mais recente vai para o índice de revisão dela.")
			       .arg(ProjectRevisions::TableRows), this);
	info->setWordWrap(true);

	m_table = new QTableWidget(0, ColumnCount, this);
	m_table->setHorizontalHeaderLabels({tr("Ed."), tr("Modificação"), tr("Modificado por"),
					    tr("Aprovado por"), tr("Data"), tr("Folhas")});
	m_table->horizontalHeader()->setSectionResizeMode(Description, QHeaderView::Stretch);
	m_table->verticalHeader()->hide();
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setColumnWidth(Edition, 45);
	m_table->setColumnWidth(ModifiedBy, 110);
	m_table->setColumnWidth(ApprovedBy, 110);
	m_table->setColumnWidth(Date, 80);
	m_table->setColumnWidth(Folios, 160);
	for (const auto &revision : project->revisions())
		insertRow(m_table->rowCount(), revision);

		//The folios of the revision selected
	auto folios_box = new QWidget(this);
	m_folios_label = new QLabel(folios_box);
	m_general = new QCheckBox(tr("Revisão geral (todas as folhas)"), folios_box);
	m_folios = new QListWidget(folios_box);
	for (Diagram *diagram : project->diagrams()) {
			//Without the number the title may start with: "02.04 – ..."
		auto item = new QListWidgetItem(QStringLiteral("%1 – %2").arg(diagram->border_and_titleblock.finalfolio(),
									     ProjectNumbering::strippedTitle(diagram->title())), m_folios);
		item->setData(Qt::UserRole, diagram->uuid().toString());
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Unchecked);
	}
	m_check_changed = new QPushButton(tr("Marcar as alteradas"), folios_box);
	m_check_changed->setToolTip(tr("As folhas cujo conteúdo mudou desde a revisão anterior, e as folhas novas"));
	m_check_current = new QPushButton(tr("Marcar a folha atual"), folios_box);
	m_check_current->setEnabled(current);
	auto folio_buttons = new QHBoxLayout;
	folio_buttons->addWidget(m_check_changed);
	folio_buttons->addWidget(m_check_current);
	folio_buttons->addStretch();
	auto folios_layout = new QVBoxLayout(folios_box);
	folios_layout->setContentsMargins(0, 0, 0, 0);
	folios_layout->addWidget(m_folios_label);
	folios_layout->addWidget(m_general);
	folios_layout->addWidget(m_folios, 1);
	folios_layout->addLayout(folio_buttons);

	auto splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(m_table);
	splitter->addWidget(folios_box);
	splitter->setStretchFactor(0, 3);
	splitter->setStretchFactor(1, 2);

	auto add = new QPushButton(tr("Nova revisão"), this);
	auto remove = new QPushButton(tr("Remover"), this);
	connect(add, &QPushButton::clicked, this, &ProjectRevisionsDialog::addRevision);
	connect(remove, &QPushButton::clicked, this, &ProjectRevisionsDialog::removeRevision);
	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row) { showFolios(row); });
	connect(m_general, &QCheckBox::toggled, this, &ProjectRevisionsDialog::foliosChanged);
	connect(m_folios, &QListWidget::itemChanged, this, &ProjectRevisionsDialog::foliosChanged);
	connect(m_check_changed, &QPushButton::clicked, this, [this]() { checkFolios(changedFolios()); });
	connect(m_check_current, &QPushButton::clicked, this, [this]() {
		if (m_current) checkFolios({m_current->uuid().toString()});
	});

	auto row_buttons = new QHBoxLayout();
	row_buttons->addWidget(add);
	row_buttons->addWidget(remove);
	row_buttons->addStretch();
	row_buttons->addWidget(buttons);

	auto layout = new QVBoxLayout(this);
	layout->addWidget(info);
	layout->addWidget(splitter, 1);
	layout->addLayout(row_buttons);
	resize(1100, 520);

	if (m_table->rowCount())
		m_table->setCurrentCell(0, Edition);
	else
		showFolios(-1);
}

/**
	@return the revision of the row of the table
*/
ProjectRevisions::Revision ProjectRevisionsDialog::rowRevision(int row) const
{
	auto text = [this, row](int column) {
		const QTableWidgetItem *item = m_table->item(row, column);
		return item ? item->text().trimmed() : QString();
	};
	ProjectRevisions::Revision revision;
	revision.edition = text(Edition);
	revision.description = text(Description);
	revision.modified_by = text(ModifiedBy);
	revision.approved_by = text(ApprovedBy);
	revision.date = text(Date);
	if (const QTableWidgetItem *item = m_table->item(row, Edition)) {
		revision.folios = item->data(FoliosRole).toStringList();
		const QVariantHash snapshot = item->data(SnapshotRole).toHash();
		for (auto it = snapshot.constBegin(); it != snapshot.constEnd(); ++it)
			revision.snapshot.insert(it.key(), it.value().toString());
			//A new revision keeps the content of the folios as they are now
		if (item->data(NewRole).toBool())
			revision.snapshot = m_hashes;
	}
	return revision;
}

/**
	@return the revisions of the table, the newest first, without the
	empty rows
*/
QList<ProjectRevisions::Revision> ProjectRevisionsDialog::revisions() const
{
	QList<ProjectRevisions::Revision> revisions;
	for (int row = 0; row < m_table->rowCount(); ++row) {
		const ProjectRevisions::Revision revision = rowRevision(row);
		if (!revision.isEmpty())
			revisions << revision;
	}
	return revisions;
}

/**
	@return the folios changed since the newest revision before the new
	ones of the table
*/
QStringList ProjectRevisionsDialog::changedFolios() const
{
	QList<ProjectRevisions::Revision> before;
	for (int row = 0; row < m_table->rowCount(); ++row) {
		const QTableWidgetItem *item = m_table->item(row, Edition);
		if (item && !item->data(NewRole).toBool())
			before << rowRevision(row);
	}
	return ProjectRevisions::changedFolios(before, m_hashes);
}

/**
	@brief ProjectRevisionsDialog::addRevision
	Add the next edition at the top, with the folios changed since the
	revision before and the current folio, and start writing its
	modification; the first revision changes every folio
*/
void ProjectRevisionsDialog::addRevision()
{
	const QTableWidgetItem *newest = m_table->rowCount() ? m_table->item(0, Edition) : nullptr;
	ProjectRevisions::Revision revision;
	revision.edition = ProjectRevisions::nextEdition(newest ? newest->text() : QString());
	revision.modified_by = QSettings().value(QStringLiteral("homiservi/author")).toString();
	revision.date = QLocale().toString(QDate::currentDate(), QStringLiteral("dd/MM/yy"));
	if (m_table->rowCount()) {
		revision.folios = changedFolios();
		if (m_current && !revision.folios.contains(m_current->uuid().toString()))
			revision.folios << m_current->uuid().toString();
	}
	insertRow(0, revision, true);
	m_table->setCurrentCell(0, Description);
	m_table->editItem(m_table->item(0, Description));
}

void ProjectRevisionsDialog::removeRevision()
{
	const int row = m_table->currentRow();
	if (row >= 0)
		m_table->removeRow(row);
}

void ProjectRevisionsDialog::insertRow(int row, const ProjectRevisions::Revision &revision, bool is_new)
{
	m_table->insertRow(row);
	const QStringList values {revision.edition, revision.description, revision.modified_by,
				  revision.approved_by, revision.date, QString()};
	for (int column = 0; column < ColumnCount; ++column)
		m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
	QTableWidgetItem *edition = m_table->item(row, Edition);
	edition->setData(FoliosRole, revision.folios);
	QVariantHash snapshot;
	for (auto it = revision.snapshot.constBegin(); it != revision.snapshot.constEnd(); ++it)
		snapshot.insert(it.key(), it.value());
	edition->setData(SnapshotRole, snapshot);
	edition->setData(NewRole, is_new);
	m_table->item(row, Folios)->setFlags(m_table->item(row, Folios)->flags() & ~Qt::ItemIsEditable);
	updateFoliosText(row);
}

/**
	@return the folios of revision as they read: "Todas", or their numbers
*/
QString ProjectRevisionsDialog::foliosText(const ProjectRevisions::Revision &revision) const
{
	if (revision.isGeneral())
		return tr("Todas");
	QStringList labels;
	for (Diagram *diagram : m_project->diagrams())
		if (revision.folios.contains(diagram->uuid().toString()))
			labels << diagram->border_and_titleblock.finalfolio();
	if (labels.isEmpty())
		return revision.folios == QStringList {QUuid().toString()} ? tr("(nenhuma folha marcada)")
									   : tr("(folhas removidas)");
	return labels.join(QStringLiteral(", "));
}

void ProjectRevisionsDialog::updateFoliosText(int row)
{
	if (QTableWidgetItem *item = m_table->item(row, Folios))
		item->setText(foliosText(rowRevision(row)));
}

/**
	@brief ProjectRevisionsDialog::showFolios
	Show the folios of the revision of row, none when there is no row
*/
void ProjectRevisionsDialog::showFolios(int row)
{
	m_filling = true;
	const bool valid = row >= 0 && row < m_table->rowCount();
	const ProjectRevisions::Revision revision = valid ? rowRevision(row) : ProjectRevisions::Revision();
	m_folios_label->setText(valid ? tr("Folhas alteradas pela revisão %1:").arg(revision.edition)
				      : tr("Crie uma revisão com \"Nova revisão\"."));
	m_general->setEnabled(valid);
	m_general->setChecked(valid && revision.isGeneral());
	for (int i = 0; i < m_folios->count(); ++i) {
		QListWidgetItem *item = m_folios->item(i);
		item->setCheckState(valid && !revision.isGeneral()
				    && revision.folios.contains(item->data(Qt::UserRole).toString())
				    ? Qt::Checked : Qt::Unchecked);
	}
	m_folios->setEnabled(valid && !revision.isGeneral());
	m_check_changed->setEnabled(valid);
	m_check_current->setEnabled(valid && m_current);
	m_filling = false;
}

/**
	@brief ProjectRevisionsDialog::foliosChanged
	Keep the folios checked as the ones of the revision selected
*/
void ProjectRevisionsDialog::foliosChanged()
{
	if (m_filling) return;
	const int row = m_table->currentRow();
	QTableWidgetItem *edition = m_table->item(row, Edition);
	if (!edition) return;
	QStringList folios;
	if (!m_general->isChecked())
		for (int i = 0; i < m_folios->count(); ++i)
			if (m_folios->item(i)->checkState() == Qt::Checked)
				folios << m_folios->item(i)->data(Qt::UserRole).toString();
		//Not general and no folio checked yet: a folio that is none, so
		//that the revision does not become one of every folio
	if (!m_general->isChecked() && folios.isEmpty())
		folios << QUuid().toString();
	edition->setData(FoliosRole, folios);
	m_folios->setEnabled(!m_general->isChecked());
	updateFoliosText(row);
}

/**
	@brief ProjectRevisionsDialog::checkFolios
	Check the folios uuids too, for the revision selected
*/
void ProjectRevisionsDialog::checkFolios(const QStringList &uuids)
{
	if (m_general->isChecked()) {
		m_filling = true;
		m_general->setChecked(false);
		m_filling = false;
	}
	m_filling = true;
	for (int i = 0; i < m_folios->count(); ++i)
		if (uuids.contains(m_folios->item(i)->data(Qt::UserRole).toString()))
			m_folios->item(i)->setCheckState(Qt::Checked);
	m_filling = false;
	foliosChanged();
}
