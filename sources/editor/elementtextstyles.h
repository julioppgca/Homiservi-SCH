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
#ifndef ELEMENTTEXTSTYLES_H
#define ELEMENTTEXTSTYLES_H

#include "../textstyles.h"

#include <QHash>
#include <QList>

class QGraphicsItem;
class QGraphicsObject;
class QUndoCommand;

/**
	@brief The ElementTextStyles namespace
	Text styles of the element editor (font and colour, like the text
	styles of a project): the name of a terminal, the text of the box, the
	label... Kept in the settings, so every element drawn uses the same.
	A text of an element (PartText, PartDynamicTextField) names the style
	it was given (element_style attribute); changing a style changes the
	texts naming it in the element being edited.
*/
namespace ElementTextStyles
{
	QList<TextStyles::Style> examples();
	QList<TextStyles::Style> load();
	void save(const QList<TextStyles::Style> &styles);

	QList<QGraphicsObject *> texts(const QList<QGraphicsItem *> &items);
	QUndoCommand *apply(const TextStyles::Style &style, const QList<QGraphicsObject *> &texts);
	QUndoCommand *update(const QList<TextStyles::Style> &styles,
			     const QHash<QString, QString> &renames,
			     const QList<QGraphicsObject *> &texts);
}

#endif // ELEMENTTEXTSTYLES_H
