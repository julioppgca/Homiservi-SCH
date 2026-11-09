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
#include "projectnumberingcommand.h"

#include "../diagram.h"
#include "../qetproject.h"

ProjectNumberingCommand::ProjectNumberingCommand(QETProject *project,
						 const ProjectNumbering::Settings &old_settings,
						 const ProjectNumbering::Settings &new_settings,
						 QUndoCommand *parent) :
	QUndoCommand(QObject::tr("Numeração das folhas"), parent),
	m_project(project),
	m_old_settings(old_settings),
	m_new_settings(new_settings)
{}

/**
	@brief ProjectNumberingCommand::addTitleBlock
	The title block of diagram changes too, from old_properties to
	new_properties
*/
void ProjectNumberingCommand::addTitleBlock(Diagram *diagram,
					    const TitleBlockProperties &old_properties,
					    const TitleBlockProperties &new_properties)
{
	m_title_blocks << TitleBlockChange {diagram, old_properties, new_properties};
}

/**
	@brief ProjectNumberingCommand::setReferences
	The cross references and folio reports change too, from old_xrefs and
	old_report to new_xrefs and new_report
*/
void ProjectNumberingCommand::setReferences(const QHash<QString, XRefProperties> &old_xrefs,
					    const QHash<QString, XRefProperties> &new_xrefs,
					    const QString &old_report, const QString &new_report)
{
	m_references = true;
	m_old_xrefs = old_xrefs;
	m_new_xrefs = new_xrefs;
	m_old_report = old_report;
	m_new_report = new_report;
}

void ProjectNumberingCommand::undo()
{
	if (!m_project) return;
	if (m_references) {
		m_project->setDefaultXRefProperties(m_old_xrefs);
		m_project->setDefaultReportProperties(m_old_report);
	}
		//First: a title block given back asks for the folio data, which
		//numbers the folios by the settings of the project
	m_project->setNumbering(m_old_settings);
	for (const TitleBlockChange &change : std::as_const(m_title_blocks))
		if (change.diagram)
			change.diagram->border_and_titleblock.importTitleBlock(change.old_properties);
	m_project->setModified(true);
}

void ProjectNumberingCommand::redo()
{
	if (!m_project) return;
	m_project->setNumbering(m_new_settings);
	for (const TitleBlockChange &change : std::as_const(m_title_blocks))
		if (change.diagram)
			change.diagram->border_and_titleblock.importTitleBlock(change.new_properties);
	if (m_references) {
		m_project->setDefaultXRefProperties(m_new_xrefs);
		m_project->setDefaultReportProperties(m_new_report);
	}
	m_project->setModified(true);
}
