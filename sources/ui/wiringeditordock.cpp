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

#include <QCheckBox>
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

			void setModelData(QWidget *editor, QAbstractItemModel *,
					  const QModelIndex &index) const override
			{
				auto line = qobject_cast<QLineEdit *>(editor);
				if (!line) return;
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
				m_model->setValues(rows, source.column(), line->text().trimmed(),
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
	return column >= Number && column != From && column != To;
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
	if (!c || (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ToolTipRole))
		return QVariant();

	switch (index.column()) {
		case Folio: {
			Diagram *diagram = c->diagram();
			if (!diagram) return QVariant();
			if (role == Qt::ToolTipRole) return diagram->title();
			return QStringLiteral("%1").arg(diagram->folioIndex() + 1, 3, 10, QLatin1Char('0'));
		}
		case From: return terminalName(c->terminal1);
		case To:   return terminalName(c->terminal2);
		default:   return field(c->properties(), index.column());
	}
}

QVariant WiringTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QAbstractTableModel::headerData(section, orientation, role);
	switch (section) {
		case Folio:     return tr("Fólio");
		case Number:    return tr("Número");
		case From:      return tr("Origem");
		case To:        return tr("Destino");
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
	top->addWidget(export_button);
	layout->addLayout(top);

	m_model = new WiringTableModel(this);
	m_proxy = new QSortFilterProxyModel(this);
	m_proxy->setSourceModel(m_model);
	m_proxy->setFilterKeyColumn(-1);
	m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

	m_view = new QTableView(widget);
	m_view->setModel(m_proxy);
	m_view->setSortingEnabled(true);
	m_view->sortByColumn(WiringTableModel::Folio, Qt::AscendingOrder);
	m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_view->setEditTriggers(QAbstractItemView::DoubleClicked
				| QAbstractItemView::EditKeyPressed
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
			      " na origem ou no destino para ver o fio no esquema."));
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
	connect(m_view, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
		if (!WiringTableModel::isEditable(index.column()))
			showConductor(index);
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
	const QString dir = project->filePath().isEmpty()
			? QString()
			: QFileInfo(project->filePath()).absolutePath() + QLatin1Char('/')
			  + QFileInfo(project->filePath()).completeBaseName() + tr("-fiacao.csv");
	const QString path = QFileDialog::getSaveFileName(this, tr("Exportar a fiação"),
							  dir, tr("Arquivos CSV (*.csv)"));
	if (path.isEmpty()) return;

	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QET::QetMessageBox::critical(this, tr("Exportar a fiação"),
			tr("Não foi possível abrir o arquivo para escrita."));
		return;
	}
	QTextStream out(&file);
	out.setEncoding(QStringConverter::Utf8);
	out.setGenerateByteOrderMark(true);	// accents in Excel
	auto cell = [](QString text) {
		text.replace(QLatin1Char('"'), QLatin1String("\"\""));
		return QLatin1Char('"') + text + QLatin1Char('"');
	};
		//Semicolons: what Excel expects with a Brazilian locale
	QStringList header;
	for (int column = 0 ; column < m_proxy->columnCount() ; ++column)
		header << cell(m_proxy->headerData(column, Qt::Horizontal).toString());
	out << header.join(QLatin1Char(';')) << '\n';
	for (int row = 0 ; row < m_proxy->rowCount() ; ++row) {
		QStringList line;
		for (int column = 0 ; column < m_proxy->columnCount() ; ++column)
			line << cell(m_proxy->index(row, column).data().toString());
		out << line.join(QLatin1Char(';')) << '\n';
	}
	if (!file.commit()) {
		QET::QetMessageBox::critical(this, tr("Exportar a fiação"),
			tr("Não foi possível gravar o arquivo."));
	}
}
