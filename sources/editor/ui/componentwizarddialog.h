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
#ifndef COMPONENTWIZARDDIALOG_H
#define COMPONENTWIZARDDIALOG_H

#include "../componentwizard.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

/**
	@brief The ComponentWizardDialog class
	Ask which usual component to draw (ComponentWizard) and how: number of
	poles, earth terminal, name, prefix of the label, and whether it
	replaces the drawing already in the editor.
*/
class ComponentWizardDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit ComponentWizardDialog(bool has_drawing, QWidget *parent = nullptr);
		ComponentWizard::Settings settings() const;
		bool replaceDrawing() const;

	private:
		void kindChanged();
		void updateSummary();

		QComboBox *m_kind = nullptr;
		QSpinBox *m_poles = nullptr;
		QCheckBox *m_earth = nullptr;
		QLineEdit *m_name = nullptr;
		QLineEdit *m_prefix = nullptr;
		QCheckBox *m_pin_names = nullptr;
		QCheckBox *m_label_field = nullptr;
		QCheckBox *m_replace = nullptr;
		QLabel *m_summary = nullptr;
			///The name and the prefix follow the kind until the user types one
		bool m_name_edited = false;
		bool m_prefix_edited = false;
};

#endif // COMPONENTWIZARDDIALOG_H
