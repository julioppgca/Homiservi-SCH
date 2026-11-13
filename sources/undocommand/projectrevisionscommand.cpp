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
#include "projectrevisionscommand.h"

#include "../dataBase/projectdatabase.h"
#include "../diagram.h"
#include "../qetproject.h"

ProjectRevisionsCommand::ProjectRevisionsCommand(QETProject *project,
						 const QList<ProjectRevisions::Revision> &revisions,
						 QUndoCommand *parent) :
	QUndoCommand(QObject::tr("Revisões do projeto"), parent),
	m_project(project),
	m_new_revisions(revisions)
{
	m_old_revisions = project->revisions();
	m_old_properties = project->projectProperties();
	m_old_defaults = project->defaultTitleBlockProperties();
	for (Diagram *diagram : project->diagrams())
		m_old_title_blocks << FolioTitleBlock {diagram, diagram->border_and_titleblock.exportTitleBlock()};
}

void ProjectRevisionsCommand::redo()
{
	if (m_project)
		ProjectRevisions::write(m_project, m_new_revisions);
}

void ProjectRevisionsCommand::undo()
{
	if (!m_project) return;
	m_project->setRevisions(m_old_revisions);
	m_project->setDefaultTitleBlockProperties(m_old_defaults);
	for (const FolioTitleBlock &folio : std::as_const(m_old_title_blocks))
		if (folio.diagram)
			folio.diagram->border_and_titleblock.importTitleBlock(folio.properties);
	m_project->setProjectProperties(m_old_properties);
	m_project->setModified(true);
	m_project->dataBase()->diagramsInfoChanged();
}
