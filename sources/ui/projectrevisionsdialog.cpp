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

#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
	enum Column { Edition, Description, ModifiedBy, ApprovedBy, Date, ColumnCount };
}

ProjectRevisionsDialog::ProjectRevisionsDialog(const QList<ProjectRevisions::Revision> &revisions,
					       QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Revisões do projeto"));

	auto info = new QLabel(tr("A revisão mais recente fica no alto. As %1 primeiras aparecem no"
				  " quadro de revisões da legenda; a edição da mais recente vai para"
				  " o índice de revisão de todas as folhas.")
			       .arg(ProjectRevisions::TableRows), this);
	info->setWordWrap(true);

	m_table = new QTableWidget(0, ColumnCount, this);
	m_table->setHorizontalHeaderLabels({tr("Ed."), tr("Modificação"), tr("Modificado por"),
					    tr("Aprovado por"), tr("Data")});
	m_table->horizontalHeader()->setSectionResizeMode(Description, QHeaderView::Stretch);
	m_table->verticalHeader()->hide();
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setColumnWidth(Edition, 45);
	m_table->setColumnWidth(ModifiedBy, 110);
	m_table->setColumnWidth(ApprovedBy, 110);
	m_table->setColumnWidth(Date, 80);
	for (const auto &revision : revisions)
		insertRow(m_table->rowCount(), revision);

	auto add = new QPushButton(tr("Nova revisão"), this);
	auto remove = new QPushButton(tr("Remover"), this);
	connect(add, &QPushButton::clicked, this, &ProjectRevisionsDialog::addRevision);
	connect(remove, &QPushButton::clicked, this, &ProjectRevisionsDialog::removeRevision);
	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto row_buttons = new QHBoxLayout();
	row_buttons->addWidget(add);
	row_buttons->addWidget(remove);
	row_buttons->addStretch();
	row_buttons->addWidget(buttons);

	auto layout = new QVBoxLayout(this);
	layout->addWidget(info);
	layout->addWidget(m_table, 1);
	layout->addLayout(row_buttons);
	resize(760, 400);
}

/**
	@return the revisions of the table, the newest first, without the
	empty rows
*/
QList<ProjectRevisions::Revision> ProjectRevisionsDialog::revisions() const
{
	QList<ProjectRevisions::Revision> revisions;
	for (int row = 0; row < m_table->rowCount(); ++row)
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
		if (!revision.isEmpty())
			revisions << revision;
	}
	return revisions;
}

	///Add the next edition at the top and start writing its modification
void ProjectRevisionsDialog::addRevision()
{
	const QTableWidgetItem *newest = m_table->rowCount() ? m_table->item(0, Edition) : nullptr;
	ProjectRevisions::Revision revision;
	revision.edition = ProjectRevisions::nextEdition(newest ? newest->text() : QString());
	revision.modified_by = QSettings().value(QStringLiteral("homiservi/author")).toString();
	revision.date = QLocale().toString(QDate::currentDate(), QStringLiteral("dd/MM/yy"));
	insertRow(0, revision);
	m_table->setCurrentCell(0, Description);
	m_table->editItem(m_table->item(0, Description));
}

void ProjectRevisionsDialog::removeRevision()
{
	const int row = m_table->currentRow();
	if (row >= 0)
		m_table->removeRow(row);
}

void ProjectRevisionsDialog::insertRow(int row, const ProjectRevisions::Revision &revision)
{
	m_table->insertRow(row);
	const QStringList values {revision.edition, revision.description, revision.modified_by,
				  revision.approved_by, revision.date};
	for (int column = 0; column < ColumnCount; ++column)
		m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
}
