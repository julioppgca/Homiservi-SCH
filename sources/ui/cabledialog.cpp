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
#include "cabledialog.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/qetshapeitem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QUndoCommand>

CableDialog::CableDialog(QetShapeItem *cable, QWidget *parent) :
	QDialog(parent),
	m_data(cable->cable())
{
	setWindowTitle(tr("Propriedades do cabo"));
	auto *form = new QFormLayout(this);

	m_name = new QLineEdit(m_data.name, this);
	m_code = new QLineEdit(m_data.code, this);
	m_code->setPlaceholderText(QStringLiteral("A02B-0124-K820"));
	m_formation = new QLineEdit(m_data.formation, this);
	m_formation->setPlaceholderText(QStringLiteral("10 × 2 × 0,18 mm²"));
	m_length = new QLineEdit(m_data.length, this);
	m_length->setPlaceholderText(QStringLiteral("1 m"));
	m_cores = new QSpinBox(this);
	m_cores->setRange(0, 999);
	m_cores->setSpecialValueText(tr("não informado"));
	m_cores->setValue(m_data.cores);
	m_shielded = new QCheckBox(tr("Blindado"), this);
	m_shielded->setChecked(m_data.shielded);
	m_shield_ground = new QComboBox(this);
	m_shield_ground->addItem(tr("Não aterrada"), int(CableData::GroundNone));
	m_shield_ground->addItem(tr("No início do cabo"), int(CableData::GroundStart));
	m_shield_ground->addItem(tr("No fim do cabo"), int(CableData::GroundEnd));
	m_shield_ground->addItem(tr("Nas duas pontas"), int(CableData::GroundBoth));
	m_shield_ground->setCurrentIndex(qMax(0, m_shield_ground->findData(m_data.shield_ground)));
	m_shield_ground->setEnabled(m_data.shielded);
	connect(m_shielded, &QCheckBox::toggled, m_shield_ground, &QWidget::setEnabled);

	form->addRow(tr("Nome"), m_name);
	form->addRow(tr("Código"), m_code);
	form->addRow(tr("Formação"), m_formation);
	form->addRow(tr("Comprimento"), m_length);
	form->addRow(tr("Vias"), m_cores);
	form->addRow(QString(), m_shielded);
	form->addRow(tr("Blindagem aterrada"), m_shield_ground);

		//The wires already in the cable, to check them against the cores
	QStringList wires;
	const QList<Conductor *> members = cable->cableMembers();
	for (Conductor *c : members)
		wires << (c->properties().text.isEmpty() ? QStringLiteral("?") : c->properties().text);
	wires.sort();
	auto *wire_label = new QLabel(this);
	wire_label->setWordWrap(true);
	wire_label->setText(members.isEmpty()
			? tr("Nenhum fio no cabo. Selecione os fios e o cabo e use \"Juntar ao cabo\".")
			: tr("%n fio(s): %1", nullptr, int(members.size())).arg(wires.join(QStringLiteral(", "))));
	form->addRow(tr("Fios"), wire_label);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, [this, cable]()
	{
		const QString name = m_name->text().trimmed();
		if (name.isEmpty())
		{
			QMessageBox::warning(this, windowTitle(), tr("Dê um nome ao cabo."));
			return;
		}
		QetShapeItem *other = QetShapeItem::findCable(cable->scene(), name);
		if (other && other != cable)
		{
			QMessageBox::warning(this, windowTitle(), tr("Já existe um cabo %1 neste folio.").arg(name));
			return;
		}
		accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	form->addRow(buttons);
}

CableData CableDialog::data() const
{
	CableData data = m_data;
	data.enabled = true;
	data.name = m_name->text().trimmed();
	data.code = m_code->text().trimmed();
	data.formation = m_formation->text().trimmed();
	data.length = m_length->text().trimmed();
	data.cores = m_cores->value();
	data.shielded = m_shielded->isChecked();
	data.shield_ground = m_shield_ground->currentData().toInt();
	return data;
}

void CableDialog::edit(QetShapeItem *cable, QWidget *parent)
{
	CableDialog dialog(cable, parent);
	if (dialog.exec() != QDialog::Accepted)
		return;
	const CableData old_data = cable->cable(), new_data = dialog.data();
	if (old_data == new_data || !cable->diagram())
		return;

	auto *undo = new QUndoCommand(tr("Modificar o cabo %1").arg(new_data.name));
	const QList<Conductor *> members = cable->cableMembers();
	new QPropertyUndoCommand(cable, "cableData", old_data.toMap(), new_data.toMap(), undo);
		//Renamed: its wires follow it
	if (new_data.name != old_data.name)
		setCableCommand(members, new_data.name, undo);
	cable->diagram()->undoStack().push(undo);
}

QUndoCommand *CableDialog::setCableCommand(const QList<Conductor *> &conductors, const QString &name,
					   QUndoCommand *parent)
{
	auto *undo = parent ? parent : new QUndoCommand(name.isEmpty() ? tr("Separar do cabo")
								      : tr("Juntar ao cabo %1").arg(name));
	for (Conductor *c : conductors)
	{
		const ConductorProperties old_properties = c->properties();
		if (old_properties.m_cable == name)
			continue;
		ConductorProperties new_properties = old_properties;
		new_properties.m_cable = name;
		new QPropertyUndoCommand(c, "properties", QVariant::fromValue(old_properties),
					 QVariant::fromValue(new_properties), undo);
	}
	return undo;
}
