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
#ifndef IOADDRESS_H
#define IOADDRESS_H

#include "diagramcontext.h"

#include <QString>

class Element;
class QETProject;

/**
	@brief The IoAddress namespace
	The addresses of an I/O module given from its base, the way the FANUC
	manuals write them: Xm+0.0 is bit 0 of the byte m of the inputs, Yn+1.7
	bit 7 of the byte n+1 of the outputs. A text of an element written
	%{Xm+0.0} shows X12.0 once the base m of the module is given as 12, and
	Xm+0.0 until then.

	An element without a base of its own takes it:
	- through a flat cable: a terminal of it wired to the terminal of the
	  same name of an element with a base (the CB104 of a terminal
	  adapter to the CB104 of the I/O unit), across folio reports too;
	- from the element with the same label that has a base (the symbols
	  of the connectors of a module, drawn on other folios).
*/
namespace IoAddress
{
	bool contains(const QString &text);

		///\a text with its addresses replaced, from the io_x_base and
		///io_y_base of \a dc
	QString replace(const QString &text, const DiagramContext &dc);

		///The information of \a element, with the base it takes from another
		///element when it has none of its own
	DiagramContext context(const Element *element);

		///Finds again where every element of \a project takes its base from
		///and updates its addresses
	void refresh(QETProject *project);

		///refresh() once the current event is handled
	void scheduleRefresh(QETProject *project);
}

#endif // IOADDRESS_H
