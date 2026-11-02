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
#include "textstylesdialog.h"

#include "../qetapp.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace TextStyles;

namespace {
	enum Column { NameColumn, FontColumn, ColorColumn, TargetColumn, ColumnCount };

	QIcon swatch(const QColor &color)
	{
		QPixmap pixmap(16, 16);
		pixmap.fill(color);
		QPainter painter(&pixmap);
		painter.setPen(Qt::gray);
		painter.drawRect(0, 0, 15, 15);
		return QIcon(pixmap);
	}
}

TextStylesDialog::TextStylesDialog(const QList<Style> &styles, QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Estilos de texto"));
	resize(760, 360);
	auto layout = new QVBoxLayout(this);

	auto help = m_help = new QLabel(tr("Cada estilo define a fonte e a cor de um grupo de textos, como uma camada:"
				  " mudar o estilo muda todos os seus textos, em todos os fólios. Um estilo pode"
				  " pegar sozinho todos os textos de um tipo (todas as identificações, todos os"
				  " textos livres...); os outros textos recebem um estilo pelo menu Édition >"
				  " Estilo de texto. Mudar a fonte ou a cor de um texto à mão o desliga do estilo."), this);
	help->setWordWrap(true);
	layout->addWidget(help);

	m_table = new QTableWidget(0, ColumnCount, this);
	m_table->setHorizontalHeaderLabels({tr("Nome"), tr("Fonte"), tr("Cor"), tr("Aplicar sozinho a")});
	m_table->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::Interactive);
	m_table->horizontalHeader()->setSectionResizeMode(FontColumn, QHeaderView::Stretch);
	m_table->horizontalHeader()->setSectionResizeMode(ColorColumn, QHeaderView::ResizeToContents);
	m_table->horizontalHeader()->setSectionResizeMode(TargetColumn, QHeaderView::ResizeToContents);
	m_table->setColumnWidth(NameColumn, 150);
	m_table->verticalHeader()->setVisible(false);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	layout->addWidget(m_table, 1);

	auto row_buttons = new QHBoxLayout();
	auto add = new QPushButton(tr("Adicionar"), this);
	auto remove = new QPushButton(tr("Remover"), this);
	auto up = new QPushButton(tr("Subir"), this);
	auto down = new QPushButton(tr("Descer"), this);
	auto examples_button = m_examples_button = new QPushButton(tr("Usar exemplos"), this);
	examples_button->setToolTip(tr("Identificação, Função, Localização e Notas"));
	row_buttons->addWidget(add);
	row_buttons->addWidget(remove);
	row_buttons->addWidget(up);
	row_buttons->addWidget(down);
	row_buttons->addStretch();
	row_buttons->addWidget(examples_button);
	layout->addLayout(row_buttons);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Salvar e aplicar"));
	layout->addWidget(buttons);

	connect(add, &QPushButton::clicked, this, [this]() {
		readNames();
		Style style;
		style.font = QETApp::diagramTextsFont(9);
		int number = m_styles.size() + 1;
		do {
			style.name = tr("Estilo %1").arg(number++);
		} while (TextStyles::find(m_styles, style.name));
		addStyle(style);
		rebuild();
		m_table->setCurrentCell(m_table->rowCount() - 1, NameColumn);
		m_table->editItem(m_table->item(m_table->rowCount() - 1, NameColumn));
	});
	connect(remove, &QPushButton::clicked, this, [this]() {
		const int row = m_table->currentRow();
		if (row < 0) return;
		readNames();
		m_styles.removeAt(row);
		m_origins.removeAt(row);
		rebuild();
	});
	connect(up, &QPushButton::clicked, this, [this]() { moveCurrentRow(-1); });
	connect(down, &QPushButton::clicked, this, [this]() { moveCurrentRow(1); });
	connect(examples_button, &QPushButton::clicked, this, [this]() {
		readNames();
		for (const Style &style : m_examples)
			if (!TextStyles::find(m_styles, style.name)) addStyle(style);
		rebuild();
	});
	m_examples = examples();
	connect(buttons, &QDialogButtonBox::accepted, this, &TextStylesDialog::validate);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	for (const Style &style : styles)
		addStyle(style, style.name);
	rebuild();
}

QList<Style> TextStylesDialog::styles() const
{
	return m_styles;
}

/**
	@return the new name of each style renamed, by its old name
*/
QHash<QString, QString> TextStylesDialog::renames() const
{
	QHash<QString, QString> renames;
	for (int i = 0; i < m_styles.size(); ++i)
		if (!m_origins.at(i).isEmpty() && m_origins.at(i) != m_styles.at(i).name)
			renames.insert(m_origins.at(i), m_styles.at(i).name);
	return renames;
}

/**
	@brief TextStylesDialog::setForElements
	Edit the text styles of the element editor instead of a project's:
	they take no text by themselves, and "Usar exemplos" adds examples.
*/
void TextStylesDialog::setForElements(const QList<Style> &examples)
{
	setWindowTitle(tr("Estilos de texto dos elementos"));
	m_help->setText(tr("Cada estilo define a fonte e a cor de um tipo de texto dos elementos (nome do borne,"
			   " texto da caixa, r�tulo...). Escolha os textos e aplique um estilo pela barra Estilos"
			   " de texto; mudar o estilo aqui muda os textos do elemento aberto que o usam. Os estilos"
			   " valem para todos os elementos que voc� desenhar."));
	m_table->setColumnHidden(TargetColumn, true);
	m_examples = examples;
	QStringList names;
	for (const Style &style : examples)
		names << style.name;
	m_examples_button->setToolTip(names.join(QStringLiteral(", ")));
}

/**
	@brief TextStylesDialog::readNames
	Bring the names typed in the table to m_styles; the other columns
	write to m_styles as soon as they change.
*/
void TextStylesDialog::readNames()
{
	for (int row = 0; row < m_table->rowCount() && row < m_styles.size(); ++row)
		m_styles[row].name = m_table->item(row, NameColumn)->text().trimmed();
}

void TextStylesDialog::addStyle(const Style &style, const QString &origin)
{
	m_styles << style;
	m_origins << origin;
}

void TextStylesDialog::rebuild()
{
	const int current = m_table->currentRow();
	m_table->setRowCount(0);
	m_table->setRowCount(int(m_styles.size()));
	const QStringList targets = TextStyles::targets();

	for (int row = 0; row < m_styles.size(); ++row)
	{
		const Style &style = m_styles.at(row);
		m_table->setItem(row, NameColumn, new QTableWidgetItem(style.name));

			//The button shows the font and the colour of the style
		auto font_button = new QPushButton(fontDescription(style.font), m_table);
		QFont shown = style.font;
		shown.setPointSizeF(qBound(7.0, style.font.pointSizeF(), 14.0));
		font_button->setFont(shown);
		font_button->setStyleSheet(QStringLiteral("color: %1; text-align: left; padding: 2px 6px;")
					   .arg(style.color.name()));
		font_button->setToolTip(tr("Escolher a fonte, o tamanho, negrito e itálico"));
		m_table->setCellWidget(row, FontColumn, font_button);
		connect(font_button, &QPushButton::clicked, this, [this, row]() {
			bool ok = false;
			const QFont font = QFontDialog::getFont(&ok, m_styles.at(row).font, this,
								tr("Fonte do estilo %1").arg(m_styles.at(row).name));
			if (!ok) return;
			readNames();
			m_styles[row].font = font;
			rebuild();
		});

		auto color_button = new QPushButton(swatch(style.color), style.color.name(), m_table);
		m_table->setCellWidget(row, ColorColumn, color_button);
		connect(color_button, &QPushButton::clicked, this, [this, row]() {
			const QColor color = QColorDialog::getColor(m_styles.at(row).color, this,
								    tr("Cor do estilo %1").arg(m_styles.at(row).name));
			if (!color.isValid()) return;
			readNames();
			m_styles[row].color = color;
			rebuild();
		});

		auto target = new QComboBox(m_table);
		for (const QString &key : targets)
			target->addItem(targetName(key), key);
		target->setCurrentIndex(qMax(0, targets.indexOf(style.target)));
		m_table->setCellWidget(row, TargetColumn, target);
		connect(target, &QComboBox::currentIndexChanged, this, [this, row, target]() {
			m_styles[row].target = target->currentData().toString();
		});
	}
	m_table->resizeRowsToContents();
	if (current >= 0 && current < m_table->rowCount())
		m_table->setCurrentCell(current, NameColumn);
}

void TextStylesDialog::moveCurrentRow(int offset)
{
	const int row = m_table->currentRow();
	const int other = row + offset;
	if (row < 0 || other < 0 || other >= m_styles.size()) return;
	readNames();
	m_styles.swapItemsAt(row, other);
	m_origins.swapItemsAt(row, other);
	rebuild();
	m_table->setCurrentCell(other, NameColumn);
}

/**
	@brief TextStylesDialog::validate
	Accept only names that are set and unique: a text finds its style by
	name. Two styles taking the same texts by themselves only get a
	warning, the first one wins.
*/
void TextStylesDialog::validate()
{
	readNames();
	QStringList names;
	for (const Style &style : std::as_const(m_styles))
	{
		if (style.name.isEmpty() || style.name == NoStyle) {
			QMessageBox::warning(this, windowTitle(), tr("Todo estilo precisa de um nome."));
			return;
		}
		if (names.contains(style.name, Qt::CaseSensitive)) {
			QMessageBox::warning(this, windowTitle(),
					     tr("Há dois estilos chamados \"%1\".").arg(style.name));
			return;
		}
		names << style.name;
	}

	QStringList taken, twice;
	for (const Style &style : std::as_const(m_styles))
	{
		if (style.target.isEmpty()) continue;
		if (taken.contains(style.target)) twice << targetName(style.target);
		taken << style.target;
	}
	if (!twice.isEmpty()
	    && QMessageBox::question(this, windowTitle(),
				     tr("Mais de um estilo pega sozinho: %1. Vale o primeiro da lista. Continuar?")
				     .arg(twice.join(QStringLiteral("; ")))) != QMessageBox::Yes)
		return;
	accept();
}
