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
#ifndef PROJECTDIFF_H
#define PROJECTDIFF_H

#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QUuid>

class Conductor;
class QETProject;

/**
	@brief The ProjectDiff namespace
	Compares two versions of a project, folio by folio: the folios, the
	elements and the conductors added, removed or changed.
	Folios are matched by uuid (by title for files saved before folios
	had one), elements by uuid, conductors by the two terminals they join,
	since a conductor's own uuid is not kept by every version.
*/
namespace ProjectDiff
{
	struct Change
	{
		enum Kind {Added, Removed, Modified};
		enum Item {Folio, Element, Conductor};

		Kind kind = Modified;
		Item item = Element;
			/// Folio the item is on: in the new version, or in the old
			/// one for what was removed
		QUuid folio_uuid;
		int folio_index = -1;
		QString folio_title;
		QString name;		///< what changed, e.g. "-K1 (Contator)"
		QStringList details;	///< e.g. "Rótulo: -K1 → -K2"
		QPointF pos;		///< scene position of the item
		QString cell;		///< folio cell of the item, e.g. "3B"
			/// To find the item again in the new version
		QUuid element_uuid;
		QString conductor_key;
	};

	QList<Change> compare(QETProject *before, QETProject *after);
	QString conductorKey(Conductor *conductor);
}

#endif // PROJECTDIFF_H
