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
#ifndef ELEMENTCHECKDIALOG_H
#define ELEMENTCHECKDIALOG_H

#include "../elementchecker.h"

#include <QDialog>
#include <QPointer>

class ElementScene;
class QLabel;
class QPushButton;
class QTreeWidget;

/**
	@brief The ElementCheckDialog class
	Lists what ElementChecker finds wrong with the element being edited,
	kept up to date while it is edited. Choosing an issue selects its
	parts; the issues with a fix are fixed one by one or all at once,
	each in the undo history.
*/
class ElementCheckDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit ElementCheckDialog(ElementScene *scene, QWidget *parent = nullptr);

		void refresh();

	private:
		void showCurrent();
		void fixCurrent();
		void fixAll();

		QPointer<ElementScene> m_scene;
		QList<ElementChecker::Issue> m_issues;
		QTreeWidget *m_list = nullptr;
		QLabel *m_summary = nullptr;
		QPushButton *m_fix = nullptr;
		QPushButton *m_fix_all = nullptr;
};

#endif // ELEMENTCHECKDIALOG_H
