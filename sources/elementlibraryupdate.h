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
#ifndef ELEMENTLIBRARYUPDATE_H
#define ELEMENTLIBRARYUPDATE_H

#include "ElementsCollection/elementslocation.h"

#include <QList>
#include <QString>

class Element;
class QETProject;

/**
	@brief The ElementLibraryUpdate namespace
	A project keeps its own copy of every symbol it uses (the embedded
	collection, "embed://import/..."). Editing the symbol in a library
	afterwards does not change that copy, so the placed elements keep the
	old drawing. This brings a symbol of the project up to date with the
	library it came from: the copy of the project is replaced, every
	placed element of that symbol is redrawn, and the dynamic texts the
	user left as the symbol had them follow the new symbol.
*/
namespace ElementLibraryUpdate
{
		///The symbol of a library the embedded symbol embedded came
		///from: same path in the user, company or common collection,
		///the most recently saved one when it is in several. Invalid when
		///there is none (a symbol made in the project).
	ElementsLocation librarySource(const ElementsLocation &embedded);

		///The embedded symbols of elements that have a library source
	QList<ElementsLocation> updatableSymbols(const QList<Element *> &elements);

	struct Result {
		int symbols_updated = 0;
		int elements_redrawn = 0;
		int texts_changed = 0;
		QStringList already_up_to_date;
			///symbols left as they were: their size, hotspot or
			///terminals changed, with the folios of their elements
		QStringList geometry_changed;
	};

		///Bring the embedded symbols up to date with their library source
	Result update(QETProject *project, const QList<ElementsLocation> &symbols);
}

#endif
