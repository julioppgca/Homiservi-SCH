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
#include "connectordialog.h"

#include "../elementpreview.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
	const QString last_key = QStringLiteral("elementeditor/connector_last");
	const QString templates_key = QStringLiteral("elementeditor/connector_templates");

		///The templates the user saved: name -> settings
	QJsonObject templates()
	{
		return QJsonDocument::fromJson(QSettings().value(templates_key).toByteArray()).object();
	}

	void setTemplates(const QJsonObject &object)
	{
		QSettings().setValue(templates_key, QJsonDocument(object).toJson(QJsonDocument::Compact));
	}
}

ConnectorDialog::ConnectorDialog(bool has_drawing, QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Gerar conector"));
	resize(980, 620);
	auto layout = new QVBoxLayout(this);
	auto splitter = new QSplitter(this);

	auto left = new QWidget(splitter);
	auto left_layout = new QVBoxLayout(left);
	left_layout->setContentsMargins(0, 0, 0, 0);
	auto form = m_form = new QFormLayout();
	left_layout->addLayout(form);

	auto template_row = new QHBoxLayout();
	m_templates = new QComboBox(left);
	auto save_template = new QPushButton(tr("Salvar..."), left);
	save_template->setToolTip(tr("Guarda o conector e os sinais como modelo (ex.: FANUC JD1A I/O Link)"));
	auto delete_template = new QPushButton(tr("Excluir"), left);
	template_row->addWidget(m_templates, 1);
	template_row->addWidget(save_template);
	template_row->addWidget(delete_template);
	form->addRow(tr("Modelo salvo:"), template_row);

	m_family = new QComboBox(left);
	for (int f = 0; f < ConnectorGenerator::FamilyCount; ++f)
		m_family->addItem(ConnectorGenerator::familyName(ConnectorGenerator::Family(f)), f);
	form->addRow(tr("Família:"), m_family);
	m_variant = new QComboBox(left);
	form->addRow(tr("Conector:"), m_variant);
	m_gender = new QComboBox(left);
	m_gender->addItems({tr("Macho (pinos)"), tr("Fêmea (soquetes)")});
	form->addRow(tr("Gênero:"), m_gender);
	m_preset = new QComboBox(left);
	form->addRow(tr("Sinais:"), m_preset);
	m_colours = new QComboBox(left);
	m_colours->addItems({tr("Sem cores"), tr("T568A"), tr("T568B")});
	form->addRow(tr("Cores do cabo:"), m_colours);

	m_generic = new QWidget(left);
	auto generic_form = new QFormLayout(m_generic);
	generic_form->setContentsMargins(0, 0, 0, 0);
	m_rows = new QSpinBox(m_generic);
	m_rows->setRange(1, 26);
	m_columns = new QSpinBox(m_generic);
	m_columns->setRange(1, 60);
	auto size_row = new QHBoxLayout();
	size_row->addWidget(m_rows);
	size_row->addWidget(new QLabel(QStringLiteral("×"), m_generic));
	size_row->addWidget(m_columns);
	generic_form->addRow(tr("Fileiras × colunas:"), size_row);
	m_numbering = new QComboBox(m_generic);
	m_numbering->addItems({tr("Por fileira (1, 2, 3...)"), tr("Por coluna"), tr("Letra + número (A1, B1...)")});
	generic_form->addRow(tr("Numeração:"), m_numbering);
	m_prefix = new QLineEdit(m_generic);
	m_first = new QSpinBox(m_generic);
	m_first->setRange(0, 999);
	auto prefix_row = new QHBoxLayout();
	prefix_row->addWidget(m_prefix);
	prefix_row->addWidget(new QLabel(tr("começando em"), m_generic));
	prefix_row->addWidget(m_first);
	generic_form->addRow(tr("Prefixo:"), prefix_row);
	form->addRow(m_generic);

	auto draw_row = new QHBoxLayout();
	m_symbol = new QCheckBox(tr("Símbolo"), left);
	m_front_view = new QCheckBox(tr("Vista frontal"), left);
	draw_row->addWidget(m_symbol);
	draw_row->addWidget(m_front_view);
	draw_row->addStretch();
	form->addRow(tr("Desenhar:"), draw_row);
	m_face = new QComboBox(left);
	m_face->addItems({tr("Lado do encaixe"), tr("Lado da solda (traseira)")});
	form->addRow(tr("Vista:"), m_face);
	m_side = new QComboBox(left);
	m_side->addItems({tr("À esquerda"), tr("À direita"), tr("Dos dois lados")});
	form->addRow(tr("Pinos do símbolo:"), m_side);
	m_style = new QComboBox(left);
	m_style->addItems({tr("Caixa com os pinos ao lado"), tr("Como nos diagramas FANUC")});
	form->insertRow(form->rowCount() - 1, tr("Símbolo:"), m_style);
	m_edge = new QComboBox(left);
	m_edge->addItems({tr("Por cima da unidade"), tr("Por baixo da unidade")});
	form->addRow(tr("Saída dos cabos:"), m_edge);
	m_title = new QLineEdit(left);
	m_title->setPlaceholderText(tr("ex.: JD1A"));
	form->addRow(tr("Nome do conector:"), m_title);
	m_used_only = new QCheckBox(tr("Desenhar só os pinos com sinal"), left);
	form->addRow(QString(), m_used_only);

	m_table = new QTableWidget(0, 2, left);
	m_table->setHorizontalHeaderLabels({tr("Pino"), tr("Sinal")});
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->verticalHeader()->hide();
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	left_layout->addWidget(m_table, 1);

	auto right = new QWidget(splitter);
	auto right_layout = new QVBoxLayout(right);
	right_layout->setContentsMargins(0, 0, 0, 0);
	m_preview_scene = new QGraphicsScene(this);
	m_preview = new QGraphicsView(m_preview_scene, right);
	m_preview->setRenderHint(QPainter::Antialiasing);
	m_preview->setBackgroundBrush(Qt::white);
	right_layout->addWidget(m_preview, 1);
	auto hint = new QLabel(tr("Só RS-232, VGA, Ethernet e USB vêm com sinais prontos, por serem padrões públicos. "
				  "Para os outros, digite os sinais do manual na tabela (ou leia o datasheet) e salve como modelo."), right);
	hint->setWordWrap(true);
	right_layout->addWidget(hint);
	auto name_form = new QFormLayout();
	m_name = new QLineEdit(right);
	name_form->addRow(tr("Nome do elemento:"), m_name);
	right_layout->addLayout(name_form);
	m_replace = new QCheckBox(tr("Substituir o desenho atual"), right);
	m_replace->setChecked(has_drawing);
	m_replace->setVisible(has_drawing);
	right_layout->addWidget(m_replace);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({400, 580});
	layout->addWidget(splitter, 1);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Gerar"));
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	m_settings = ConnectorGenerator::Settings::fromJson(
		QJsonDocument::fromJson(QSettings().value(last_key).toByteArray()).object());
	if (!ConnectorGenerator::variant(m_settings.variant))
		m_settings = ConnectorGenerator::Settings();
	fillTemplates();
	showSettings();

	connect(m_family, &QComboBox::currentIndexChanged, this, [this]() {
		if (m_updating)
			return;
		fillVariants();
		variantChanged();
	});
	connect(m_variant, &QComboBox::currentIndexChanged, this, [this]() {
		if (!m_updating)
			variantChanged();
	});
		//What changes the pins or their signals
	auto signals_changed = [this]() {
		if (m_updating)
			return;
		readSettings();
		m_settings.signal_names = ConnectorGenerator::presetSignals(m_settings);
		followTitle();
		fillSignals();
		updatePreview();
	};
	connect(m_preset, &QComboBox::currentIndexChanged, this, signals_changed);
	connect(m_colours, &QComboBox::currentIndexChanged, this, signals_changed);
	connect(m_rows, &QSpinBox::valueChanged, this, signals_changed);
	connect(m_columns, &QSpinBox::valueChanged, this, signals_changed);
	connect(m_numbering, &QComboBox::currentIndexChanged, this, signals_changed);
	connect(m_prefix, &QLineEdit::textChanged, this, signals_changed);
	connect(m_first, &QSpinBox::valueChanged, this, signals_changed);
		//What changes only the drawing
	auto drawing_changed = [this]() {
		if (m_updating)
			return;
		readSettings();
		updatePreview();
	};
	connect(m_gender, &QComboBox::currentIndexChanged, this, drawing_changed);
	connect(m_symbol, &QCheckBox::toggled, this, drawing_changed);
	connect(m_front_view, &QCheckBox::toggled, this, drawing_changed);
	connect(m_face, &QComboBox::currentIndexChanged, this, drawing_changed);
	connect(m_side, &QComboBox::currentIndexChanged, this, drawing_changed);
	connect(m_style, &QComboBox::currentIndexChanged, this, [this]() {
		if (m_updating)
			return;
		readSettings();
		showSettings();
	});
	connect(m_edge, &QComboBox::currentIndexChanged, this, drawing_changed);
	connect(m_used_only, &QCheckBox::toggled, this, drawing_changed);
	connect(m_title, &QLineEdit::textChanged, this, drawing_changed);
	connect(m_table, &QTableWidget::cellChanged, this, [this](int row, int column) {
		if (m_updating || column != 1)
			return;
		while (m_settings.signal_names.size() <= row)
			m_settings.signal_names << QString();
		m_settings.signal_names[row] = m_table->item(row, column)->text().trimmed();
		updatePreview();
	});
	connect(m_templates, &QComboBox::activated, this, &ConnectorDialog::loadTemplate);
	connect(save_template, &QPushButton::clicked, this, &ConnectorDialog::saveTemplate);
	connect(delete_template, &QPushButton::clicked, this, &ConnectorDialog::deleteTemplate);
	connect(this, &QDialog::accepted, this, [this]() {
		QSettings().setValue(last_key, QJsonDocument(m_settings.toJson()).toJson(QJsonDocument::Compact));
	});
}

QDomDocument ConnectorDialog::drawing() const
{
	return ConnectorGenerator::build(m_settings);
}

QString ConnectorDialog::name() const
{
	return m_name->text().trimmed();
}

bool ConnectorDialog::replaceDrawing() const
{
	return m_replace->isChecked();
}

void ConnectorDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	m_preview->fitInView(m_preview_scene->sceneRect(), Qt::KeepAspectRatio);
}

	///The variants of the family chosen
void ConnectorDialog::fillVariants()
{
	const bool updating = m_updating;
	m_updating = true;
	m_variant->clear();
	const int family = m_family->currentData().toInt();
	for (const ConnectorGenerator::Variant &v : ConnectorGenerator::variants())
		if (v.family == family)
			m_variant->addItem(v.name, v.id);
	m_updating = updating;
}

	///Another connector: its presets, its own fields and signals
void ConnectorDialog::variantChanged()
{
	m_settings.variant = m_variant->currentData().toString();
	m_settings.preset = 0;
	const ConnectorGenerator::Variant *v = ConnectorGenerator::variant(m_settings.variant);
		//The FANUC connectors are drawn as in the FANUC diagrams
	const bool fanuc = v && v->family == ConnectorGenerator::Fanuc;
	m_updating = true;
	m_style->setCurrentIndex(fanuc ? ConnectorGenerator::FanucStyle : ConnectorGenerator::Box);
	m_used_only->setChecked(fanuc);
	m_updating = false;
	m_updating = true;
	m_preset->clear();
	if (v)
		m_preset->addItems(v->presets);
		//A standard has its signals: take them at once
	if (m_preset->count() > 1)
		m_preset->setCurrentIndex(1);
	m_updating = false;
	readSettings();
	m_settings.signal_names = ConnectorGenerator::presetSignals(m_settings);
	followTitle();
	showSettings();
}

	///The name of the connector from its preset, unless the user typed one
void ConnectorDialog::followTitle()
{
	const QStringList titles = ConnectorGenerator::presetTitles(m_settings);
	const QString title = titles.isEmpty() ? QString() : titles.first();
	if (m_settings.title.isEmpty() || m_settings.title == m_auto_title) {
		m_settings.title = title;
		const bool updating = m_updating;
		m_updating = true;
		m_title->setText(title);
		m_updating = updating;
	}
	m_auto_title = title;
}

/**
	@brief ConnectorDialog::showSettings
	Show m_settings in the fields, the table and the preview
*/
void ConnectorDialog::showSettings()
{
	m_updating = true;
	const ConnectorGenerator::Variant *v = ConnectorGenerator::variant(m_settings.variant);
	m_family->setCurrentIndex(m_family->findData(int(v ? v->family : ConnectorGenerator::Generic)));
	fillVariants();
	m_updating = true;
	m_variant->setCurrentIndex(qMax(0, m_variant->findData(m_settings.variant)));
	m_preset->clear();
	if (v)
		m_preset->addItems(v->presets);
	m_preset->setCurrentIndex(qBound(0, m_settings.preset, m_preset->count() - 1));
	m_preset->setEnabled(m_preset->count() > 1);
	m_gender->setCurrentIndex(m_settings.male ? 0 : 1);
	m_colours->setCurrentIndex(m_settings.colours);
	m_rows->setValue(m_settings.rows);
	m_columns->setValue(m_settings.columns);
	m_numbering->setCurrentIndex(m_settings.numbering);
	m_prefix->setText(m_settings.prefix);
	m_first->setValue(m_settings.first);
	m_symbol->setChecked(m_settings.symbol);
	m_front_view->setChecked(m_settings.front_view);
	m_face->setCurrentIndex(m_settings.solder_side ? 1 : 0);
	m_side->setCurrentIndex(m_settings.side);
	m_style->setCurrentIndex(m_settings.style);
	m_edge->setCurrentIndex(m_settings.top ? 0 : 1);
	m_used_only->setChecked(m_settings.used_only);
	m_title->setText(m_settings.title);
	m_updating = false;
	const bool fanuc_style = m_settings.style == ConnectorGenerator::FanucStyle;
	m_form->setRowVisible(m_side, !fanuc_style);
	m_form->setRowVisible(m_edge, fanuc_style);

	const bool rj45 = m_settings.variant == QLatin1String("rj45");
	m_form->setRowVisible(m_colours, rj45);
	m_form->setRowVisible(m_generic, m_settings.variant == QLatin1String("generic"));
	if (m_settings.signal_names.isEmpty())
		m_settings.signal_names = ConnectorGenerator::presetSignals(m_settings);
	fillSignals();
	updatePreview();
}

	///The fields into m_settings, but the signals
void ConnectorDialog::readSettings()
{
	m_settings.variant = m_variant->currentData().toString();
	m_settings.preset = qMax(0, m_preset->currentIndex());
	m_settings.male = m_gender->currentIndex() == 0;
	m_settings.colours = m_colours->currentIndex();
	m_settings.rows = m_rows->value();
	m_settings.columns = m_columns->value();
	m_settings.numbering = ConnectorGenerator::Numbering(m_numbering->currentIndex());
	m_settings.prefix = m_prefix->text().trimmed();
	m_settings.first = m_first->value();
	m_settings.symbol = m_symbol->isChecked();
	m_settings.front_view = m_front_view->isChecked();
	m_settings.solder_side = m_face->currentIndex() == 1;
	m_settings.side = ConnectorGenerator::PinSide(m_side->currentIndex());
	m_settings.style = ConnectorGenerator::Style(m_style->currentIndex());
	m_settings.top = m_edge->currentIndex() == 0;
	m_settings.used_only = m_used_only->isChecked();
	m_settings.title = m_title->text().trimmed();
}

	///One row per pin: its number, its signal to edit
void ConnectorDialog::fillSignals()
{
	m_updating = true;
	const QList<ConnectorGenerator::Pin> pins = ConnectorGenerator::pins(m_settings);
	m_table->setRowCount(pins.size());
	for (int i = 0; i < pins.size(); ++i) {
		auto number = new QTableWidgetItem(pins.at(i).id);
		number->setFlags(Qt::ItemIsEnabled);
		m_table->setItem(i, 0, number);
		m_table->setItem(i, 1, new QTableWidgetItem(m_settings.signal_names.value(i)));
	}
	m_table->resizeColumnToContents(0);
	m_updating = false;
}

void ConnectorDialog::updatePreview()
{
	const QString auto_name = ConnectorGenerator::defaultName(m_settings);
	if (m_name->text().isEmpty() || m_name->text() == m_auto_name)
		m_name->setText(auto_name);
	m_auto_name = auto_name;

	m_preview_scene->clear();
	ElementPreview::draw(m_preview_scene, drawing(), false);
	const QRectF rect = m_preview_scene->itemsBoundingRect().adjusted(-10, -10, 10, 10);
	m_preview_scene->setSceneRect(rect);
	m_preview->fitInView(rect, Qt::KeepAspectRatio);
}

void ConnectorDialog::fillTemplates()
{
	m_templates->clear();
	m_templates->addItem(tr("(nenhum)"));
	const QJsonObject object = templates();
	for (auto it = object.constBegin(); it != object.constEnd(); ++it)
		m_templates->addItem(it.key());
}

void ConnectorDialog::loadTemplate()
{
	if (m_templates->currentIndex() <= 0)
		return;
	const QString name = m_templates->currentText();
	m_settings = ConnectorGenerator::Settings::fromJson(templates().value(name).toObject());
	showSettings();
	m_name->setText(name);
}

void ConnectorDialog::saveTemplate()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Salvar modelo"), tr("Nome do modelo:"),
						   QLineEdit::Normal, m_name->text(), &ok).trimmed();
	if (!ok || name.isEmpty())
		return;
	QJsonObject object = templates();
	object.insert(name, m_settings.toJson());
	setTemplates(object);
	fillTemplates();
	m_templates->setCurrentText(name);
	m_name->setText(name);
}

void ConnectorDialog::deleteTemplate()
{
	if (m_templates->currentIndex() <= 0)
		return;
	QJsonObject object = templates();
	object.remove(m_templates->currentText());
	setTemplates(object);
	fillTemplates();
}
