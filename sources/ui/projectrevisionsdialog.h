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
#ifndef PROJECTREVISIONSDIALOG_H
#define PROJECTREVISIONSDIALOG_H

#include "../projectrevisions.h"

#include <QDialog>

class QTableWidget;

/**
	@brief The ProjectRevisionsDialog class
	Edit the revisions of a project in a table, the newest at the top.
	"Nova revisão" adds the next edition with today's date and the name
	of who draws, remembered by the Homiservi project wizard.
*/
class ProjectRevisionsDialog : public QDialog
{
	Q_OBJECT

	public:
		ProjectRevisionsDialog(const QList<ProjectRevisions::Revision> &revisions,
				       QWidget *parent = nullptr);

		QList<ProjectRevisions::Revision> revisions() const;

	private:
		void addRevision();
		void removeRevision();
		void insertRow(int row, const ProjectRevisions::Revision &revision);

		QTableWidget *m_table = nullptr;
};

#endif // PROJECTREVISIONSDIALOG_H
