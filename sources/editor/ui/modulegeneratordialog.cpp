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
#include "modulegeneratordialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace ModuleGenerator;

namespace {
	SidePins pins(int count, const QString &prefix, Numbering numbering = Numeric,
		      int start = 1, int step = 1)
	{
		SidePins side;
		side.count = count;
		side.prefix = prefix;
		side.numbering = numbering;
		side.start = start;
		side.step = step;
		return side;
	}

	struct Preset
	{
		const char *name;
		Settings settings;
	};

	QList<Preset> presets()
	{
		QList<Preset> list;
		Settings s;

		s = Settings();
		s.sides[Left] = pins(8, QStringLiteral("I0."), Numeric, 0);
		s.sides[Right] = pins(8, QStringLiteral("I1."), Numeric, 0);
		s.body_text = QStringLiteral("DI 16x24VCC");
		list << Preset{QT_TRANSLATE_NOOP("ModuleGeneratorDialog",
			"Cartão de entradas digitais 16 (I0.0 ... I1.7)"), s};

		s = Settings();
		s.sides[Left] = pins(16, QStringLiteral("I"), ByteBit, 0);
		s.body_text = QStringLiteral("DI 16x24VCC");
		list << Preset{QT_TRANSLATE_NOOP("ModuleGeneratorDialog",
			"Cartão de entradas digitais 16, um lado"), s};

		s = Settings();
		s.sides[Right] = pins(16, QStringLiteral("Q"), ByteBit, 0);
		s.body_text = QStringLiteral("DO 16x24VCC");
		list << Preset{QT_TRANSLATE_NOOP("ModuleGeneratorDialog",
			"Cartão de saídas digitais 16 (Q0.0 ... Q1.7)"), s};

		s = Settings();
		s.sides[Left] = pins(4, QStringLiteral("AI"), Numeric, 0);
		s.sides[Right] = pins(4, QStringLiteral("M"), Numeric, 0);
		s.body_text = QStringLiteral("AI 4x 4-20mA");
		list << Preset{QT_TRANSLATE_NOOP("ModuleGeneratorDialog",
			"Cartão de entradas analógicas 4 (AI0 ... AI3)"), s};

		s = Settings();
		s.sides[Bottom] = pins(10, QStringLiteral("X1:"));
		list << Preset{QT_TRANSLATE_NOOP("ModuleGeneratorDialog",
			"Conector 10 pinos (X1:1 ... X1:10)"), s};

		s = Settings();
		s.sides[Top] = pins(12, QString());
		s.sides[Bottom] = pins(12, QString());
		s.show_names = true;
		s.body_text = QStringLiteral("X1");
		list << Preset{QT_TRANSLATE_NOOP("ModuleGeneratorDialog",
			"Régua de bornes 12, entrada e saída (1 ... 12)"), s};
		return list;
	}

	QString sideName(int side)
	{
		switch (side) {
			case Left:   return ModuleGeneratorDialog::tr("Esquerda");
			case Right:  return ModuleGeneratorDialog::tr("Direita");
			case Top:    return ModuleGeneratorDialog::tr("Topo");
			case Bottom: return ModuleGeneratorDialog::tr("Base");
		}
		return QString();
	}
}

ModuleGeneratorDialog::ModuleGeneratorDialog(QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Gerar módulo ou conector"));
	auto layout = new QVBoxLayout(this);

	auto preset_layout = new QHBoxLayout();
	preset_layout->addWidget(new QLabel(tr("Modelo:"), this));
	auto preset = new QComboBox(this);
	preset->addItem(tr("(personalizado)"));
	for (const Preset &p : presets())
		preset->addItem(tr(p.name));
	preset_layout->addWidget(preset, 1);
	layout->addLayout(preset_layout);

	auto pins_box = new QGroupBox(tr("Pinos"), this);
	auto grid = new QGridLayout(pins_box);
	const QStringList headers = {tr("Lado"), tr("Quantidade"), tr("Prefixo"),
				     tr("Numeração"), tr("Início"), tr("Passo"), tr("Nomes")};
	for (int column = 0; column < headers.size(); ++column)
		grid->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(headers.at(column)), pins_box), 0, column);

	for (int side = 0; side < SideCount; ++side) {
		SideWidgets &w = m_sides[side];
		const int row = side + 1;
		grid->addWidget(new QLabel(sideName(side), pins_box), row, 0);
		w.count = new QSpinBox(pins_box);
		w.count->setRange(0, 256);
		grid->addWidget(w.count, row, 1);
		w.prefix = new QLineEdit(pins_box);
		w.prefix->setPlaceholderText(tr("ex.: I, X1:"));
		w.prefix->setMaximumWidth(90);
		grid->addWidget(w.prefix, row, 2);
		w.numbering = new QComboBox(pins_box);
		w.numbering->addItem(tr("1, 2, 3..."));
		w.numbering->addItem(tr("Byte.bit (0.0 ... 0.7, 1.0)"));
		grid->addWidget(w.numbering, row, 3);
		w.start = new QSpinBox(pins_box);
		w.start->setRange(0, 99999);
		w.start->setToolTip(tr("Primeiro número; em byte.bit, o primeiro byte"));
		grid->addWidget(w.start, row, 4);
		w.step = new QSpinBox(pins_box);
		w.step->setRange(1, 100);
		grid->addWidget(w.step, row, 5);
		w.preview = new QLabel(pins_box);
		w.preview->setMinimumWidth(150);
		grid->addWidget(w.preview, row, 6);

		connect(w.count, &QSpinBox::valueChanged, this, &ModuleGeneratorDialog::updatePreview);
		connect(w.prefix, &QLineEdit::textChanged, this, &ModuleGeneratorDialog::updatePreview);
		connect(w.numbering, &QComboBox::currentIndexChanged, this, &ModuleGeneratorDialog::updatePreview);
		connect(w.start, &QSpinBox::valueChanged, this, &ModuleGeneratorDialog::updatePreview);
		connect(w.step, &QSpinBox::valueChanged, this, &ModuleGeneratorDialog::updatePreview);
	}
	layout->addWidget(pins_box);

	auto form = new QFormLayout();
	m_pitch = new QSpinBox(this);
	m_pitch->setRange(10, 100);
	m_pitch->setSingleStep(10);
	m_pitch->setSuffix(tr(" px"));
	m_pitch->setToolTip(tr("Múltiplo de 10, para os bornes ficarem na grade dos fólios"));
	form->addRow(tr("Distância entre pinos:"), m_pitch);
	m_body_text = new QLineEdit(this);
	m_body_text->setPlaceholderText(tr("ex.: DI 16x24VCC (opcional)"));
	form->addRow(tr("Texto no corpo:"), m_body_text);
	m_show_names = new QCheckBox(tr("Escrever o nome de cada pino dentro do corpo"), this);
	form->addRow(QString(), m_show_names);
	m_label_field = new QCheckBox(tr("Adicionar o campo de identificação (label) acima do corpo"), this);
	form->addRow(QString(), m_label_field);
	layout->addLayout(form);

	m_summary = new QLabel(this);
	layout->addWidget(m_summary);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Gerar"));
	layout->addWidget(buttons);

	connect(m_pitch, &QSpinBox::valueChanged, this, &ModuleGeneratorDialog::updatePreview);
	connect(preset, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (index > 0) applyPreset(index - 1);
	});
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	setSettings(Settings());
}

Settings ModuleGeneratorDialog::settings() const
{
	Settings settings;
	for (int side = 0; side < SideCount; ++side) {
		const SideWidgets &w = m_sides[side];
		settings.sides[side] = pins(w.count->value(), w.prefix->text().trimmed(),
					    Numbering(w.numbering->currentIndex()),
					    w.start->value(), w.step->value());
	}
	settings.pitch = m_pitch->value() / 10 * 10;
	settings.show_names = m_show_names->isChecked();
	settings.body_text = m_body_text->text().trimmed();
	settings.label_field = m_label_field->isChecked();
	return settings;
}

void ModuleGeneratorDialog::setSettings(const Settings &settings)
{
	for (int side = 0; side < SideCount; ++side) {
		const SideWidgets &w = m_sides[side];
		const SidePins &p = settings.sides[side];
		w.count->setValue(p.count);
		w.prefix->setText(p.prefix);
		w.numbering->setCurrentIndex(p.numbering);
		w.start->setValue(p.start);
		w.step->setValue(p.step);
	}
	m_pitch->setValue(settings.pitch);
	m_show_names->setChecked(settings.show_names);
	m_body_text->setText(settings.body_text);
	m_label_field->setChecked(settings.label_field);
	updatePreview();
}

void ModuleGeneratorDialog::applyPreset(int index)
{
	const QList<Preset> list = presets();
	if (index >= 0 && index < list.size())
		setSettings(list.at(index).settings);
}

void ModuleGeneratorDialog::updatePreview()
{
	const Settings current = settings();
	int total = 0;
	for (int side = 0; side < SideCount; ++side) {
		const QStringList names = pinNames(current.sides[side]);
		total += names.size();
		QString text;
		if (names.size() == 1)
			text = names.first();
		else if (names.size() > 1)
			text = QStringLiteral("%1 ... %2").arg(names.first(), names.last());
		m_sides[side].preview->setText(text);
		m_sides[side].step->setEnabled(current.sides[side].numbering == Numeric);
	}
	m_summary->setText(tr("%n borne(s) no total. O módulo é colado no elemento já selecionado,"
			      " para mover com o mouse.", nullptr, total));
}
