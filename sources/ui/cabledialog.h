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
#ifndef CABLEDIALOG_H
#define CABLEDIALOG_H

#include "../qetgraphicsitem/cabledata.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QetShapeItem;
class Conductor;
class QUndoCommand;

/**
	@brief The CableDialog class
	Edits the data of a cable: its name, reference, formation, length,
	number of cores and shield. Renaming it carries its wires along.
*/
class CableDialog : public QDialog
{
		Q_OBJECT

	public:
		CableDialog(QetShapeItem *cable, QWidget *parent = nullptr);
		CableData data() const;

			///Opens the dialog for \a cable and applies it with an undo command
		static void edit(QetShapeItem *cable, QWidget *parent);

			///The undo command that puts \a conductors into the cable \a name,
			///or takes them out of any cable when \a name is empty
		static QUndoCommand *setCableCommand(const QList<Conductor *> &conductors, const QString &name,
						     QUndoCommand *parent = nullptr);

	private:
		CableData m_data;
		QLineEdit *m_name, *m_code, *m_formation, *m_length;
		QSpinBox *m_cores;
		QCheckBox *m_shielded, *m_reset_label;
		QComboBox *m_shield_ground;
};

#endif // CABLEDIALOG_H
