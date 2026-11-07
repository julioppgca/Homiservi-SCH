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
#ifndef MODULEGENERATORDIALOG_H
#define MODULEGENERATORDIALOG_H

#include "../modulegenerator.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

/**
	@brief The ModuleGeneratorDialog class
	Ask how many pins each side of a module or connector has and how they
	are named (ModuleGenerator), with ready-made settings for the usual
	PLC cards and connectors.
*/
class ModuleGeneratorDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit ModuleGeneratorDialog(QWidget *parent = nullptr);
		ModuleGenerator::Settings settings() const;

	private:
		struct SideWidgets
		{
			QSpinBox *count = nullptr;
			QLineEdit *prefix = nullptr;
			QComboBox *numbering = nullptr;
			QSpinBox *start = nullptr;
			QSpinBox *step = nullptr;
			QLabel *preview = nullptr;
		};

		void setSettings(const ModuleGenerator::Settings &settings);
		void applyPreset(int index);
		void updatePreview();

		SideWidgets m_sides[ModuleGenerator::SideCount];
		QSpinBox *m_pitch = nullptr;
		QCheckBox *m_show_names = nullptr;
		QLineEdit *m_body_text = nullptr;
		QCheckBox *m_label_field = nullptr;
		QLabel *m_summary = nullptr;
};

#endif // MODULEGENERATORDIALOG_H
