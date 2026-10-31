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
#ifndef PROJECTVARIANTS_H
#define PROJECTVARIANTS_H

#include <QList>
#include <QString>

class Diagram;
class Element;
class QUndoCommand;

/**
	@brief The ProjectVariants namespace
	What a component or a folio is for the machines a project describes:
	X standard, O optional, V variant, E special. It is kept as the
	"variant" information of the element and the "variant" variable of
	the title block of the folio, so that the component list and the table
	of contents show it; nothing marked counts as standard. The machine or
	configuration a component belongs to is its "application"
	information.
*/
namespace ProjectVariants
{
	struct Kind
	{
		QString code;	///< X, O, V, E
		QString name;
	};

	extern const QString Key;
	extern const QString ApplicationKey;
	extern const QString Standard;

	QList<Kind> kinds();
	QString legend();
	QUndoCommand *elementsCommand(const QList<Element *> &elements, const QString &code);
	QUndoCommand *folioCommand(Diagram *diagram, const QString &code);
}

#endif // PROJECTVARIANTS_H
