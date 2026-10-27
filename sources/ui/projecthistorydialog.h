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
#ifndef PROJECTHISTORYDIALOG_H
#define PROJECTHISTORYDIALOG_H

#include "../projecthistory.h"

#include <QDialog>

class QPushButton;
class QTreeWidget;

/**
	@brief The ProjectHistoryDialog class
	Lists the versions ProjectHistory kept of a project and lets the user
	pick one to open as a copy or to restore. The dialog only reports the
	choice: the caller closes and reopens projects.
*/
class ProjectHistoryDialog : public QDialog
{
	Q_OBJECT

	public:
		enum Action {
			NoAction,
			OpenCopy,	///< open the version beside the project, read-only
			Restore		///< put the version in place of the project file
		};

		explicit ProjectHistoryDialog(const QString &project_path,
					      QWidget *parent = nullptr);

		Action action() const {return m_action;}
		ProjectHistory::Entry selectedEntry() const;

	private:
		void updateButtons();
		void choose(Action action);

		QString m_project_path;
		QList<ProjectHistory::Entry> m_entries;
		Action m_action = NoAction;
		QTreeWidget *m_list = nullptr;
		QPushButton *m_open_copy = nullptr;
		QPushButton *m_restore = nullptr;
};

#endif // PROJECTHISTORYDIALOG_H
