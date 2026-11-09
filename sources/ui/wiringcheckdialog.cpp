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
#include "wiringcheckdialog.h"

#include "../diagram.h"
#include "../qeticons.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetproject.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

WiringCheckDialog::WiringCheckDialog(QETProject *project, QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Verificar fiação"));
	resize(820, 480);
	auto layout = new QVBoxLayout(this);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);

	auto filters = new QHBoxLayout();
	const char *names[] = {QT_TR_NOOP("Erros"), QT_TR_NOOP("Avisos"), QT_TR_NOOP("Informações")};
	for (int i = 0; i < 3; ++i) {
		m_show[i] = new QCheckBox(tr(names[i]), this);
		m_show[i]->setChecked(true);
		connect(m_show[i], &QCheckBox::toggled, this, &WiringCheckDialog::fill);
		filters->addWidget(m_show[i]);
	}
	filters->addStretch();
	layout->addLayout(filters);

	m_table = new QTableWidget(0, 4, this);
	m_table->setHorizontalHeaderLabels({tr("Tipo"), tr("Folha"), tr("Fio"), tr("Problema")});
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->verticalHeader()->hide();
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->setAlternatingRowColors(true);
	layout->addWidget(m_table, 1);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	auto again = buttons->addButton(tr("Verificar de novo"), QDialogButtonBox::ActionRole);
	layout->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
	connect(again, &QPushButton::clicked, this, &WiringCheckDialog::refresh);
	connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { showIssue(row); });

	setProject(project);
}

void WiringCheckDialog::setProject(QETProject *project)
{
	m_project = project;
	refresh();
}

/**
	@brief WiringCheckDialog::refresh
	Check the wires of the project again
*/
void WiringCheckDialog::refresh()
{
	m_issues = WiringCheck::check(m_project);
	int count[3] = {0, 0, 0};
	for (const WiringCheck::Issue &issue : std::as_const(m_issues))
		++count[issue.severity];
	if (!m_project)
		m_summary->setText(tr("Nenhum projeto aberto."));
	else if (m_issues.isEmpty())
		m_summary->setText(tr("<b>Nenhum problema encontrado</b> na fiação de \"%1\".").arg(m_project->title()));
	else
		m_summary->setText(tr("%1 erro(s), %2 aviso(s), %3 informação(ões). Clique duas vezes numa"
				      " linha para ver o fio na folha.").arg(count[0]).arg(count[1]).arg(count[2]));
	fill();
}

void WiringCheckDialog::fill()
{
	m_table->setRowCount(0);
	const QIcon icons[] = {style()->standardIcon(QStyle::SP_MessageBoxCritical),
			       style()->standardIcon(QStyle::SP_MessageBoxWarning),
			       style()->standardIcon(QStyle::SP_MessageBoxInformation)};
	for (int i = 0; i < m_issues.size(); ++i) {
		const WiringCheck::Issue &issue = m_issues.at(i);
		if (!m_show[issue.severity]->isChecked()) continue;
		const int row = m_table->rowCount();
		m_table->insertRow(row);
		auto type = new QTableWidgetItem(icons[issue.severity], WiringCheck::severityName(issue.severity));
		type->setData(Qt::UserRole, i);
		m_table->setItem(row, 0, type);
		m_table->setItem(row, 1, new QTableWidgetItem(issue.folio));
		m_table->setItem(row, 2, new QTableWidgetItem(issue.wire));
		auto text = new QTableWidgetItem(issue.text);
		text->setToolTip(issue.text);
		m_table->setItem(row, 3, text);
	}
	m_table->resizeColumnsToContents();
}

/**
	@brief WiringCheckDialog::showIssue
	Show the wire of the issue of row on its folio, selected
*/
void WiringCheckDialog::showIssue(int row)
{
	const QTableWidgetItem *item = m_table->item(row, 0);
	if (!item) return;
	const int index = item->data(Qt::UserRole).toInt();
	if (index < 0 || index >= m_issues.size()) return;
	Conductor *conductor = m_issues.at(index).conductor;
	if (!conductor || !conductor->diagram()) return;
	Diagram *diagram = conductor->diagram();
	diagram->showMe();
	diagram->clearSelection();
	conductor->setSelected(true);
	for (QGraphicsView *view : diagram->views())
		view->centerOn(conductor->sceneBoundingRect().center());
}
