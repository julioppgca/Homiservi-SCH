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
#include <QHash>

class Diagram;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;
class QETProject;

/**
	@brief The ProjectRevisionsDialog class
	Edit the revisions of a project in a table, the newest at the top, and
	the folios each one changes. "Nova revisão" adds the next edition with
	today's date and the name of who draws (remembered by the Homiservi
	project wizard), the folios changed since the revision before and the
	current folio checked; the first revision of a project changes every
	folio.
*/
class ProjectRevisionsDialog : public QDialog
{
	Q_OBJECT

	public:
		ProjectRevisionsDialog(QETProject *project, Diagram *current, QWidget *parent = nullptr);

		QList<ProjectRevisions::Revision> revisions() const;

	private:
		void addRevision();
		void removeRevision();
		void insertRow(int row, const ProjectRevisions::Revision &revision, bool is_new = false);
		void showFolios(int row);
		void foliosChanged();
		void checkFolios(const QStringList &uuids);
		void updateFoliosText(int row);
		QStringList changedFolios() const;
		QString foliosText(const ProjectRevisions::Revision &revision) const;
		ProjectRevisions::Revision rowRevision(int row) const;

		QETProject *m_project;
		Diagram *m_current;
			///The hash of the content of every folio now, by uuid
		QHash<QString, QString> m_hashes;
		QTableWidget *m_table = nullptr;
		QCheckBox *m_general = nullptr;
		QListWidget *m_folios = nullptr;
		QPushButton *m_check_changed = nullptr;
		QPushButton *m_check_current = nullptr;
		QLabel *m_folios_label = nullptr;
		bool m_filling = false;
};

#endif // PROJECTREVISIONSDIALOG_H
