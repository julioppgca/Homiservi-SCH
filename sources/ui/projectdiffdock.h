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
#ifndef PROJECTDIFFDOCK_H
#define PROJECTDIFFDOCK_H

#include "../projectdiff.h"

#include <QDockWidget>
#include <QPointer>

class Diagram;
class Element;
class QETProject;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

/**
	@brief The ProjectDiffDock class
	Shows what changed between an open project and another version of it
	(a file, or a version of its history), folio by folio. Clicking a
	change shows the folio and the item in the open project; for what was
	removed, the place it was.
*/
class ProjectDiffDock : public QDockWidget
{
	Q_OBJECT

	public:
		explicit ProjectDiffDock(QWidget *parent = nullptr);

		bool compare(QETProject *project, const QString &reference_path,
			     const QString &reference_label);

	private:
		void refresh();
		void fillTree();
		void showChange(QTreeWidgetItem *item);
		Diagram *folio(const QUuid &uuid) const;
		void setHighlighted(Element *element);

		QPointer<QETProject> m_project;
		QString m_reference_path;
		QString m_reference_label;
		QList<ProjectDiff::Change> m_changes;
		QPointer<Element> m_highlighted;
		QLabel *m_summary = nullptr;
		QTreeWidget *m_tree = nullptr;
};

#endif // PROJECTDIFFDOCK_H
