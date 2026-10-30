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
#include "diagrameventpaintstyle.h"

#include "../diagram.h"
#include "../qetapp.h"
#include "../qetdiagrameditor.h"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QStatusBar>
#include <QTimer>
#include <QUndoCommand>

/**
	@brief DiagramEventPaintStyle::DiagramEventPaintStyle
	@param diagram : diagram where operate this event
	@param source : the look to give, see StylePainter::capture()
*/
DiagramEventPaintStyle::DiagramEventPaintStyle(Diagram *diagram,
					       const StylePainter::Source &source) :
	DiagramEventInterface(diagram),
	m_source(source)
{
	if (m_diagram->isReadOnly() || m_source.kind == StylePainter::Nothing) return;

		//DiagramEventInterface::init() is called by Diagram::setEventInterface
		//only when it is replacing an earlier interface, as DiagramEventAddPaste.
	init();
	m_running = true;
	setCursor(Qt::CrossCursor);
	for (QGraphicsView *view : m_diagram->views())
		view->setContextMenuPolicy(Qt::NoContextMenu);
		//Deferred as DiagramEventAddImage: the status bar is cleared by the
		//tool this one replaces, after this constructor
	QTimer::singleShot(0, this, [this]() { showHint(); });
}

DiagramEventPaintStyle::~DiagramEventPaintStyle()
{
	if (!m_diagram) return;
	for (QGraphicsView *view : m_diagram->views())
	{
		view->viewport()->unsetCursor();
		view->setContextMenuPolicy(Qt::DefaultContextMenu);
	}
	if (!m_diagram->views().isEmpty())
	{
		if (auto *editor = QETApp::diagramEditorAncestorOf(m_diagram->views().constFirst()))
			editor->statusBar()->clearMessage();
	}
}

/**
	@brief DiagramEventPaintStyle::mousePressEvent
	Left click: paint the item under the mouse, then end unless Ctrl is
	held. Right click: end.
*/
void DiagramEventPaintStyle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (!m_running) return;
	event->setAccepted(true);

	if (event->button() == Qt::RightButton)
	{
		m_running = false;
		emit finish();
		return;
	}
	if (event->button() != Qt::LeftButton) return;

	QGraphicsItem *target = targetAt(event->scenePos());
	if (!target) return;
	if (QUndoCommand *undo = StylePainter::paint(m_source, target, m_diagram->project()))
		m_diagram->undoStack().push(undo);

	if (!(event->modifiers() & Qt::ControlModifier))
	{
		m_running = false;
		emit finish();
	}
}

/**
	@brief DiagramEventPaintStyle::mouseMoveEvent
	Show whether a click would paint the item under the mouse.
*/
void DiagramEventPaintStyle::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (!m_running) return;
	setCursor(targetAt(event->scenePos()) ? Qt::PointingHandCursor : Qt::CrossCursor);
	event->setAccepted(true);
}

	///Kept from the diagram: no selection rubber band nor move
void DiagramEventPaintStyle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_running) event->setAccepted(true);
}

	///Kept from the diagram: no edition of the item under the mouse
void DiagramEventPaintStyle::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_running) event->setAccepted(true);
}

/**
	@return the topmost item under pos that the look can be given to,
	see StylePainter::target()
*/
QGraphicsItem *DiagramEventPaintStyle::targetAt(const QPointF &pos) const
{
	for (QGraphicsItem *item : m_diagram->items(pos))
	{
		if (QGraphicsItem *target = StylePainter::target(m_source, item))
			return target;
	}
	return nullptr;
}

void DiagramEventPaintStyle::setCursor(Qt::CursorShape shape) const
{
	for (QGraphicsView *view : m_diagram->views())
		view->viewport()->setCursor(shape);
}

void DiagramEventPaintStyle::showHint() const
{
	if (m_diagram->views().isEmpty()) return;
	if (auto *editor = QETApp::diagramEditorAncestorOf(m_diagram->views().constFirst()))
		editor->statusBar()->showMessage(
				tr("Pincel de estilo (%1): clique no item que recebe o estilo."
				   " Ctrl+clique para continuar pintando. Esc ou clique direito: terminar.")
				.arg(StylePainter::kindName(m_source.kind)));
}
