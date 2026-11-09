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
#ifndef PROJECTNUMBERINGCOMMAND_H
#define PROJECTNUMBERINGCOMMAND_H

#include "../projectnumbering.h"
#include "../properties/xrefproperties.h"
#include "../titleblockproperties.h"

#include <QHash>

#include <QList>
#include <QPointer>
#include <QUndoCommand>

class Diagram;
class QETProject;

/**
	@brief The ProjectNumberingCommand class
	Homiservi-SCH: change the sections and the folio numbering of a project
	(ProjectNumbering) and the title blocks of its folios (their number and
	the title without it) as one step. Undone, every folio gets its title
	block back, even its number typed by hand.
*/
class ProjectNumberingCommand : public QUndoCommand
{
	public:
		ProjectNumberingCommand(QETProject *project,
					const ProjectNumbering::Settings &old_settings,
					const ProjectNumbering::Settings &new_settings,
					QUndoCommand *parent = nullptr);

		void addTitleBlock(Diagram *diagram, const TitleBlockProperties &old_properties,
				   const TitleBlockProperties &new_properties);
		int titleBlockCount() const {return int(m_title_blocks.size());}
		void setReferences(const QHash<QString, XRefProperties> &old_xrefs,
				   const QHash<QString, XRefProperties> &new_xrefs,
				   const QString &old_report, const QString &new_report);

		void undo() override;
		void redo() override;

	private:
		struct TitleBlockChange
		{
			QPointer<Diagram> diagram;
			TitleBlockProperties old_properties;
			TitleBlockProperties new_properties;
		};

		QPointer<QETProject> m_project;
		ProjectNumbering::Settings m_old_settings;
		ProjectNumbering::Settings m_new_settings;
		QList<TitleBlockChange> m_title_blocks;
		bool m_references = false;
		QHash<QString, XRefProperties> m_old_xrefs, m_new_xrefs;
		QString m_old_report, m_new_report;
};

#endif
