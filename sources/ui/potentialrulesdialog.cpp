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
#include "potentialrulesdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
	enum Column { FieldColumn, PatternColumn, ColorColumn, SectionColumn };

	const QList<PotentialRules::Field> &fields()
	{
		static const QList<PotentialRules::Field> list = {
			PotentialRules::Voltage, PotentialRules::Function,
			PotentialRules::Number, PotentialRules::Bus, PotentialRules::Cable};
		return list;
	}

		///A swatch of colors, half and half for a bicolour wire
	QIcon swatch(const QList<QColor> &colors)
	{
		if (colors.isEmpty()) return QIcon();
		QPixmap pixmap(16, 16);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setPen(Qt::darkGray);
		painter.setBrush(colors.first());
		painter.drawRect(0, 0, 15, 15);
		if (colors.size() > 1) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(colors.last());
			painter.drawRect(8, 1, 7, 14);
		}
		return QIcon(pixmap);
	}
}

/**
	@brief PotentialRulesDialog::PotentialRulesDialog
	@param settings : the rules to edit
	@param parent
*/
PotentialRulesDialog::PotentialRulesDialog(const PotentialRules::Settings &settings,
					   QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Regras de cor e bitola"));
	resize(640, 420);
	auto layout = new QVBoxLayout(this);

	auto help = new QLabel(tr("A primeira regra que combina com um fio dá a ele a cor e a"
				  " seção. No valor, maiúsculas e espaços não importam;"
				  " use * e ? como curingas (L? vale para L1, L2 e L3). Na"
				  " cor, use um código IEC (BK, BN, RD, BU, LBU, GNYE...),"
				  " um nome (preto, azul claro...) ou #rrggbb."), this);
	help->setWordWrap(true);
	layout->addWidget(help);

	m_automatic = new QCheckBox(tr("Aplicar automaticamente quando um fio passa a combinar"
				       " com uma regra"), this);
	m_automatic->setToolTip(tr("Ao digitar a tensão ou a função de um fio nas propriedades"
				   " do condutor ou no editor de fiação. Uma cor ou seção"
				   " digitada na mesma edição prevalece."));
	layout->addWidget(m_automatic);

	m_table = new QTableWidget(0, 4, this);
	m_table->setHorizontalHeaderLabels({tr("Campo"), tr("Valor"), tr("Cor"), tr("Seção (mm²)")});
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->verticalHeader()->hide();
	m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	layout->addWidget(m_table, 1);

	auto rows = new QHBoxLayout();
	auto add = new QPushButton(tr("Adicionar"), this);
	auto remove = new QPushButton(tr("Remover"), this);
	auto up = new QPushButton(tr("Subir"), this);
	auto down = new QPushButton(tr("Descer"), this);
	auto examples = new QPushButton(tr("Usar exemplos"), this);
	examples->setToolTip(tr("Fases L1, L2, L3 pretas, neutro azul claro, PE verde-amarelo,"
				" 24 VCC e 0 VCC azuis (IEC 60204-1 / NBR 5410)"));
	for (QPushButton *button : {add, remove, up, down})
		rows->addWidget(button);
	rows->addStretch();
	rows->addWidget(examples);
	layout->addLayout(rows);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	auto apply_all = buttons->addButton(tr("Salvar e aplicar a todos os fios"),
					    QDialogButtonBox::ActionRole);
	apply_all->setToolTip(tr("Dá a cor e a seção das regras a todos os fios que combinam,"
				 " mesmo os que já têm outras"));
	auto legend = buttons->addButton(tr("Salvar e inserir legenda no fólio"),
					 QDialogButtonBox::ActionRole);
	layout->addWidget(buttons);

	connect(add, &QPushButton::clicked, this, [this]() {
		addRow(PotentialRules::Rule());
		m_table->setCurrentCell(m_table->rowCount() - 1, PatternColumn);
		m_table->editItem(m_table->item(m_table->rowCount() - 1, PatternColumn));
	});
	connect(remove, &QPushButton::clicked, this, [this]() {
		if (m_table->currentRow() >= 0) m_table->removeRow(m_table->currentRow());
	});
	connect(up, &QPushButton::clicked, this, [this]() {moveCurrentRow(-1);});
	connect(down, &QPushButton::clicked, this, [this]() {moveCurrentRow(1);});
	connect(examples, &QPushButton::clicked, this, [this]() {
		PotentialRules::Settings current = this->settings();
		current.rules += PotentialRules::examples().rules;
		fill(current);
	});
	connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
		if (item->column() == ColorColumn) updateSwatch(item->row());
	});
	connect(buttons, &QDialogButtonBox::accepted, this, [this]() {closeWith(NoAction);});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(apply_all, &QPushButton::clicked, this, [this]() {closeWith(ApplyAll);});
	connect(legend, &QPushButton::clicked, this, [this]() {closeWith(InsertLegend);});

	fill(settings);
}

/**
	@return the rules as edited, the empty rows left out
*/
PotentialRules::Settings PotentialRulesDialog::settings() const
{
	PotentialRules::Settings settings;
	settings.automatic = m_automatic->isChecked();
	for (int row = 0; row < m_table->rowCount(); ++row) {
		PotentialRules::Rule rule;
		auto combo = qobject_cast<QComboBox *>(m_table->cellWidget(row, FieldColumn));
		rule.field = combo ? fields().at(combo->currentIndex()) : PotentialRules::Voltage;
		auto text = [this, row](int column) {
			QTableWidgetItem *item = m_table->item(row, column);
			return item ? item->text().trimmed() : QString();
		};
		rule.pattern = text(PatternColumn);
		rule.color = text(ColorColumn);
		rule.section = text(SectionColumn);
		if (rule.pattern.isEmpty() && rule.color.isEmpty() && rule.section.isEmpty())
			continue;
		settings.rules << rule;
	}
	return settings;
}

void PotentialRulesDialog::fill(const PotentialRules::Settings &settings)
{
	m_automatic->setChecked(settings.automatic);
	m_table->setRowCount(0);
	for (const PotentialRules::Rule &rule : settings.rules)
		addRow(rule);
}

void PotentialRulesDialog::addRow(const PotentialRules::Rule &rule)
{
	const int row = m_table->rowCount();
	m_table->insertRow(row);
	auto combo = new QComboBox(m_table);
	for (PotentialRules::Field field : fields())
		combo->addItem(PotentialRules::fieldName(field));
	combo->setCurrentIndex(int(fields().indexOf(rule.field)));
	m_table->setCellWidget(row, FieldColumn, combo);
	m_table->setItem(row, PatternColumn, new QTableWidgetItem(rule.pattern));
	m_table->setItem(row, ColorColumn, new QTableWidgetItem(rule.color));
	m_table->setItem(row, SectionColumn, new QTableWidgetItem(rule.section));
	updateSwatch(row);
}

void PotentialRulesDialog::moveCurrentRow(int offset)
{
	const int row = m_table->currentRow();
	PotentialRules::Settings current = this->settings();
	if (row < 0 || row >= current.rules.size()) return;
	const int target = row + offset;
	if (target < 0 || target >= current.rules.size()) return;
	current.rules.swapItemsAt(row, target);
	fill(current);
	m_table->setCurrentCell(target, PatternColumn);
}

void PotentialRulesDialog::updateSwatch(int row)
{
	QTableWidgetItem *item = m_table->item(row, ColorColumn);
	if (!item) return;
	const QSignalBlocker blocker(m_table);
	item->setIcon(swatch(PotentialRules::drawingColors(item->text())));
	item->setToolTip(PotentialRules::colorName(item->text()));
}

void PotentialRulesDialog::closeWith(Action action)
{
	m_action = action;
	accept();
}
