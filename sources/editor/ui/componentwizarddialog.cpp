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
#include "componentwizarddialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace ComponentWizard;

ComponentWizardDialog::ComponentWizardDialog(bool has_drawing, QWidget *parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Criar componente por tipo"));
	auto layout = new QVBoxLayout(this);
	auto form = new QFormLayout();

	m_kind = new QComboBox(this);
	for (int kind = 0; kind < KindCount; ++kind)
		m_kind->addItem(kindName(Kind(kind)));
	form->addRow(tr("Tipo:"), m_kind);

	m_poles = new QSpinBox(this);
	m_poles->setRange(1, 4);
	m_poles->setValue(3);
	form->addRow(tr("Polos:"), m_poles);

	m_earth = new QCheckBox(tr("Borne de terra (PE)"), this);
	m_earth->setChecked(true);
	form->addRow(QString(), m_earth);

	m_name = new QLineEdit(this);
	form->addRow(tr("Nome do elemento:"), m_name);

	m_prefix = new QLineEdit(this);
	m_prefix->setMaximumWidth(80);
	m_prefix->setToolTip(tr("Letra do identificador ao inserir o elemento (K, Q, S, H, M, G, X)."
				" Um escravo mostra o identificador do mestre."));
	form->addRow(tr("Prefixo do identificador:"), m_prefix);

	m_pin_names = new QCheckBox(tr("Escrever a marcação de cada borne (A1, 13, 1...)"), this);
	m_pin_names->setChecked(true);
	form->addRow(QString(), m_pin_names);

	m_label_field = new QCheckBox(tr("Adicionar o campo de identificação (label)"), this);
	m_label_field->setChecked(true);
	form->addRow(QString(), m_label_field);

	m_replace = new QCheckBox(tr("Substituir o desenho atual do elemento"), this);
	m_replace->setEnabled(has_drawing);
	m_replace->setToolTip(has_drawing
			      ? tr("Sem marcar, o componente é colado junto do desenho atual")
			      : tr("O elemento ainda está vazio"));
	form->addRow(QString(), m_replace);
	layout->addLayout(form);

	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Criar"));
	layout->addWidget(buttons);

	connect(m_kind, &QComboBox::currentIndexChanged, this, &ComponentWizardDialog::kindChanged);
	connect(m_poles, &QSpinBox::valueChanged, this, &ComponentWizardDialog::kindChanged);
	connect(m_earth, &QCheckBox::toggled, this, &ComponentWizardDialog::updateSummary);
	connect(m_name, &QLineEdit::textEdited, this, [this]() { m_name_edited = true; });
	connect(m_prefix, &QLineEdit::textEdited, this, [this]() { m_prefix_edited = true; });
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	kindChanged();
}

Settings ComponentWizardDialog::settings() const
{
	Settings settings;
	settings.kind = Kind(m_kind->currentIndex());
	settings.poles = m_poles->value();
	settings.earth = m_earth->isChecked();
	settings.pin_names = m_pin_names->isChecked();
	settings.label_field = m_label_field->isChecked();
	settings.prefix = m_prefix->text().trimmed();
	settings.name = m_name->text().trimmed();
	return settings;
}

bool ComponentWizardDialog::replaceDrawing() const
{
	return m_replace->isEnabled() && m_replace->isChecked();
}

void ComponentWizardDialog::kindChanged()
{
	const Kind kind = Kind(m_kind->currentIndex());
	m_poles->setEnabled(hasPoles(kind));
	m_earth->setEnabled(hasEarth(kind));
	const bool slave = kind == NoContact || kind == NcContact || kind == PowerContacts;
	m_prefix->setEnabled(!slave);
	if (!m_name_edited)
		m_name->setText(defaultName(kind, m_poles->value()));
	if (!m_prefix_edited)
		m_prefix->setText(defaultPrefix(kind));
	updateSummary();
}

void ComponentWizardDialog::updateSummary()
{
	const Settings current = settings();
	QStringList names = terminalNames(current);
	names.removeAll(QString());
	m_summary->setText(tr("Bornes: %1\nTipo do elemento: %2")
			   .arg(names.isEmpty() ? tr("sem nome") : names.join(QStringLiteral(", ")),
				typeDescription(current.kind)));
}
