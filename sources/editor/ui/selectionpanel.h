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
#ifndef SELECTIONPANEL_H
#define SELECTIONPANEL_H

#include <QPointer>
#include <QWidget>

class ElementScene;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QSpinBox;

/**
	@brief The SelectionPanel class
	What the element editor shows for a selection of parts of several
	kinds: how many of each, where the selection is and how big (moved
	as a whole by typing its position), buttons keeping only the parts
	of one kind (whose own editor then edits them all), and the fields
	shared by the terminals and the texts among them.
*/
class SelectionPanel : public QWidget
{
	Q_OBJECT

	public:
		explicit SelectionPanel(ElementScene *scene, QWidget *parent = nullptr);

		void refresh();

	private:
		void moveTo();
		void keepOnly(int type);
		void setOrientation(int index);
		void setTextSize(int size);

		QPointer<ElementScene> m_scene;
		QLabel *m_summary = nullptr;
		QDoubleSpinBox *m_x = nullptr;
		QDoubleSpinBox *m_y = nullptr;
		QLabel *m_size = nullptr;
		QWidget *m_filters = nullptr;
		QGridLayout *m_filters_layout = nullptr;
		QLabel *m_orientation_label = nullptr;
		QComboBox *m_orientation = nullptr;
		QLabel *m_text_size_label = nullptr;
		QSpinBox *m_text_size = nullptr;
};

#endif // SELECTIONPANEL_H
