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
#include "wiringeditordock.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../conductorproperties.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetmessagebox.h"
#include "../potentialrules.h"
#include "../qetproject.h"
#include "../conductorendlabeling.h"
#include "../wiringlist.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QTimer>
#include <QCollator>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSaveFile>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextStream>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

#include <algorithm>

namespace {
	QString terminalName(Terminal *terminal)
	{
		if (!terminal) return QString();
		Element *element = terminal->parentElement();
		const QString label = element ? element->actualLabel() : QString();
		const QString name = terminal->name();
		if (name.isEmpty() || name == QLatin1String("_"))
			return label;
		return label.isEmpty() ? name : QStringLiteral("%1:%2").arg(label, name);
	}

	QString field(const ConductorProperties &p, int column)
	{
		switch (column) {
			case WiringTableModel::Number:    return p.text;
			case WiringTableModel::Function:  return p.m_function;
			case WiringTableModel::WireColor: return p.m_wire_color;
			case WiringTableModel::Section:   return p.m_wire_section;
			case WiringTableModel::Cable:     return p.m_cable;
			case WiringTableModel::Voltage:   return p.m_tension_protocol;
			case WiringTableModel::Bus:       return p.m_bus;
			default:                          return QString();
		}
	}

	void setField(ConductorProperties &p, int column, const QString &value)
	{
		switch (column) {
				//A typed number replaces the numbering formula, which
				//would otherwise give the old number back
			case WiringTableModel::Number:    p.text = value; p.m_formula = value; break;
			case WiringTableModel::Function:  p.m_function = value; break;
			case WiringTableModel::WireColor: PotentialRules::setWireColor(p, value); break;
			case WiringTableModel::Section:   p.m_wire_section = value; break;
			case WiringTableModel::Cable:     p.m_cable = value; break;
			case WiringTableModel::Voltage:   p.m_tension_protocol = value; break;
			case WiringTableModel::Bus:       p.m_bus = value; break;
			default: break;
		}
	}

	/**
		Edits a cell for every selected row: the value typed in one cell
		goes to all the selected rows, as in a spreadsheet.
	*/
	class WiringDelegate : public QStyledItemDelegate
	{
		public:
			WiringDelegate(QTableView *view, QSortFilterProxyModel *proxy,
				       WiringTableModel *model) :
				QStyledItemDelegate(view), m_view(view), m_proxy(proxy), m_model(model)
			{}

			/**
				The function column chooses among the function rules of
				the project (and the functions its wires already have);
				a function can still be typed.
			*/
			QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
					      const QModelIndex &index) const override
			{
				if (index.column() != WiringTableModel::Function || !m_model->project())
					return QStyledItemDelegate::createEditor(parent, option, index);
				auto combo = new QComboBox(parent);
				combo->setEditable(true);
				combo->setInsertPolicy(QComboBox::NoInsert);
				combo->addItem(tr("(sem função)"), QString());
				QStringList known;
				for (const PotentialRules::Rule &rule : m_model->project()->potentialRules().rules) {
					if (rule.field != PotentialRules::Function || rule.pattern.isEmpty()
					    || rule.pattern.contains(QLatin1Char('*'))
					    || rule.pattern.contains(QLatin1Char('?'))
					    || known.contains(rule.pattern, Qt::CaseInsensitive))
						continue;
					known << rule.pattern;
					QStringList details;
					if (!rule.color.isEmpty())   details << PotentialRules::colorName(rule.color);
					if (!rule.section.isEmpty()) details << PotentialRules::sectionText(rule.section);
					combo->addItem(details.isEmpty() ? rule.pattern
						       : QStringLiteral("%1  (%2)").arg(rule.pattern, details.join(QStringLiteral(", "))),
						       rule.pattern);
					const QList<QColor> colors = PotentialRules::drawingColors(rule.color);
					if (!colors.isEmpty()) {
						QPixmap swatch(12, 12);
						swatch.fill(colors.first());
						if (colors.size() > 1) {
							QPainter painter(&swatch);
							painter.fillRect(0, 6, 12, 6, colors.at(1));
						}
						combo->setItemIcon(combo->count() - 1, QIcon(swatch));
					}
				}
				const int rules_end = combo->count();
				QStringList used;
				for (int row = 0 ; row < m_model->rowCount() ; ++row)
					if (Conductor *c = m_model->conductor(row)) {
						const QString function = c->properties().m_function.trimmed();
						if (!function.isEmpty() && !known.contains(function, Qt::CaseInsensitive)
						    && !used.contains(function, Qt::CaseInsensitive))
							used << function;
					}
				used.sort(Qt::CaseInsensitive);
				if (!used.isEmpty()) {
					combo->insertSeparator(rules_end);
					for (const QString &function : std::as_const(used))
						combo->addItem(function, function);
				}
					//The cell is narrow: the list is as wide as its items
				combo->view()->setMinimumWidth(combo->view()->sizeHintForColumn(0)
							       + combo->iconSize().width() + 24);
				combo->setToolTip(tr("As funções das regras de fiação, com a cor e a seção"
						     " que dão ao fio; abaixo, as outras funções já usadas"
						     " no projeto. Também se pode digitar."));
					//Choosing an item is the edit: commit it at once
				connect(combo, QOverload<int>::of(&QComboBox::activated), combo, [this, combo]() {
					auto self = const_cast<WiringDelegate *>(this);
					emit self->commitData(combo);
					emit self->closeEditor(combo);
				});
				return combo;
			}

			void setEditorData(QWidget *editor, const QModelIndex &index) const override
			{
				auto combo = qobject_cast<QComboBox *>(editor);
				if (!combo) {
					QStyledItemDelegate::setEditorData(editor, index);
					return;
				}
				const QString value = index.data(Qt::EditRole).toString();
				const int found = combo->findData(value, Qt::UserRole, Qt::MatchFixedString);
				if (found >= 0) combo->setCurrentIndex(found);
				else combo->setEditText(value);
				QTimer::singleShot(0, combo, &QComboBox::showPopup);
			}

			void setModelData(QWidget *editor, QAbstractItemModel *,
					  const QModelIndex &index) const override
			{
				QString value;
				if (auto combo = qobject_cast<QComboBox *>(editor)) {
						//The text of a chosen item shows its colour too:
						//write the function only
					const int current = combo->currentIndex();
					value = current >= 0 && combo->currentText() == combo->itemText(current)
							? combo->itemData(current).toString()
							: combo->currentText();
				} else if (auto line = qobject_cast<QLineEdit *>(editor)) {
					value = line->text();
				} else {
					return;
				}
				QList<int> rows;
				const QModelIndexList selected = m_view->selectionModel()->selectedRows();
				const QModelIndex source = m_proxy->mapToSource(index);
				bool edited_row_selected = false;
				for (const QModelIndex &row : selected) {
					const int source_row = m_proxy->mapToSource(row).row();
					rows << source_row;
					if (source_row == source.row()) edited_row_selected = true;
				}
				if (!edited_row_selected) rows = {source.row()};
				m_model->setValues(rows, source.column(), value.trimmed(),
						   m_model->m_whole_potential);
			}

		private:
			QTableView *m_view;
			QSortFilterProxyModel *m_proxy;
			WiringTableModel *m_model;
	};
}

WiringTableModel::WiringTableModel(QObject *parent) :
	QAbstractTableModel(parent)
{}

void WiringTableModel::setProject(QETProject *project)
{
	m_project = project;
	reload();
}

/**
	@brief WiringTableModel::reload
	Read the conductors of the project again, by folio then number.
*/
void WiringTableModel::reload()
{
	beginResetModel();
	m_conductors.clear();
	if (m_project) {
		QCollator collator;
		collator.setNumericMode(true);
		for (Diagram *diagram : m_project->diagrams()) {
			QList<Conductor *> conductors = diagram->conductors();
			std::sort(conductors.begin(), conductors.end(),
				  [&collator](Conductor *a, Conductor *b) {
				return collator.compare(a->properties().text, b->properties().text) < 0;
			});
			for (Conductor *conductor : std::as_const(conductors))
				m_conductors << conductor;
		}
	}
	endResetModel();
}

Conductor *WiringTableModel::conductor(int row) const
{
	return row >= 0 && row < m_conductors.size() ? m_conductors.at(row).data() : nullptr;
}

bool WiringTableModel::isEditable(int column)
{
	return column >= Number && column != From && column != To
			&& column != FromLabel && column != ToLabel;
}

/**
	@brief WiringTableModel::setValues
	Write value in column to the conductors of rows, and to the other
	conductors of their potentials if whole_potential, as one undo step.
	Only that property changes: the others stay as each conductor has them.
	@return true if something changed
*/
bool WiringTableModel::setValues(const QList<int> &rows, int column,
				 const QString &value, bool whole_potential)
{
	if (!m_project || !isEditable(column)) return false;

	QList<Conductor *> targets;
	for (int row : rows) {
		Conductor *c = conductor(row);
		if (!c) continue;
		if (!targets.contains(c)) targets << c;
		if (whole_potential) {
			for (Conductor *other : c->relatedPotentialConductors())
				if (!targets.contains(other)) targets << other;
		}
	}

	auto undo = new QUndoCommand(tr("Editar a fiação: %1 = %2")
				     .arg(headerData(column, Qt::Horizontal).toString(), value));
	int changed = 0;
	for (Conductor *c : std::as_const(targets)) {
		ConductorProperties properties = c->properties();
		const ConductorProperties before = properties;
		setField(properties, column, value);
		PotentialRules::applyOnEdit(m_project->potentialRules(), before, properties);
		if (properties == before) continue;
		QVariant old_value, new_value;
		old_value.setValue(before);
		new_value.setValue(properties);
		new QPropertyUndoCommand(c, "properties", old_value, new_value, undo);
		++changed;
	}
	if (!changed) {
		delete undo;
		return false;
	}
	m_project->undoStack()->push(undo);
	return true;
}

int WiringTableModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : int(m_conductors.size());
}

int WiringTableModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : ColumnCount;
}

QVariant WiringTableModel::data(const QModelIndex &index, int role) const
{
	Conductor *c = conductor(index.row());
	if (c && index.column() == WireColor) {
			//A swatch of how the wire is drawn
		if (role == Qt::DecorationRole) return c->properties().color;
		if (role == Qt::ToolTipRole)
			return tr("Um código IEC (BK, BN, RD, BU, GNYE...), um nome (preto,"
				  " azul...) ou #rrggbb também muda a cor do fio no esquema");
	}
		//Sorted by the order of the folios, not by the text of their number
	if (c && role == Qt::UserRole) {
		if (index.column() == Folio)
			return c->diagram() ? c->diagram()->folioIndex() : 0;
		return data(index, Qt::DisplayRole);
	}
		//The function links to the wiring rules
	if (c && index.column() == Function) {
		if (role == Qt::ForegroundRole)
			return QApplication::palette().brush(QPalette::Link);
		if (role == Qt::FontRole) {
			QFont font;
			font.setUnderline(true);
			return font;
		}
		if (role == Qt::ToolTipRole)
			return tr("Clique para escolher entre as funções das regras de fiação;"
				  " clique duas vezes para abrir as regras de fiação");
	}
	if (!c || (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ToolTipRole))
		return QVariant();

	switch (index.column()) {
		case Folio: {
				//The folio number of the title block (02.04)
			Diagram *diagram = c->diagram();
			if (!diagram) return QVariant();
			if (role == Qt::ToolTipRole) return diagram->title();
			return WiringList::folioLabel(diagram);
		}
		case From: return terminalName(c->terminal1);
		case To:   return terminalName(c->terminal2);
		case FromLabel:
			if (role == Qt::ToolTipRole)
				return tr("A etiqueta desta ponta, como desenhada na folha ou pela regra do projeto");
			return ConductorEndLabeling::shownLabel(c, true);
		case ToLabel:
			if (role == Qt::ToolTipRole)
				return tr("A etiqueta desta ponta, como desenhada na folha ou pela regra do projeto");
			return ConductorEndLabeling::shownLabel(c, false);
		default:   return field(c->properties(), index.column());
	}
}

QVariant WiringTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QAbstractTableModel::headerData(section, orientation, role);
	switch (section) {
		case Folio:     return tr("Folha");
		case Number:    return tr("Nº do fio");
		case From:      return tr("Origem");
		case To:        return tr("Destino");
		case FromLabel: return tr("Etiqueta da origem");
		case ToLabel:   return tr("Etiqueta do destino");
		case Function:  return tr("Função");
		case WireColor: return tr("Cor do fio");
		case Section:   return tr("Seção");
		case Cable:     return tr("Cabo");
		case Voltage:   return tr("Tensão / protocolo");
		case Bus:       return tr("Barramento");
		default:        return QVariant();
	}
}

Qt::ItemFlags WiringTableModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = QAbstractTableModel::flags(index);
	if (isEditable(index.column()) && m_project && !m_project->isReadOnly())
		flags |= Qt::ItemIsEditable;
	return flags;
}

bool WiringTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (role != Qt::EditRole) return false;
	return setValues({index.row()}, index.column(), value.toString().trimmed(),
			 m_whole_potential);
}

/**
	@brief WiringEditorDock::WiringEditorDock
	@param parent
*/
WiringEditorDock::WiringEditorDock(QWidget *parent) :
	QDockWidget(tr("Editor de fiação"), parent)
{
	setObjectName(QStringLiteral("wiring_editor_dock"));
	setFeatures(QDockWidget::DockWidgetClosable
		    | QDockWidget::DockWidgetMovable
		    | QDockWidget::DockWidgetFloatable);

	auto widget = new QWidget(this);
	auto layout = new QVBoxLayout(widget);
	layout->setContentsMargins(4, 4, 4, 4);

	auto top = new QHBoxLayout();
	auto filter = new QLineEdit(widget);
	filter->setPlaceholderText(tr("Filtrar (número, elemento, cor, seção...)"));
	filter->setClearButtonEnabled(true);
	top->addWidget(filter, 1);
	m_whole_potential = new QCheckBox(tr("Aplicar ao potencial inteiro"), widget);
	m_whole_potential->setChecked(true);
	m_whole_potential->setToolTip(tr("Uma alteração vale também para os outros fios"
					 " ligados ao mesmo potencial, como na janela de"
					 " propriedades do condutor"));
	top->addWidget(m_whole_potential);
	auto export_button = new QToolButton(widget);
	export_button->setText(tr("Exportar CSV..."));
	export_button->setToolTip(tr("Os fios mostrados (com o filtro), nas colunas da lista de fiação"));
	top->addWidget(export_button);
	layout->addLayout(top);
		//The wiring actions of the editor (addTools())
	m_tools = new QHBoxLayout();
	layout->addLayout(m_tools);

	m_model = new WiringTableModel(this);
	m_proxy = new QSortFilterProxyModel(this);
	m_proxy->setSourceModel(m_model);
	m_proxy->setFilterKeyColumn(-1);
	m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_proxy->setSortRole(Qt::UserRole);
	m_proxy->setSortLocaleAware(true);

	m_view = new QTableView(widget);
	m_view->setModel(m_proxy);
	m_view->setSortingEnabled(true);
	m_view->sortByColumn(WiringTableModel::Folio, Qt::AscendingOrder);
	m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
		//Double clicks are handled below: the function column opens
		//the wiring rules on a double click
	m_view->setEditTriggers(QAbstractItemView::EditKeyPressed
				| QAbstractItemView::AnyKeyPressed);
	m_view->setAlternatingRowColors(true);
	m_view->verticalHeader()->hide();
	m_view->verticalHeader()->setDefaultSectionSize(m_view->fontMetrics().height() + 6);
	m_view->horizontalHeader()->setStretchLastSection(true);
	m_view->setItemDelegate(new WiringDelegate(m_view, m_proxy, m_model));
	layout->addWidget(m_view);

	m_summary = new QLabel(widget);
	m_summary->setText(tr("Dica: selecione várias linhas e edite uma célula para"
			      " mudar todas de uma vez. Clique duas vezes no fólio,"
			      " na origem ou no destino para ver o fio no esquema;"
			      " na função, para abrir as regras de fiação."));
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);
	setWidget(widget);

	connect(filter, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);
	connect(filter, &QLineEdit::textChanged, this, &WiringEditorDock::updateSummary);
	connect(m_model, &QAbstractItemModel::modelReset, this, &WiringEditorDock::updateSummary);
	connect(m_whole_potential, &QCheckBox::toggled, this, [this](bool checked) {
		m_model->m_whole_potential = checked;
	});
	connect(export_button, &QToolButton::clicked, this, &WiringEditorDock::exportCsv);
		//A click on a function opens its list, unless it is the first
		//click of a double click
	m_function_click = new QTimer(this);
	m_function_click->setSingleShot(true);
	m_function_click->setInterval(QApplication::doubleClickInterval());
	connect(m_function_click, &QTimer::timeout, this, [this]() {
		if (m_function_index.isValid() && m_function_index == m_view->currentIndex())
			m_view->edit(m_function_index);
	});
	connect(m_view, &QTableView::clicked, this, [this](const QModelIndex &index) {
		if (index.column() != WiringTableModel::Function
		    || !(index.flags() & Qt::ItemIsEditable)
		    || QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
			return;
		m_function_index = index;
		m_function_click->start();
	});
	connect(m_view, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
		if (index.column() == WiringTableModel::Function) {
			m_function_click->stop();
			emit rulesRequested();
		} else if (!WiringTableModel::isEditable(index.column())) {
			showConductor(index);
		} else {
			m_view->edit(index);
		}
	});
}

/**
	@brief WiringEditorDock::setProject
	Show the wiring of project; follow its changes (any edit, undo or
	redo) until another project is set.
*/
void WiringEditorDock::setProject(QETProject *project)
{
	if (project == m_model->project()) return;
	disconnect(m_undo_connection);
	m_model->setProject(project);
	if (project) {
			//Queued: the edit that pushed the command may still be
			//committing its cell
		m_undo_connection = connect(project->undoStack(), &QUndoStack::indexChanged,
					    m_model, &WiringTableModel::reload, Qt::QueuedConnection);
	}
	m_view->resizeColumnsToContents();
	updateSummary();
}

void WiringEditorDock::updateSummary()
{
	if (!m_model->project()) {
		setWindowTitle(tr("Editor de fiação"));
		return;
	}
	setWindowTitle(tr("Editor de fiação - %1 de %2 fios")
		       .arg(m_proxy->rowCount()).arg(m_model->rowCount()));
}

void WiringEditorDock::showConductor(const QModelIndex &proxy_index)
{
	Conductor *c = m_model->conductor(m_proxy->mapToSource(proxy_index).row());
	if (!c || !c->diagram()) return;
	Diagram *diagram = c->diagram();
	diagram->showMe();
	diagram->clearSelection();
	c->setSelected(true);
	for (QGraphicsView *view : diagram->views())
		view->centerOn(c->sceneBoundingRect().center());
}

void WiringEditorDock::exportCsv()
{
	QETProject *project = m_model->project();
	if (!project) return;
	QList<WiringList::Wire> wires;
	for (int row = 0 ; row < m_proxy->rowCount() ; ++row)
		if (Conductor *c = m_model->conductor(m_proxy->mapToSource(m_proxy->index(row, 0)).row()))
			wires << WiringList::wire(c);
	WiringList::saveCsv(this, project, wires);
}

/**
	@brief WiringEditorDock::addTools
	Show actions as buttons over the table: the wiring actions of the
	project, at hand where the wires are edited
*/
void WiringEditorDock::addTools(const QList<QAction *> &actions)
{
	for (QAction *action : actions) {
		if (!action) continue;
		auto button = new QToolButton(widget());
		button->setDefaultAction(action);
		button->setToolButtonStyle(Qt::ToolButtonTextOnly);
		m_tools->addWidget(button);
	}
	m_tools->addStretch();
}
