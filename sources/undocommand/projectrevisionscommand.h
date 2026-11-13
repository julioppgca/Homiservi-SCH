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
#ifndef PROJECTREVISIONSCOMMAND_H
#define PROJECTREVISIONSCOMMAND_H

#include "../diagramcontext.h"
#include "../projectrevisions.h"
#include "../titleblockproperties.h"

#include <QList>
#include <QPointer>
#include <QUndoCommand>

class Diagram;
class QETProject;

/**
	@brief The ProjectRevisionsCommand class
	Homiservi-SCH: give a project its new revisions
	(ProjectRevisions::write()); undone, the project gets back its
	revisions, properties, default title block and the title block of
	every folio as they were.
*/
class ProjectRevisionsCommand : public QUndoCommand
{
	public:
		ProjectRevisionsCommand(QETProject *project, const QList<ProjectRevisions::Revision> &revisions,
					QUndoCommand *parent = nullptr);

		void undo() override;
		void redo() override;

	private:
		struct FolioTitleBlock
		{
			QPointer<Diagram> diagram;
			TitleBlockProperties properties;
		};

		QPointer<QETProject> m_project;
		QList<ProjectRevisions::Revision> m_new_revisions;
		QList<ProjectRevisions::Revision> m_old_revisions;
		DiagramContext m_old_properties;
		TitleBlockProperties m_old_defaults;
		QList<FolioTitleBlock> m_old_title_blocks;
};

#endif
