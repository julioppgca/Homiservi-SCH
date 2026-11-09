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
#ifndef POTENTIALRULESDIALOG_H
#define POTENTIALRULESDIALOG_H

#include "../potentialrules.h"
#include "../wiringrules.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QTableWidget;

/**
	@brief The PotentialRulesDialog class
	Edit the wiring rules of a project: what the end labels of the wires
	say, how many wires a terminal takes (WiringRules), and the colour and
	section rules. Besides OK, the
	dialog can be closed asking to apply the rules to every wire, or to
	insert their legend on the current folio: see action().
*/
class PotentialRulesDialog : public QDialog
{
	Q_OBJECT

	public:
		enum Action { NoAction, ApplyAll, InsertLegend };

		explicit PotentialRulesDialog(const PotentialRules::Settings &settings,
					      QWidget *parent = nullptr);

		PotentialRules::Settings settings() const;
			///The wires per terminal of the project (Settings::own false:
			///the application's)
		void setWiringRules(const WiringRules::Settings &rules);
		WiringRules::Settings wiringRules() const;
		Action action() const {return m_action;}

	private:
		void fill(const PotentialRules::Settings &settings);
		void addRow(const PotentialRules::Rule &rule);
		void moveCurrentRow(int offset);
		void updateSwatch(int row);
		void closeWith(Action action);

		QCheckBox *m_automatic = nullptr;
		QComboBox *m_end_labels = nullptr;
		QCheckBox *m_own_wiring = nullptr;
		QSpinBox *m_max_wires = nullptr;
		QCheckBox *m_one_per_report = nullptr;
		QTableWidget *m_table = nullptr;
		Action m_action = NoAction;
};

#endif // POTENTIALRULESDIALOG_H
