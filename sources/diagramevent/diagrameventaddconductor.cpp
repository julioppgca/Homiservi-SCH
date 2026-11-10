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
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../undocommand/addgraphicsobjectcommand.h"
#include "../wirejunction.h"

#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QStatusBar>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>

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
						   bool one_shot, Mode mode) :
	DiagramEventInterface(diagram),
	m_mode(mode),
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
	updateCursor();
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
		for (QGraphicsView *view : views) {
			view->setContextMenuPolicy(Qt::DefaultContextMenu);
			view->viewport()->unsetCursor();
		}
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

/**
	@return the glyph of the tool: a wire with a bend, or a junction dot
	with its three wires
*/
QPixmap DiagramEventAddConductor::glyph(Mode mode, int size, const QColor &color)
{
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	const qreal s = size;
	QPen pen(color, qMax(1.5, s / 14.0));
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	painter.setPen(pen);
	if (mode == Junction) {
		painter.drawLine(QPointF(s * 0.12, s * 0.5), QPointF(s * 0.88, s * 0.5));
		painter.drawLine(QPointF(s * 0.5, s * 0.5), QPointF(s * 0.5, s * 0.88));
		painter.setBrush(color);
		painter.drawEllipse(QPointF(s * 0.5, s * 0.5), s * 0.13, s * 0.13);
	} else {
		QPainterPath path;
		path.moveTo(s * 0.15, s * 0.85);
		path.lineTo(s * 0.15, s * 0.35);
		path.lineTo(s * 0.85, s * 0.35);
		painter.drawPath(path);
		painter.setBrush(color);
		painter.drawEllipse(QPointF(s * 0.15, s * 0.85), s * 0.08, s * 0.08);
		painter.drawEllipse(QPointF(s * 0.85, s * 0.35), s * 0.08, s * 0.08);
	}
	return pixmap;
}

/**
	@return the cursor of the tool: a cross whose centre is the hot spot,
	with the glyph of the tool beside it, so it shows the tool is on
*/
QCursor DiagramEventAddConductor::cursor(Mode mode)
{
	const int size = 32;
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	const auto cross = [&painter](const QColor &color, qreal width) {
		painter.setPen(QPen(color, width));
		painter.drawLine(QPointF(8, 0.5), QPointF(8, 15.5));
		painter.drawLine(QPointF(0.5, 8), QPointF(15.5, 8));
	};
	cross(Qt::white, 3);
	cross(Qt::black, 1);
	const QColor color(0, 90, 200);
	painter.drawPixmap(14, 14, glyph(mode, 18, Qt::white));
	painter.drawPixmap(15, 15, glyph(mode, 16, color));
	painter.end();
	return QCursor(pixmap, 8, 8);
}

void DiagramEventAddConductor::updateCursor() const
{
	if (!m_diagram) return;
	const QCursor shape = cursor(m_mode);
	for (QGraphicsView *view : m_diagram->views())
		view->viewport()->setCursor(shape);
}

void DiagramEventAddConductor::message(const QString &text) const
{
	if (!m_diagram || m_diagram->views().isEmpty()) return;
	if (auto editor = QETApp::diagramEditorAncestorOf(m_diagram->views().constFirst()))
		editor->statusBar()->showMessage(text, 4000);
}

void DiagramEventAddConductor::showHint() const
{
	if (!m_diagram || m_diagram->views().isEmpty()) return;
	auto editor = QETApp::diagramEditorAncestorOf(m_diagram->views().constFirst());
	if (!editor) return;
	QString text;
	if (isDrawing())
		text = tr("Fio: clique para fazer uma dobra, clique num borne ou num fio para terminar."
			  " Espaço: inverte a dobra. Botão direito/Backspace: desfaz a dobra. Esc: cancela.");
	else if (m_mode == Junction)
		text = tr("Junção: clique na folha ou num fio para pôr uma junção; dela sai um fio. Esc: sai.");
	else
		text = tr("Fio: clique num borne ou num fio para começar. Esc: sai.");
	editor->statusBar()->showMessage(text);
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
	@return the wire at scene_pos and in on the point of its path there,
	on the grid; nullptr if there is none
*/
Conductor *DiagramEventAddConductor::conductorAt(const QPointF &scene_pos, QPointF *on) const
{
	const qreal margin = 3.0;
	const QList<QGraphicsItem *> items = m_diagram->items(
		QRectF(scene_pos - QPointF(margin, margin), QSizeF(2 * margin, 2 * margin)));
	for (QGraphicsItem *item : items) {
		auto conductor = qgraphicsitem_cast<Conductor *>(item);
		if (conductor && WireJunction::pointOnConductor(conductor, scene_pos, on, margin))
			return conductor;
	}
	return nullptr;
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
	@return whether the next L, towards towards, starts with a vertical
	segment: out of the start terminal along its axis (out of a junction
	or a wire, along the longer way), then square to the last segment;
	the other way after Space
*/
bool DiagramEventAddConductor::nextLegVertical(const QPointF &towards) const
{
	bool vertical = true;
	if (m_points.size() < 2) {
		if (m_start && !WireJunction::isJunction(m_start)) {
			vertical = isVertical(m_start->orientation());
		} else if (!m_points.isEmpty()) {
			const QPointF from = m_points.first();
			vertical = qAbs(towards.y() - from.y()) > qAbs(towards.x() - from.x());
		}
	} else {
		const QPointF a = m_points.at(m_points.size() - 2);
		const QPointF b = m_points.last();
		vertical = qAbs(a.x() - b.x()) > 0.01;	//the last one is horizontal
	}
	return vertical != m_flip;
}

/**
	@return the path from the start to scene_pos (snapped to the grid),
	to the terminal there or to the wire there, as the wire would be drawn
*/
QList<QPointF> DiagramEventAddConductor::pathTo(const QPointF &scene_pos) const
{
	if (m_points.isEmpty()) return {};
	QList<QPointF> path = m_points;
	Terminal *target = terminalAt(scene_pos);
	QPointF on;
	if (target && target != m_start && !WireJunction::isJunction(target)) {
			//Into the terminal along its axis
		const bool vertical_first = isVertical(target->orientation()) == m_flip;
		path << leg(path.last(), target->dockConductor(), vertical_first);
	} else if (target && target != m_start) {
		path << leg(path.last(), target->dockConductor(), nextLegVertical(target->dockConductor()));
	} else if (!target && conductorAt(scene_pos, &on)) {
		path << leg(path.last(), on, nextLegVertical(on));
	} else {
		const QPointF point = Diagram::snapToGrid(scene_pos);
		path << leg(path.last(), point, nextLegVertical(point));
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
	m_start_conductor.clear();
	m_points = {terminal->dockConductor()};
	m_steps.clear();
	m_flip = false;
	updatePreview();
	showHint();
}

void DiagramEventAddConductor::startOnConductor(Conductor *conductor, const QPointF &point)
{
	m_start.clear();
	m_start_conductor = conductor;
	m_points = {point};
	m_steps.clear();
	m_flip = false;
	updatePreview();
	showHint();
}

/**
	Put a junction at point, on the wire there if any, as one undo step
	@return its terminal, nullptr if it could not be put
*/
Terminal *DiagramEventAddConductor::placeJunction(const QPointF &point)
{
	QPointF on;
	Conductor *conductor = conductorAt(point, &on);
	auto undo = new QUndoCommand(tr("Adicionar junção"));
	Terminal *joint = nullptr;
	if (conductor) {
		joint = WireJunction::split(conductor, on, undo);
	} else {
		const QPointF at = Diagram::snapToGrid(point);
		if (Element *junction = WireJunction::create(m_diagram, at)) {
			new AddGraphicsObjectCommand(junction, m_diagram, at, undo);
			if (!junction->terminals().isEmpty()) joint = junction->terminals().first();
		}
	}
	if (!joint) {
		delete undo;
		return nullptr;
	}
	m_diagram->undoStack().push(undo);
	return joint;
}

/**
	End the wire at end, or on end_conductor at end_point (a junction is
	put there), as one undo step with the junction(s) it needs
*/
void DiagramEventAddConductor::finishWire(Terminal *end, Conductor *end_conductor,
					  const QPointF &end_point)
{
	if (!isDrawing()) return;
	Terminal *start = m_start.data();
	if (!start && !m_start_conductor) {
		cancelWire();
		return;
	}
	if (end_conductor && end_conductor == m_start_conductor) {
		message(tr("O fio não pode começar e terminar no mesmo fio."));
		return;
	}
	if (end && start && !start->canBeLinkedTo(end)) {
		message(start->isLinkedTo(end)
			? tr("Estes bornes já estão ligados.")
			: tr("Um dos bornes já tem o máximo de fios das regras de fiação."));
		return;
	}
	if (end && !start && !end->hasRoomForWire()) {
		message(tr("Este borne já tem o máximo de fios das regras de fiação."));
		return;
	}

	const QPointF target = end ? end->dockConductor() : end_point;
		//No bend placed between two terminals: the path the application
		//draws, as with a drag
	QList<QPointF> path;
	const bool plain = start && end && m_points.size() == 1
			&& !WireJunction::isJunction(start) && !WireJunction::isJunction(end);
	if (!plain) path = pathTo(target);

	QUndoStack &stack = m_diagram->undoStack();
	stack.beginMacro(tr("Adicionar fio"));
	if (!start) {
		auto undo = new QUndoCommand(tr("Adicionar junção"));
		start = WireJunction::split(m_start_conductor, m_points.first(), undo);
		if (start) stack.push(undo); else delete undo;
	}
	if (start && end_conductor) {
		auto undo = new QUndoCommand(tr("Adicionar junção"));
		end = WireJunction::split(end_conductor, end_point, undo);
		if (end) stack.push(undo); else delete undo;
	}
	if (start && end) start->connectTo(end, path);
	stack.endMacro();

	cancelWire();
	if (m_one_shot) stop();
}

void DiagramEventAddConductor::cancelWire()
{
	m_start.clear();
	m_start_conductor.clear();
	m_points.clear();
	m_steps.clear();
	m_flip = false;
	updatePreview();
	showHint();
}

void DiagramEventAddConductor::stepBack()
{
	if (!isDrawing()) {
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
	const QList<QPointF> points = isDrawing() ? pathTo(m_mouse) : QList<QPointF>();
	for (int i = 0 ; i < points.size() ; ++i) {
		if (!i) path.moveTo(points.at(i));
		else path.lineTo(points.at(i));
	}
	m_preview->setPath(path);

		//Where the click would go: a terminal (green if the wire can end
		//there, orange if it already has a wire, red if not) or the
		//point of a wire where a junction would be put
	Terminal *target = terminalAt(m_mouse);
	QPointF on;
	if (target && target != m_start) {
		QColor color = Terminal::allowedColor;
		if (m_start && !m_start->canBeLinkedTo(target)) color = Terminal::forbiddenColor;
		else if (!WireJunction::isJunction(target) && target->conductorsCount())
			color = Terminal::warningColor;
		m_mark->setPen(QPen(color, 1.5));
		m_mark->setBrush(Qt::NoBrush);
		m_mark->setPos(target->dockConductor());
		m_mark->setVisible(true);
	} else if (!target && conductorAt(m_mouse, &on)) {
		m_mark->setPen(QPen(Terminal::allowedColor, 1.5));
		m_mark->setBrush(Terminal::allowedColor);
		m_mark->setPos(on);
		m_mark->setVisible(true);
	} else {
		m_mark->setVisible(false);
	}
}

void DiagramEventAddConductor::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
		//Every button is the tool's while it runs
	event->setAccepted(true);
	if (event->button() != Qt::LeftButton || m_diagram->isReadOnly()) return;

	m_mouse = event->scenePos();
	Terminal *terminal = terminalAt(event->scenePos());
	QPointF on;
	Conductor *conductor = terminal ? nullptr : conductorAt(event->scenePos(), &on);

	if (!isDrawing()) {
		if (m_mode == Junction && !terminal) {
			if (Terminal *joint = placeJunction(event->scenePos())) start(joint);
		} else if (terminal) {
			start(terminal);
		} else if (conductor) {
			startOnConductor(conductor, on);
		}
		return;
	}
	if (terminal && terminal != m_start) {
		finishWire(terminal);	//may delete this tool
		return;
	}
	if (terminal) return;	//the start terminal again
	if (conductor) {
		finishWire(nullptr, conductor, on);	//may delete this tool
		return;
	}

		//A bend
	const QPointF point = Diagram::snapToGrid(event->scenePos());
	const QList<QPointF> added = leg(m_points.last(), point, nextLegVertical(point));
	m_steps << m_points.size();
	m_points = simplified(m_points + added);
	m_flip = false;
	updatePreview();
}

void DiagramEventAddConductor::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	m_mouse = event->scenePos();
	updatePreview();
		//An item under the mouse may have set its own
	updateCursor();
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
			if (isDrawing() && !m_one_shot) cancelWire();
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
