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
#include <QStringList>

class Element;
class QETProject;

/**
	@brief The ElementLibraryUpdate namespace
	A project keeps its own copy of every symbol it uses (the embedded
	collection). The placed elements are built from that copy when the
	project opens, and were not rebuilt when it changed afterwards:

	- the symbol edited in a library: the copy of the project stayed old;
	- the copy of the project edited in the symbol editor ("Edit the
	  element"): the placed elements kept the old drawing.

	This brings the placed elements up to date. The version each symbol
	had when its elements were built is remembered (watch()), so the
	dynamic texts the user left where the symbol put them can follow the
	new version, and the ones the user moved stay.
*/
namespace ElementLibraryUpdate
{
		///Remember the symbols of project as its elements were built,
		///and rebuild the elements whenever the copy of a symbol changes
		///(the symbol editor saving it). Called once the project is loaded.
	void watch(QETProject *project);

		///The symbol of a library the embedded symbol embedded came
		///from: same path in the user, company or common collection,
		///the most recently saved one when it is in several. Invalid when
		///there is none (a symbol made for the project).
	ElementsLocation librarySource(const ElementsLocation &embedded);

		///The symbols (in the project) of elements, each once
	QList<ElementsLocation> symbolsOf(const QList<Element *> &elements);

	struct Result {
		int symbols_updated = 0;
		int elements_redrawn = 0;
		int texts_changed = 0;
		QStringList from_library;
		QStringList already_up_to_date;
			///elements that took a new size, hotspot or terminals
		int geometry_applied = 0;
			///symbols left as they were: a terminal with a wire is gone
			///from the new version (element, folio and terminal)
		QStringList geometry_changed;
	};

		///Bring the elements of symbols up to date: with the library when
		///it has a newer version of the symbol, else with the copy of the
		///project when it changed since the elements were built
	Result update(QETProject *project, const QList<ElementsLocation> &symbols);
}

#endif
