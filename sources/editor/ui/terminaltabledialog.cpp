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
#include "terminaltabledialog.h"

#include "../../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../../qet.h"
#include "../../qeticons.h"
#include "../elementscene.h"
#include "../graphicspart/customelementpart.h"
#include "../graphicspart/partterminal.h"
#include "../modulegenerator.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {
	enum Column { NameColumn, XColumn, YColumn, OrientationColumn, TypeColumn };
		///The terminal of a row, kept on its name item so it follows sorting
	constexpr int terminal_role = Qt::UserRole + 1;

	QList<QPair<int, QString>> orientations()
	{
			//The terminal editor's own words, already translated
		return {
			{Qet::North, QCoreApplication::translate("TerminalEditor", "Nord")},
			{Qet::East,  QCoreApplication::translate("TerminalEditor", "Est")},
			{Qet::South, QCoreApplication::translate("TerminalEditor", "Sud")},
			{Qet::West,  QCoreApplication::translate("TerminalEditor", "Ouest")}};
	}

	QList<QPair<int, QString>> types()
	{
		return {
			{TerminalData::Generic, QCoreApplication::translate("TerminalEditor", "Générique")},
			{TerminalData::Inner,   QCoreApplication::translate("TerminalEditor", "Bornier intérieur")},
			{TerminalData::Outer,   QCoreApplication::translate("TerminalEditor", "Bornier extérieur")},
			{TerminalData::No,      QCoreApplication::translate("TerminalEditor", "NO (contact SW)")},
			{TerminalData::Nc,      QCoreApplication::translate("TerminalEditor", "NC (contact SW)")},
			{TerminalData::Common,  QCoreApplication::translate("TerminalEditor", "Commun (contact SW)")}};
	}

	QString label(const QList<QPair<int, QString>> &choices, int value)
	{
		for (const auto &choice : choices)
			if (choice.first == value) return choice.second;
		return QString::number(value);
	}

		///A choice cell shows its label and keeps its value in Qt::UserRole
	QTableWidgetItem *choiceItem(const QList<QPair<int, QString>> &choices, int value)
	{
		auto item = new QTableWidgetItem(label(choices, value));
		item->setData(Qt::UserRole, value);
		return item;
	}

	class TerminalDelegate : public QStyledItemDelegate
	{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
					      const QModelIndex &index) const override
			{
				if (index.column() == XColumn || index.column() == YColumn) {
					auto spin = new QDoubleSpinBox(parent);
					spin->setRange(-100000, 100000);
					spin->setDecimals(1);
					return spin;
				}
				if (index.column() == OrientationColumn || index.column() == TypeColumn) {
					auto combo = new QComboBox(parent);
					const auto choices = index.column() == OrientationColumn ? orientations() : types();
					for (const auto &choice : choices)
						combo->addItem(choice.second, choice.first);
					return combo;
				}
				return QStyledItemDelegate::createEditor(parent, option, index);
			}

			void setEditorData(QWidget *editor, const QModelIndex &index) const override
			{
				if (auto combo = qobject_cast<QComboBox *>(editor))
					combo->setCurrentIndex(combo->findData(index.data(Qt::UserRole)));
				else
					QStyledItemDelegate::setEditorData(editor, index);
			}

			void setModelData(QWidget *editor, QAbstractItemModel *model,
					  const QModelIndex &index) const override
			{
				if (auto combo = qobject_cast<QComboBox *>(editor)) {
					model->setData(index, combo->currentData(), Qt::UserRole);
					model->setData(index, combo->currentText(), Qt::DisplayRole);
				} else {
					QStyledItemDelegate::setModelData(editor, model, index);
				}
			}
	};
}

/**
	@brief TerminalTableDialog::TerminalTableDialog
	@param scene : the scene of the edited element
	@param parent
*/
TerminalTableDialog::TerminalTableDialog(ElementScene *scene, QWidget *parent) :
	QDialog(parent),
	m_scene(scene)
{
	setWindowTitle(tr("Tabela de bornes"));
	resize(620, 480);
	auto layout = new QVBoxLayout(this);

	QList<PartTerminal *> terminals;
	for (CustomElementPart *part : scene->primitives())
		if (auto terminal = qgraphicsitem_cast<PartTerminal *>(part->toItem()))
			terminals << terminal;
		//Top to bottom, then left to right, as one reads a symbol
	std::stable_sort(terminals.begin(), terminals.end(), [](PartTerminal *a, PartTerminal *b) {
		if (!qFuzzyCompare(a->scenePos().y(), b->scenePos().y()))
			return a->scenePos().y() < b->scenePos().y();
		return a->scenePos().x() < b->scenePos().x();
	});

	m_table = new QTableWidget(int(terminals.size()), 5, this);
	m_table->setHorizontalHeaderLabels({tr("Nome"), tr("X"), tr("Y"), tr("Orientação"), tr("Tipo")});
	m_table->verticalHeader()->hide();
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_table->setEditTriggers(QAbstractItemView::DoubleClicked
				 | QAbstractItemView::EditKeyPressed
				 | QAbstractItemView::AnyKeyPressed);
	m_table->setItemDelegate(new TerminalDelegate(m_table));
	m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	for (int row = 0; row < terminals.size(); ++row) {
		PartTerminal *t = terminals.at(row);
		auto name = new QTableWidgetItem(t->terminalName());
		name->setData(terminal_role, QVariant::fromValue<quintptr>(quintptr(t)));
		m_table->setItem(row, NameColumn, name);
		auto x = new QTableWidgetItem();
		x->setData(Qt::DisplayRole, t->pos().x());
		m_table->setItem(row, XColumn, x);
		auto y = new QTableWidgetItem();
		y->setData(Qt::DisplayRole, t->pos().y());
		m_table->setItem(row, YColumn, y);
		m_table->setItem(row, OrientationColumn, choiceItem(orientations(), t->orientation()));
		m_table->setItem(row, TypeColumn, choiceItem(types(), t->terminalType()));
	}
	m_table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
	m_table->setSortingEnabled(true);
	layout->addWidget(m_table, 1);

	auto rename = new QHBoxLayout();
	rename->addWidget(new QLabel(tr("Renomear as linhas selecionadas:"), this));
	m_prefix = new QLineEdit(this);
	m_prefix->setPlaceholderText(tr("prefixo, ou lista: L1, L2, L3, N, PE"));
	m_prefix->setToolTip(tr("Com vírgulas, cada nome da lista vai para uma linha, na ordem"
				" da tabela; sem vírgulas, é o prefixo da numeração"));
	rename->addWidget(m_prefix, 1);
	m_numbering = new QComboBox(this);
	m_numbering->addItem(tr("1, 2, 3..."));
	m_numbering->addItem(tr("Byte.bit"));
	rename->addWidget(m_numbering);
	rename->addWidget(new QLabel(tr("início"), this));
	m_start = new QSpinBox(this);
	m_start->setRange(0, 99999);
	m_start->setValue(1);
	rename->addWidget(m_start);
	rename->addWidget(new QLabel(tr("passo"), this));
	m_step = new QSpinBox(this);
	m_step->setRange(1, 100);
	rename->addWidget(m_step);
	auto rename_button = new QPushButton(tr("Renomear"), this);
	rename->addWidget(rename_button);
	layout->addLayout(rename);

	m_status = new QLabel(this);
	m_status->setWordWrap(true);
	layout->addWidget(m_status);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addWidget(buttons);

	connect(rename_button, &QPushButton::clicked, this, &TerminalTableDialog::renameSelection);
	connect(m_prefix, &QLineEdit::returnPressed, this, &TerminalTableDialog::renameSelection);
	connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
		if (item->column() == NameColumn) checkNames();
	});
	connect(m_table, &QTableWidget::itemSelectionChanged, this, &TerminalTableDialog::selectInScene);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	checkNames();
}

PartTerminal *TerminalTableDialog::terminal(int row) const
{
	QTableWidgetItem *item = m_table->item(row, NameColumn);
	return item ? reinterpret_cast<PartTerminal *>(item->data(terminal_role).value<quintptr>()) : nullptr;
}

/**
	@brief TerminalTableDialog::renameSelection
	Give the selected rows, in the order the table shows them, the names
	of the series (or of the comma separated list).
*/
void TerminalTableDialog::renameSelection()
{
	QList<int> rows;
	for (const QModelIndex &index : m_table->selectionModel()->selectedRows())
		rows << index.row();
	std::sort(rows.begin(), rows.end());
	if (rows.isEmpty()) {
		m_status->setText(tr("Selecione as linhas a renomear (Ctrl ou Shift + clique)."));
		return;
	}

	QStringList names;
	const QString text = m_prefix->text();
	if (text.contains(QLatin1Char(','))) {
		for (const QString &name : text.split(QLatin1Char(',')))
			names << name.trimmed();
	} else {
		ModuleGenerator::SidePins series;
		series.count = int(rows.size());
		series.prefix = text.trimmed();
		series.numbering = ModuleGenerator::Numbering(m_numbering->currentIndex());
		series.start = m_start->value();
		series.step = m_step->value();
		names = ModuleGenerator::pinNames(series);
	}

		//Renaming must not move the rows under the loop
	m_table->setSortingEnabled(false);
	for (int i = 0; i < rows.size() && i < names.size(); ++i)
		m_table->item(rows.at(i), NameColumn)->setText(names.at(i));
	m_table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
	m_table->setSortingEnabled(true);
	checkNames();
}

/**
	Mark the names used twice and say how many terminals have none.
*/
void TerminalTableDialog::checkNames()
{
	QHash<QString, int> counts;
	for (int row = 0; row < m_table->rowCount(); ++row)
		if (QTableWidgetItem *item = m_table->item(row, NameColumn))
			++counts[item->text().trimmed()];

	const QSignalBlocker blocker(m_table);
	int duplicates = 0;
	for (int row = 0; row < m_table->rowCount(); ++row) {
		QTableWidgetItem *item = m_table->item(row, NameColumn);
		if (!item) continue;
		const QString name = item->text().trimmed();
		const bool duplicate = !name.isEmpty() && counts.value(name) > 1;
		if (duplicate) ++duplicates;
		item->setBackground(duplicate ? QBrush(QColor(255, 200, 200)) : QBrush());
		item->setToolTip(duplicate ? tr("Nome repetido") : QString());
	}

	QStringList status;
	status << tr("%n borne(s).", nullptr, m_table->rowCount());
	if (duplicates) status << tr("%n com nome repetido (em vermelho).", nullptr, duplicates);
	if (const int unnamed = counts.value(QString())) status << tr("%n sem nome.", nullptr, unnamed);
	status << tr("Clique no cabeçalho para ordenar; dê dois cliques numa célula para editar.");
	m_status->setText(status.join(QLatin1Char(' ')));
}

/**
	Select in the editor the terminals of the selected rows.
*/
void TerminalTableDialog::selectInScene()
{
	if (!m_scene) return;
	m_scene->clearSelection();
	for (const QModelIndex &index : m_table->selectionModel()->selectedRows())
		if (PartTerminal *t = terminal(index.row()))
			t->setSelected(true);
}

/**
	@return the changes made in the table as one undo command, or nullptr
	if there is none
*/
QUndoCommand *TerminalTableDialog::undoCommand() const
{
	auto undo = new QUndoCommand(tr("Editar a tabela de bornes"));
	int changed = 0;
	for (int row = 0; row < m_table->rowCount(); ++row) {
		PartTerminal *t = terminal(row);
		if (!t) continue;
		const QString name = m_table->item(row, NameColumn)->text().trimmed();
		if (name != t->terminalName()) {
			new QPropertyUndoCommand(t, "terminal_name", t->terminalName(), name, undo);
			++changed;
		}
		const QPointF pos(m_table->item(row, XColumn)->data(Qt::DisplayRole).toDouble(),
				  m_table->item(row, YColumn)->data(Qt::DisplayRole).toDouble());
		if (pos != t->pos()) {
			new QPropertyUndoCommand(t, "pos", t->pos(), pos, undo);
			++changed;
		}
		const int orientation = m_table->item(row, OrientationColumn)->data(Qt::UserRole).toInt();
		if (orientation != t->orientation()) {
			new QPropertyUndoCommand(t, "orientation", t->property("orientation"),
						 orientation, undo);
			++changed;
		}
		const int type = m_table->item(row, TypeColumn)->data(Qt::UserRole).toInt();
		if (type != t->terminalType()) {
			new QPropertyUndoCommand(t, "terminal_type", t->property("terminal_type"),
						 type, undo);
			++changed;
		}
	}
	if (!changed) {
		delete undo;
		return nullptr;
	}
	return undo;
}
