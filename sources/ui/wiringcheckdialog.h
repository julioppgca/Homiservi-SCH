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
#ifndef WIRINGCHECKDIALOG_H
#define WIRINGCHECKDIALOG_H

#include "../wiringcheck.h"

#include <QDialog>
#include <QPointer>

class QCheckBox;
class QETProject;
class QLabel;
class QTableWidget;

/**
	@brief The WiringCheckDialog class
	Homiservi-SCH: the issues of the wires of a project (WiringCheck), in
	a window that stays open beside the drawing: a double click shows the
	wire on its folio, "Verificar de novo" checks again after the fixes.
*/
class WiringCheckDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit WiringCheckDialog(QETProject *project, QWidget *parent = nullptr);
		void setProject(QETProject *project);
		void refresh();
		int issueCount() const {return int(m_issues.size());}

	private:
		void fill();
		void showIssue(int row);

		QPointer<QETProject> m_project;
		QList<WiringCheck::Issue> m_issues;
		QLabel *m_summary = nullptr;
		QTableWidget *m_table = nullptr;
		QCheckBox *m_show[3] = {nullptr, nullptr, nullptr};
};

#endif // WIRINGCHECKDIALOG_H
