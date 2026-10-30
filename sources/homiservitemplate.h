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
#ifndef HOMISERVITEMPLATE_H
#define HOMISERVITEMPLATE_H

#include <QDate>
#include <QString>
#include <QStringList>

class QETProject;

/**
	@brief The HomiserviTemplate namespace
	The Homiservi project template: the "Homiservi_Legenda" title block
	with the logo, the text styles and the wire colour and section rules,
	and the data of a project (client, place, drawing number...) written
	in the title block of every folio. createFolios() adds the usual
	folios of a new project: cover, table of contents, schematics and bill
	of materials.

	The data shared by every folio is kept as project properties (client,
	drawing number, checker; the title is the project title), the rest in
	the title block of each folio, as the title block template expects.
*/
namespace HomiserviTemplate
{
	extern const QString TitleBlockName;

	struct Data
	{
		QString title;		///< %{projecttitle}
		QString client;		///< %{cliente}
		QString plant;		///< %{plant}, Instalação
		QString location;	///< %{locmach}, Local
		QString drawing_number;	///< %{drwnumber}
		QString author;		///< %{author}
		QString checker;	///< %{verficado}, as the title block spells it
		QDate date;		///< %{date}
		QString revision;	///< %{indexrev}
		QString filename;	///< %{filename}
	};

	struct Folios
	{
		bool cover = true;
		bool contents = true;
		QStringList schematics;
		bool materials = true;
	};

	Data fromProject(QETProject *project);
	void prepare(QETProject *project, bool styles = true);
	void apply(QETProject *project, const Data &data, bool use_title_block = true);
	void createFolios(QETProject *project, const Data &data, const Folios &folios);
	bool hasCover(QETProject *project);
}

#endif // HOMISERVITEMPLATE_H
