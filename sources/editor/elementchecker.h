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
#ifndef ELEMENTCHECKER_H
#define ELEMENTCHECKER_H

#include <QList>
#include <QString>

class ElementScene;
class QGraphicsItem;
class QUndoCommand;

/**
	@brief The ElementChecker namespace
	Checks the element being edited for what makes it awkward in a
	diagram: terminals off the diagrams' grid, on top of each other,
	facing into the drawing or touching nothing, repeated or missing
	terminal names, no field showing the label, texts over a terminal,
	parts too small to be seen. Some can be fixed by fix().
*/
namespace ElementChecker
{
	enum Severity {Error, Warning, Hint};

	enum Fix {
		NoFix,
		TerminalsOnGrid,	///< put the terminals on the diagrams' grid
		TurnTerminals,		///< turn the terminals to face out of the drawing
		AddLabelField,		///< add a field showing the label of the element
		RemoveParts		///< remove the parts
	};

	struct Issue
	{
			///What is checked: stacked, names, off_grid, facing_in, loose,
			///unnamed, label, text_over or useless
		QString id;
		Severity severity = Warning;
		QString title;
		QString detail;
			///The parts concerned, selected when the issue is chosen
		QList<QGraphicsItem *> items;
		Fix fix = NoFix;
	};

	QList<Issue> check(ElementScene *scene);
	QString fixName(Fix fix);
	QUndoCommand *fix(ElementScene *scene, const Issue &issue);
}

#endif // ELEMENTCHECKER_H
