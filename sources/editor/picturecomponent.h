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
#ifndef PICTURECOMPONENT_H
#define PICTURECOMPONENT_H

#include "modulegenerator.h"

#include <QByteArray>
#include <QDomDocument>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QSizeF>
#include <QString>

/**
	@brief The PictureComponent namespace
	Makes a component from a picture of it: the connection diagram of a
	relay, a module or a contactor, as printed on the product or in its
	datasheet. The terminals are read from the picture, by the Claude API
	or by the user clicking on each one; the contacts and coils between
	two terminals are guessed from their names (13-14 normally open,
	11-12 normally closed, A1-A2 coil). build() draws the box with the
	terminals where the picture has them, the contacts drawn between
	their terminals if asked.
*/
namespace PictureComponent
{
	struct Pin
	{
		QString name;
		ModuleGenerator::Side side = ModuleGenerator::Top;
			///Column (top, bottom) or row (left, right), from 0. A bottom
			///terminal right under a top one has the same position.
		int position = 0;
		QString function;
			///Where the user clicked it on the picture, null when read by the AI
		QPointF point;
	};

	struct Part
	{
		ModuleGenerator::SymbolKind kind = ModuleGenerator::NormallyOpen;
		QString pin_a;
		QString pin_b;
			///Device the part belongs to, e.g. K1
		QString label;
	};

	struct Result
	{
		QString name;
		QMap<QString, QString> info;
		QList<Pin> pins;
		QList<Part> parts;
			///Text for the part of the box without contacts (the logic)
		QString body_text;
	};

	QString kindName(ModuleGenerator::SymbolKind kind);
	QList<Part> guessParts(const QList<Pin> &pins);
	void placeClickedPins(QList<Pin> &pins, const QSizeF &picture);
	bool canDraw(const Part &part, const QList<Pin> &pins);

	QByteArray aiRequestBody(const QByteArray &image, const QString &media_type);
	Result fromAiReply(const QByteArray &reply, QString *error);

	QDomDocument build(const Result &result, bool draw_parts);
}

#endif // PICTURECOMPONENT_H
