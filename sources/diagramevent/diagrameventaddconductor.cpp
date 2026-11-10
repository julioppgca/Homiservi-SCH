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
#include "diagrameventaddconductor.h"

#include "../diagram.h"
#include "../qetapp.h"
#include "../qetdiagrameditor.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/terminal.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QPen>
#include <QStatusBar>
#include <QTimer>
#include <QToolTip>

namespace {
	bool isVertical(Qet::Orientation orientation)
	{
		return orientation == Qet::North || orientation == Qet::South;
	}

	bool same(const QPointF &a, const QPointF &b)
	{
		return qAbs(a.x() - b.x()) < 0.01 && qAbs(a.y() - b.y()) < 0.01;
	}
}

DiagramEventAddConductor::DiagramEventAddConductor(Diagram *diagram, Terminal *start,
						   bool one_shot) :
	DiagramEventInterface(diagram),
	m_one_shot(one_shot)
{
	m_running = true;
	init();

	m_preview = new QGraphicsPathItem();
	QPen pen(QColor(0, 90, 200));
	pen.setWidthF(1.0);
	pen.setStyle(Qt::DashLine);
	pen.setCosmetic(true);
	m_preview->setPen(pen);
	m_preview->setZValue(1000000);
	m_diagram->addItem(m_preview);

	m_mark = new QGraphicsEllipseItem(-4, -4, 8, 8);
	m_mark->setZValue(1000001);
	m_mark->setVisible(false);
	m_diagram->addItem(m_mark);

	if (start) {
		m_mouse = start->dockConductor();
		this->start(start);
	}
		//After the tool this one replaces has cleared the status bar
	QTimer::singleShot(0, this, [this]() { showHint(); });
}

DiagramEventAddConductor::~DiagramEventAddConductor()
{
	if (m_diagram) {
		if (m_preview) {
			m_diagram->removeItem(m_preview);
			delete m_preview;
		}
		if (m_mark) {
			m_diagram->removeItem(m_mark);
			delete m_mark;
		}
		const QList<QGraphicsView *> views = m_diagram->views();
		for (QGraphicsView *view : views)
			view->setContextMenuPolicy(Qt::DefaultContextMenu);
		if (!views.isEmpty())
			if (auto editor = QETApp::diagramEditorAncestorOf(views.constFirst()))
				editor->statusBar()->clearMessage();
	}
}

Terminal *DiagramEventAddConductor::startTerminal() const
{
	return m_start.data();
}

void DiagramEventAddConductor::init()
{
		//The right button takes back a bend
	for (QGraphicsView *view : m_diagram->views())
		view->setContextMenuPolicy(Qt::NoContextMenu);
}

void DiagramEventAddConductor::showHint() const
{
	if (!m_diagram || m_diagram->views().isEmpty()) return;
	auto editor = QETApp::diagramEditorAncestorOf(m_diagram->views().constFirst());
	if (!editor) return;
	editor->statusBar()->showMessage(m_start
		? tr("Fio: clique para fazer uma dobra, clique num borne para terminar."
		     " Espaço: inverte a dobra. Botão direito/Backspace: desfaz a dobra. Esc: cancela.")
		: tr("Fio: clique num borne para começar. Esc: sai."));
}

/**
	@return the terminal at scene_pos, the one whose docking point is the
	nearest when several are close
*/
Terminal *DiagramEventAddConductor::terminalAt(const QPointF &scene_pos) const
{
	const qreal margin = 3.0;
	Terminal *nearest = nullptr;
	qreal best = 0;
	const QList<QGraphicsItem *> items = m_diagram->items(
		QRectF(scene_pos - QPointF(margin, margin), QSizeF(2 * margin, 2 * margin)));
	for (QGraphicsItem *item : items) {
		auto terminal = qgraphicsitem_cast<Terminal *>(item);
		if (!terminal) continue;
		const qreal d = QLineF(terminal->dockConductor(), scene_pos).length();
		if (!nearest || d < best) {
			nearest = terminal;
			best = d;
		}
	}
	return nearest;
}

/**
	@return the corner and the end of an L from from to to: the first
	segment vertical if vertical_first; to alone when they are aligned
*/
QList<QPointF> DiagramEventAddConductor::leg(const QPointF &from, const QPointF &to,
					     bool vertical_first) const
{
	if (qAbs(from.x() - to.x()) < 0.01 || qAbs(from.y() - to.y()) < 0.01)
		return {to};
	const QPointF corner = vertical_first ? QPointF(from.x(), to.y())
					      : QPointF(to.x(), from.y());
	return {corner, to};
}

/**
	@return whether the next L starts with a vertical segment: out of the
	start terminal along its axis, then square to the last segment, or
	the other way after Space
*/
bool DiagramEventAddConductor::nextLegVertical() const
{
	bool vertical = true;
	if (m_points.size() < 2) {
		vertical = m_start ? isVertical(m_start->orientation()) : true;
	} else {
		const QPointF a = m_points.at(m_points.size() - 2);
		const QPointF b = m_points.last();
		vertical = qAbs(a.x() - b.x()) > 0.01;	//the last one is horizontal
	}
	return vertical != m_flip;
}

/**
	@return the path from the start terminal to scene_pos (snapped to the
	grid) or to the terminal there, as the wire would be drawn
*/
QList<QPointF> DiagramEventAddConductor::pathTo(const QPointF &scene_pos) const
{
	if (m_points.isEmpty()) return {};
	QList<QPointF> path = m_points;
	Terminal *target = terminalAt(scene_pos);
	if (target && target != m_start) {
			//Into the terminal along its axis
		const bool vertical_first = isVertical(target->orientation()) == m_flip;
		path << leg(path.last(), target->dockConductor(), vertical_first);
	} else {
		path << leg(path.last(), Diagram::snapToGrid(scene_pos), nextLegVertical());
	}
	return simplified(path);
}

/**
	@return points without repeated points nor points in the middle of a
	straight run
*/
QList<QPointF> DiagramEventAddConductor::simplified(const QList<QPointF> &points)
{
	QList<QPointF> result;
	for (const QPointF &point : points) {
		if (!result.isEmpty() && same(result.last(), point)) continue;
		if (result.size() >= 2) {
			const QPointF a = result.at(result.size() - 2);
			const QPointF b = result.last();
			const bool straight = (qAbs(a.x() - b.x()) < 0.01 && qAbs(b.x() - point.x()) < 0.01)
					   || (qAbs(a.y() - b.y()) < 0.01 && qAbs(b.y() - point.y()) < 0.01);
			if (straight) result.removeLast();
		}
		result << point;
	}
	return result;
}

void DiagramEventAddConductor::start(Terminal *terminal)
{
	m_start = terminal;
	m_points = {terminal->dockConductor()};
	m_steps.clear();
	m_flip = false;
	updatePreview();
	showHint();
}

void DiagramEventAddConductor::finishWire(Terminal *end)
{
	Terminal *start = m_start.data();
	if (!start) return;
	if (!start->canBeLinkedTo(end)) {
		const QString why = start->isLinkedTo(end)
			? tr("Estes bornes já estão ligados.")
			: tr("Um dos bornes já tem o máximo de fios das regras de fiação.");
		if (!m_diagram->views().isEmpty())
			if (auto editor = QETApp::diagramEditorAncestorOf(m_diagram->views().constFirst()))
				editor->statusBar()->showMessage(why, 4000);
		return;
	}
		//No bend placed: the path the application draws, as with a drag
	const QList<QPointF> path = m_points.size() > 1 ? pathTo(end->dockConductor())
							: QList<QPointF>();
	start->connectTo(end, path);
	cancelWire();
	if (m_one_shot) stop();
}

void DiagramEventAddConductor::cancelWire()
{
	m_start.clear();
	m_points.clear();
	m_steps.clear();
	m_flip = false;
	updatePreview();
	showHint();
}

void DiagramEventAddConductor::stepBack()
{
	if (!m_start) {
		stop();
		return;
	}
	if (m_steps.isEmpty()) {
		cancelWire();
		if (m_one_shot) stop();
		return;
	}
	const int size = m_steps.takeLast();
	while (m_points.size() > size) m_points.removeLast();
	m_flip = false;
	updatePreview();
}

void DiagramEventAddConductor::stop()
{
	m_running = false;
	emit finish();
}

void DiagramEventAddConductor::updatePreview()
{
	if (!m_preview) return;
	QPainterPath path;
	const QList<QPointF> points = m_start ? pathTo(m_mouse) : QList<QPointF>();
	for (int i = 0 ; i < points.size() ; ++i) {
		if (!i) path.moveTo(points.at(i));
		else path.lineTo(points.at(i));
	}
	m_preview->setPath(path);

		//The terminal under the mouse: green if the wire can end there,
		//orange if it already has a wire, red if not
	Terminal *target = terminalAt(m_mouse);
	if (!target || target == m_start) {
		m_mark->setVisible(false);
		return;
	}
	QColor color = Terminal::allowedColor;
	if (m_start && !m_start->canBeLinkedTo(target)) color = Terminal::forbiddenColor;
	else if (target->conductorsCount()) color = Terminal::warningColor;
	m_mark->setPen(QPen(color, 1.5));
	m_mark->setPos(target->dockConductor());
	m_mark->setVisible(true);
}

void DiagramEventAddConductor::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
		//Every button is the tool's while it runs
	event->setAccepted(true);
	if (event->button() != Qt::LeftButton || m_diagram->isReadOnly()) return;

	m_mouse = event->scenePos();
	Terminal *terminal = terminalAt(event->scenePos());
	if (!m_start) {
		if (terminal) start(terminal);
		return;
	}
	if (terminal && terminal != m_start) {
		finishWire(terminal);	//may delete this tool
		return;
	}
	if (terminal) return;	//the start terminal again

		//A bend
	const QPointF point = Diagram::snapToGrid(event->scenePos());
	const QList<QPointF> added = leg(m_points.last(), point, nextLegVertical());
	m_steps << m_points.size();
	m_points = simplified(m_points + added);
	m_flip = false;
	updatePreview();
}

void DiagramEventAddConductor::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	m_mouse = event->scenePos();
	updatePreview();
	event->setAccepted(true);
}

void DiagramEventAddConductor::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	event->setAccepted(true);
	if (event->button() == Qt::RightButton)
		stepBack();	//may delete this tool
}

void DiagramEventAddConductor::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
		//The second press of it has already been a click
	event->setAccepted(true);
}

void DiagramEventAddConductor::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
		case Qt::Key_Escape:
			event->accept();
			if (m_start && !m_one_shot) cancelWire();
			else stop();
			return;
		case Qt::Key_Space:
			event->accept();
			m_flip = !m_flip;
			updatePreview();
			return;
		case Qt::Key_Backspace:
			event->accept();
			stepBack();
			return;
		default:
			break;
	}
}
