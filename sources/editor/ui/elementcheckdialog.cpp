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
#include "elementcheckdialog.h"

#include "../elementscene.h"

#include <QDialogButtonBox>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

ElementCheckDialog::ElementCheckDialog(ElementScene *scene, QWidget *parent) :
	QDialog(parent),
	m_scene(scene)
{
	setWindowTitle(tr("Verificar componente"));
	resize(640, 340);
	auto layout = new QVBoxLayout(this);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);

	m_list = new QTreeWidget(this);
	m_list->setColumnCount(2);
	m_list->setHeaderLabels({tr("Problema"), tr("Detalhe")});
	m_list->setRootIsDecorated(false);
	m_list->setWordWrap(true);
	m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_list->header()->setStretchLastSection(true);
	layout->addWidget(m_list, 1);

	auto buttons = new QHBoxLayout();
	m_fix = new QPushButton(tr("Corrigir"), this);
	m_fix_all = new QPushButton(tr("Corrigir tudo que der"), this);
	m_fix_all->setToolTip(tr("Aplica todas as correções automáticas; cada uma pode ser desfeita"));
	auto close = new QPushButton(tr("Fechar"), this);
	buttons->addWidget(m_fix);
	buttons->addWidget(m_fix_all);
	buttons->addStretch();
	buttons->addWidget(close);
	layout->addLayout(buttons);

	connect(m_list, &QTreeWidget::currentItemChanged, this, &ElementCheckDialog::showCurrent);
	connect(m_fix, &QPushButton::clicked, this, &ElementCheckDialog::fixCurrent);
	connect(m_fix_all, &QPushButton::clicked, this, &ElementCheckDialog::fixAll);
	connect(close, &QPushButton::clicked, this, &QDialog::close);
		//Follow the element while it is edited
	connect(&scene->undoStack(), &QUndoStack::indexChanged, this, &ElementCheckDialog::refresh);

	refresh();
}

/**
	@brief ElementCheckDialog::refresh
	Check the element again, keeping the issue chosen when it is still there
*/
void ElementCheckDialog::refresh()
{
	if (!m_scene)
		return;
	const QString current = m_list->currentItem() ? m_list->currentItem()->text(0) : QString();
	m_issues = ElementChecker::check(m_scene);

	const QSignalBlocker blocker(m_list);
	m_list->clear();
	int errors = 0, warnings = 0;
	for (const ElementChecker::Issue &issue : std::as_const(m_issues)) {
		auto item = new QTreeWidgetItem(m_list);
		QStyle::StandardPixmap icon = QStyle::SP_MessageBoxInformation;
		if (issue.severity == ElementChecker::Error) {
			icon = QStyle::SP_MessageBoxCritical;
			++errors;
		} else if (issue.severity == ElementChecker::Warning) {
			icon = QStyle::SP_MessageBoxWarning;
			++warnings;
		}
		item->setIcon(0, style()->standardIcon(icon));
		item->setText(0, issue.title);
		item->setText(1, issue.detail);
		item->setToolTip(1, issue.detail);
		if (issue.title == current)
			m_list->setCurrentItem(item);
	}

	if (m_issues.isEmpty())
		m_summary->setText(tr("Nenhum problema encontrado: o componente está pronto para os esquemas."));
	else
		m_summary->setText(tr("%1 erro(s), %2 aviso(s), %3 dica(s). Escolha um item para selecionar as partes.")
				   .arg(errors).arg(warnings).arg(m_issues.size() - errors - warnings));

	bool any_fix = false;
	for (const ElementChecker::Issue &issue : std::as_const(m_issues))
		any_fix = any_fix || issue.fix != ElementChecker::NoFix;
	m_fix_all->setEnabled(any_fix);
	const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
	const bool fixable = row >= 0 && m_issues.at(row).fix != ElementChecker::NoFix;
	m_fix->setEnabled(fixable);
	m_fix->setText(fixable ? ElementChecker::fixName(m_issues.at(row).fix) : tr("Corrigir"));
}

/**
	@brief ElementCheckDialog::showCurrent
	Select the parts of the issue chosen and show them
*/
void ElementCheckDialog::showCurrent()
{
	const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
	const bool fixable = row >= 0 && m_issues.at(row).fix != ElementChecker::NoFix;
	m_fix->setEnabled(fixable);
	m_fix->setText(fixable ? ElementChecker::fixName(m_issues.at(row).fix) : tr("Corrigir"));
	if (!m_scene || row < 0)
		return;

	const ElementChecker::Issue &issue = m_issues.at(row);
	m_scene->clearSelection();
	QRectF rect;
	for (QGraphicsItem *item : issue.items) {
		item->setSelected(true);
		rect = rect.united(item->sceneBoundingRect());
	}
	if (rect.isValid())
		for (QGraphicsView *view : m_scene->views())
			view->ensureVisible(rect);
}

void ElementCheckDialog::fixCurrent()
{
	const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
	if (!m_scene || row < 0)
		return;
	if (QUndoCommand *undo = ElementChecker::fix(m_scene, m_issues.at(row)))
		m_scene->undoStack().push(undo);
}

/**
	@brief ElementCheckDialog::fixAll
	Apply every fix, checking again after each one, in one undo
*/
void ElementCheckDialog::fixAll()
{
	if (!m_scene)
		return;
	m_scene->undoStack().beginMacro(tr("Corrigir o componente"));
	for (int pass = 0; pass < 8; ++pass) {
		const QList<ElementChecker::Issue> issues = ElementChecker::check(m_scene);
		auto next = std::find_if(issues.cbegin(), issues.cend(), [](const ElementChecker::Issue &issue) {
			return issue.fix != ElementChecker::NoFix;
		});
		if (next == issues.cend())
			break;
		QUndoCommand *undo = ElementChecker::fix(m_scene, *next);
		if (!undo)
			break;
		m_scene->undoStack().push(undo);
	}
	m_scene->undoStack().endMacro();
}
