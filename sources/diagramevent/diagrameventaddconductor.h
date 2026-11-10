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
#ifndef DIAGRAMEVENTADDCONDUCTOR_H
#define DIAGRAMEVENTADDCONDUCTOR_H

#include "diagrameventinterface.h"

#include <QList>
#include <QPointF>
#include <QPointer>

class QGraphicsEllipseItem;
class QGraphicsPathItem;
class Terminal;

/**
	@brief The DiagramEventAddConductor class
	Homiservi-SCH: draw a wire by clicks, without holding the mouse
	button. A click on a terminal starts the wire, which then follows the
	mouse; each click on the folio fixes a bend; a click on another
	terminal ends the wire. The path between two clicks is an L of a
	horizontal and a vertical segment: Space turns the L the other way.
	Right click or Backspace takes back the last bend, Escape drops the
	wire.

	The wire is made by Terminal::connectTo(), as one undo step, with the
	path through the bends (none placed: the path the application
	draws).

	Started by the "Desenhar fio" tool (W), it stays for the next wire
	until Escape; started by a click on a terminal (one_shot), it ends
	with the wire.
*/
class DiagramEventAddConductor : public DiagramEventInterface
{
	Q_OBJECT

	public:
		DiagramEventAddConductor(Diagram *diagram, Terminal *start = nullptr,
					 bool one_shot = false);
		~DiagramEventAddConductor() override;

		void mousePressEvent       (QGraphicsSceneMouseEvent *event) override;
		void mouseMoveEvent        (QGraphicsSceneMouseEvent *event) override;
		void mouseReleaseEvent     (QGraphicsSceneMouseEvent *event) override;
		void mouseDoubleClickEvent (QGraphicsSceneMouseEvent *event) override;
		void keyPressEvent         (QKeyEvent *event) override;
		void init() override;

		Terminal *startTerminal() const;
			///The path fixed so far, in scene coordinates, from the
			///docking point of the start terminal
		QList<QPointF> fixedPoints() const { return m_points; }
		QList<QPointF> pathTo(const QPointF &scene_pos) const;

		static QList<QPointF> simplified(const QList<QPointF> &points);

	private:
		Terminal *terminalAt(const QPointF &scene_pos) const;
		void start(Terminal *terminal);
		void finishWire(Terminal *end);
		void cancelWire();
		void stepBack();
		void stop();
		void updatePreview();
		void showHint() const;
		QList<QPointF> leg(const QPointF &from, const QPointF &to, bool vertical_first) const;
		bool nextLegVertical() const;

		QPointer<Terminal> m_start;
		QList<QPointF> m_points;
			///Size of m_points before each bend, to take it back
		QList<int> m_steps;
		bool m_one_shot = false;
		bool m_flip = false;
		QPointF m_mouse;
		QGraphicsPathItem *m_preview = nullptr;
		QGraphicsEllipseItem *m_mark = nullptr;
};

#endif // DIAGRAMEVENTADDCONDUCTOR_H
