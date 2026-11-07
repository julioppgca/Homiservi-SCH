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
#include "projectdiffdock.h"

#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetmessagebox.h"
#include "../qetproject.h"

#include <QApplication>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
	const int change_role = Qt::UserRole + 1;

	QColor kindColor(ProjectDiff::Change::Kind kind)
	{
		switch (kind) {
			case ProjectDiff::Change::Added:   return QColor(0x2e, 0x7d, 0x32);
			case ProjectDiff::Change::Removed: return QColor(0xc6, 0x28, 0x28);
			default:                           return QColor(0xb2, 0x6a, 0x00);
		}
	}

	QString kindName(ProjectDiff::Change::Kind kind)
	{
		switch (kind) {
			case ProjectDiff::Change::Added:   return QObject::tr("Adicionado");
			case ProjectDiff::Change::Removed: return QObject::tr("Removido");
			default:                           return QObject::tr("Alterado");
		}
	}
}

/**
	@brief ProjectDiffDock::ProjectDiffDock
	@param parent
*/
ProjectDiffDock::ProjectDiffDock(QWidget *parent) :
	QDockWidget(tr("Diferenças"), parent)
{
	setObjectName(QStringLiteral("project_diff_dock"));
	setFeatures(QDockWidget::DockWidgetClosable
		    | QDockWidget::DockWidgetMovable
		    | QDockWidget::DockWidgetFloatable);

	auto widget = new QWidget(this);
	auto layout = new QVBoxLayout(widget);
	layout->setContentsMargins(4, 4, 4, 4);

	auto top = new QHBoxLayout();
	m_summary = new QLabel(widget);
	m_summary->setWordWrap(true);
	top->addWidget(m_summary, 1);
	auto refresh_button = new QToolButton(widget);
	refresh_button->setText(tr("Atualizar"));
	refresh_button->setToolTip(tr("Compara de novo, com as alterações feitas desde então"));
	top->addWidget(refresh_button);
	layout->addLayout(top);

	m_tree = new QTreeWidget(widget);
	m_tree->setHeaderLabels({tr("Alteração"), tr("Detalhes")});
	m_tree->setUniformRowHeights(true);
	m_tree->header()->setStretchLastSection(true);
	layout->addWidget(m_tree);
	setWidget(widget);

	connect(refresh_button, &QToolButton::clicked, this, &ProjectDiffDock::refresh);
	connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
		showChange(item);
	});
	connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
		showChange(item);
	});
	connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) {
		if (!visible) setHighlighted(nullptr);
	});
}

/**
	@brief ProjectDiffDock::compare
	Compare project, as it is now (unsaved changes included), with the
	project file at reference_path.
	@param project : the open project
	@param reference_path : another version of it
	@param reference_label : how to name that version, e.g. "versão de
	27/10/2026 14:02"
	@return false if the reference could not be opened
*/
bool ProjectDiffDock::compare(QETProject *project, const QString &reference_path,
			      const QString &reference_label)
{
	m_project = project;
	m_reference_path = reference_path;
	m_reference_label = reference_label;
	refresh();
	return !m_reference_path.isEmpty();
}

void ProjectDiffDock::refresh()
{
	setHighlighted(nullptr);
	m_changes.clear();
	if (!m_project) {
		m_summary->setText(tr("O projeto comparado foi fechado."));
		m_tree->clear();
		return;
	}

	QApplication::setOverrideCursor(Qt::WaitCursor);
	{
		QETProject reference(m_reference_path);
		if (reference.state() == QETProject::Ok)
			m_changes = ProjectDiff::compare(&reference, m_project);
		else
			m_reference_path.clear();
	}
	QApplication::restoreOverrideCursor();

	if (m_reference_path.isEmpty()) {
		m_summary->setText(tr("Não foi possível abrir a versão a comparar."));
		m_tree->clear();
		QET::QetMessageBox::critical(this, tr("Comparar versões"),
			tr("Não foi possível abrir %1.").arg(m_reference_label));
		return;
	}
	fillTree();
}

void ProjectDiffDock::fillTree()
{
	m_tree->clear();
	int added = 0, removed = 0, modified = 0;
	for (const ProjectDiff::Change &change : std::as_const(m_changes)) {
		if (change.kind == ProjectDiff::Change::Added) ++added;
		else if (change.kind == ProjectDiff::Change::Removed) ++removed;
		else ++modified;
	}
	m_summary->setText(m_changes.isEmpty()
		? tr("Nenhuma diferença em relação à %1.").arg(m_reference_label)
		: tr("Em relação à %1: %2 adicionado(s), %3 removido(s), %4 alterado(s).")
			  .arg(m_reference_label).arg(added).arg(removed).arg(modified));

	QHash<QString, QTreeWidgetItem *> folio_items;
	for (int i = 0 ; i < m_changes.size() ; ++i) {
		const ProjectDiff::Change &change = m_changes.at(i);
		const bool old_folio = !folio(change.folio_uuid);
		const QString folio_name = old_folio
			? tr("Fólio %1 - %2 (versão comparada)").arg(change.folio_index + 1).arg(change.folio_title)
			: tr("Fólio %1 - %2").arg(change.folio_index + 1).arg(change.folio_title);
		QTreeWidgetItem *parent = folio_items.value(folio_name);
		if (!parent) {
			parent = new QTreeWidgetItem(m_tree, {folio_name});
			QFont font = parent->font(0);
			font.setBold(true);
			parent->setFont(0, font);
			parent->setFirstColumnSpanned(true);
			parent->setExpanded(true);
			folio_items.insert(folio_name, parent);
		}

		QString text = QStringLiteral("%1: %2").arg(kindName(change.kind), change.name);
		if (!change.cell.isEmpty() && change.item != ProjectDiff::Change::Folio)
			text += QStringLiteral("  [%1]").arg(change.cell);
		auto item = new QTreeWidgetItem(parent, {text, change.details.join(QStringLiteral("; "))});
		item->setForeground(0, kindColor(change.kind));
		item->setToolTip(0, change.name);
		item->setToolTip(1, change.details.join(QLatin1Char('\n')));
		item->setData(0, change_role, i);
	}
	m_tree->resizeColumnToContents(0);
}

/**
	@brief ProjectDiffDock::showChange
	Show the folio of the change and the item in the open project, or the
	place it was if it was removed.
*/
void ProjectDiffDock::showChange(QTreeWidgetItem *item)
{
	if (!item || !m_project) return;
	const QVariant index = item->data(0, change_role);
	if (!index.isValid()) return;
	const ProjectDiff::Change &change = m_changes.at(index.toInt());

	setHighlighted(nullptr);
	Diagram *diagram = folio(change.folio_uuid);
	if (!diagram) return;	// removed folio: nothing to show
	diagram->showMe();

	QPointF center = change.pos;
	if (change.kind != ProjectDiff::Change::Removed) {
		if (change.item == ProjectDiff::Change::Element) {
			for (Element *element : diagram->elements()) {
				if (element->uuid() == change.element_uuid) {
					setHighlighted(element);
					center = element->sceneBoundingRect().center();
					break;
				}
			}
		} else if (change.item == ProjectDiff::Change::Conductor) {
			for (Conductor *conductor : diagram->conductors()) {
				if (ProjectDiff::conductorKey(conductor) == change.conductor_key) {
					diagram->clearSelection();
					conductor->setSelected(true);
					center = conductor->sceneBoundingRect().center();
					break;
				}
			}
		}
	}
	if (change.item != ProjectDiff::Change::Folio) {
		for (QGraphicsView *view : diagram->views())
			view->centerOn(center);
	}
}

Diagram *ProjectDiffDock::folio(const QUuid &uuid) const
{
	if (!m_project) return nullptr;
	for (Diagram *diagram : m_project->diagrams()) {
		if (diagram->uuid() == uuid) return diagram;
	}
	return nullptr;
}

void ProjectDiffDock::setHighlighted(Element *element)
{
	if (m_highlighted) m_highlighted->setHighlighted(false);
	m_highlighted = element;
	if (m_highlighted) m_highlighted->setHighlighted(true);
}
