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
#ifndef CONNECTORDIALOG_H
#define CONNECTORDIALOG_H

#include "../connectorgenerator.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGraphicsScene;
class QGraphicsView;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QWidget;

/**
	@brief The ConnectorDialog class
	Choose a connector of ConnectorGenerator, its gender, what to draw and
	the signal of each pin (from a public standard or typed, or read from
	a template the user saved), while seeing it.
*/
class ConnectorDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit ConnectorDialog(bool has_drawing, QWidget *parent = nullptr);

		ConnectorGenerator::Settings settings() const {return m_settings;}
		QDomDocument drawing() const;
		QString name() const;
		bool replaceDrawing() const;

	protected:
		void showEvent(QShowEvent *event) override;

	private:
		void fillVariants();
		void variantChanged();
		void fillSignals();
		void showSettings();
		void readSettings();
		void updatePreview();
		void fillTemplates();
		void loadTemplate();
		void saveTemplate();
		void deleteTemplate();

		ConnectorGenerator::Settings m_settings;
		bool m_updating = false;
			///The name given by itself, followed until the user types one
		QString m_auto_name;
		QFormLayout *m_form = nullptr;
		QComboBox *m_templates = nullptr;
		QComboBox *m_family = nullptr;
		QComboBox *m_variant = nullptr;
		QComboBox *m_gender = nullptr;
		QComboBox *m_preset = nullptr;
		QComboBox *m_colours = nullptr;
		QCheckBox *m_symbol = nullptr;
		QCheckBox *m_front_view = nullptr;
		QComboBox *m_face = nullptr;
		QComboBox *m_side = nullptr;
		QWidget *m_generic = nullptr;
		QSpinBox *m_rows = nullptr;
		QSpinBox *m_columns = nullptr;
		QComboBox *m_numbering = nullptr;
		QLineEdit *m_prefix = nullptr;
		QSpinBox *m_first = nullptr;
		QTableWidget *m_table = nullptr;
		QLineEdit *m_name = nullptr;
		QCheckBox *m_replace = nullptr;
		QGraphicsScene *m_preview_scene = nullptr;
		QGraphicsView *m_preview = nullptr;
};

#endif // CONNECTORDIALOG_H
