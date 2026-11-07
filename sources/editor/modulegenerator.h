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
#ifndef MODULEGENERATOR_H
#define MODULEGENERATOR_H

#include <QDomDocument>
#include <QList>
#include <QString>
#include <QStringList>

/**
	@brief The ModuleGenerator namespace
	Draws a module or connector with many terminals in one go: a box, a
	numbered terminal with a short lead on each pin of each side, the pin
	names inside the box, an optional text in the middle and the label
	field. build() gives the parts as an element definition, ready to be
	pasted in the element editor.
*/
namespace ModuleGenerator
{
	enum Side { Left, Right, Top, Bottom, SideCount };
	enum Numbering {
		Numeric,	///< prefix + start, start + step... : X1:1, X1:2
		ByteBit		///< prefix + byte.bit from byte start: I0.0 ... I0.7, I1.0
	};

	struct SidePins
	{
		int count = 0;
		QString prefix;
		Numbering numbering = Numeric;
		int start = 1;
		int step = 1;
			///Names given one by one (a datasheet), used instead of the numbering
		QStringList names;
			///Place of each name along the side, from 0; empty: one after
			///the other. Pins of the top and bottom sides at the same
			///place face each other, as do left and right ones.
		QList<int> places;
	};

	enum SymbolKind { NormallyOpen, NormallyClosed, Coil, SymbolKindCount };

		///A contact or coil drawn inside the box, between the two pins at
		///one place: top to bottom when vertical, else left to right
	struct Symbol
	{
		SymbolKind kind = NormallyOpen;
		bool vertical = true;
		int place = 0;
		QString label;
	};

	struct Settings
	{
		SidePins sides[SideCount];
			///Distance between two pins, a multiple of 10 to stay on the grid
		int pitch = 20;
		bool show_names = true;
		QString body_text;
		bool label_field = true;
		QList<Symbol> symbols;
	};

	QStringList pinNames(const SidePins &pins);
	QDomDocument build(const Settings &settings);
}

#endif // MODULEGENERATOR_H
