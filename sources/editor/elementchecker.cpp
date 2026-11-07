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
#include "elementchecker.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "UndoCommand/addpartcommand.h"
#include "UndoCommand/deletepartscommand.h"
#include "elementscene.h"
#include "elementtextstyles.h"
#include "graphicspart/partdynamictextfield.h"
#include "graphicspart/partterminal.h"
#include "graphicspart/parttext.h"
#include "terminalnamecheck.h"

#include <QCoreApplication>
#include <QHash>
#include <QUndoCommand>

namespace {
	QString tr(const char *text, int n = -1)
	{
		return QCoreApplication::translate("ElementChecker", text, nullptr, n);
	}

	bool isText(const QGraphicsItem *item)
	{
		return item->type() == PartText::Type || item->type() == PartDynamicTextField::Type;
	}

		///The rectangle of the drawing: the parts other than terminals and texts
	QRectF drawingRect(ElementScene *scene)
	{
		QRectF rect;
		for (CustomElementPart *part : scene->primitives()) {
			QGraphicsItem *item = part->toItem();
			if (item->type() != PartTerminal::Type && !isText(item))
				rect = rect.united(item->sceneBoundingRect());
		}
		return rect;
	}

		///The way out of the drawing nearest to point: the side of rect it is closest to
	Qet::Orientation outward(const QRectF &rect, const QPointF &point)
	{
		const qreal top = qAbs(point.y() - rect.top()), bottom = qAbs(point.y() - rect.bottom()),
			    left = qAbs(point.x() - rect.left()), right = qAbs(point.x() - rect.right());
		const qreal nearest = qMin(qMin(top, bottom), qMin(left, right));
		if (nearest == top) return Qet::North;
		if (nearest == bottom) return Qet::South;
		if (nearest == left) return Qet::West;
		return Qet::East;
	}

		///Whether terminal faces into the drawing: it leaves by the side opposite to where it is
	bool facesIn(const PartTerminal *terminal, const QRectF &rect)
	{
		const QPointF p = terminal->scenePos(), c = rect.center();
		switch (terminal->orientation()) {
			case Qet::North: return p.y() > c.y();
			case Qet::South: return p.y() < c.y();
			case Qet::West:  return p.x() > c.x();
			case Qet::East:  return p.x() < c.x();
		}
		return false;
	}

		///Whether the short line of terminal (from its point into the element) touches a part
	bool touchesDrawing(ElementScene *scene, const PartTerminal *terminal)
	{
		const QPointF p = terminal->scenePos();
		QPointF inward;
		switch (terminal->orientation()) {
			case Qet::North: inward = QPointF(0, 1); break;
			case Qet::South: inward = QPointF(0, -1); break;
			case Qet::West:  inward = QPointF(1, 0); break;
			case Qet::East:  inward = QPointF(-1, 0); break;
		}
		const QRectF reach = QRectF(p, p + inward * 6).normalized().adjusted(-1.5, -1.5, 1.5, 1.5);
		for (QGraphicsItem *item : scene->items(reach, Qt::IntersectsItemShape)) {
			if (item->type() == PartTerminal::Type || isText(item))
				continue;
			if (dynamic_cast<CustomElementPart *>(item))
				return true;
		}
		return false;
	}

	bool showsLabel(const QGraphicsItem *item)
	{
		auto field = qgraphicsitem_cast<const PartDynamicTextField *>(item);
		if (!field)
			return false;
		if (field->textFrom() == DynamicElementTextItem::ElementInfo)
			return field->infoName() == QLatin1String("label");
		if (field->textFrom() == DynamicElementTextItem::CompositeText)
			return field->compositeText().contains(QLatin1String("%{label}"));
		return false;
	}
}

/**
	@return what is wrong with the element of scene, errors first
*/
QList<ElementChecker::Issue> ElementChecker::check(ElementScene *scene)
{
	QList<Issue> issues;
	const ElementData::Type type = scene->elementData().m_type;
	const bool connectable = !(type & ElementData::AllReport)
				 && type != ElementData::ConductorDefinition
				 && type != ElementData::Thumbnail;

	QList<PartTerminal *> terminals;
	QList<QGraphicsItem *> texts, useless;
	bool has_label = false;
	for (CustomElementPart *part : scene->primitives()) {
		QGraphicsItem *item = part->toItem();
		if (auto terminal = qgraphicsitem_cast<PartTerminal *>(item))
			terminals << terminal;
		else if (isText(item))
			texts << item;
		if (part->isUseless())
			useless << item;
		has_label = has_label || showsLabel(item);
	}
	const QRectF drawing = drawingRect(scene);

		//Terminals on top of each other: one conductor would reach both
	QHash<QPair<qreal, qreal>, QList<QGraphicsItem *>> at;
	for (PartTerminal *terminal : terminals)
		at[qMakePair(terminal->scenePos().x(), terminal->scenePos().y())] << terminal;
	QList<QGraphicsItem *> stacked;
	for (const QList<QGraphicsItem *> &same : std::as_const(at))
		if (same.size() > 1)
			stacked << same;
	if (!stacked.isEmpty())
		issues << Issue {QStringLiteral("stacked"), Error, tr("Bornes sobrepostos"),
				 tr("%n borne(s) no mesmo ponto de outro: um condutor ligado ali pega todos eles.", stacked.size()),
				 stacked, NoFix};

		//Repeated and missing names
	QStringList names;
	for (PartTerminal *terminal : terminals)
		names << terminal->terminalName();
	const auto repeated = TerminalNameCheck::repeatedNames(names);
	if (!repeated.isEmpty()) {
		QList<QGraphicsItem *> items;
		for (PartTerminal *terminal : terminals)
			for (const auto &entry : repeated)
				if (terminal->terminalName().trimmed() == entry.first)
					items << terminal;
		issues << Issue {QStringLiteral("names"), Error, tr("Nomes de bornes repetidos"),
				 tr("Mais de um borne com o mesmo nome: %1. A lista de ligações não consegue dizer qual é qual.")
					 .arg(TerminalNameCheck::describe(repeated)),
				 items, NoFix};
	}

		//Off the diagrams' grid: the conductors do not reach them
	QList<QGraphicsItem *> off_grid;
	for (PartTerminal *terminal : scene->terminalsOffGrid())
		off_grid << terminal;
	if (!off_grid.isEmpty())
		issues << Issue {QStringLiteral("off_grid"), Warning, tr("Bornes fora da grade"),
				 tr("%n borne(s) fora da grade de 10 dos esquemas: os condutores ficam tortos ou não se ligam.", off_grid.size()),
				 off_grid, TerminalsOnGrid};

	if (drawing.isValid()) {
		QList<QGraphicsItem *> facing_in, loose;
		for (PartTerminal *terminal : terminals) {
			if (facesIn(terminal, drawing))
				facing_in << terminal;
			else if (!touchesDrawing(scene, terminal))
				loose << terminal;
		}
		if (!facing_in.isEmpty())
			issues << Issue {QStringLiteral("facing_in"), Warning, tr("Bornes virados para dentro"),
					 tr("%n borne(s) com a saída do condutor virada para dentro do desenho: o condutor atravessa o símbolo.", facing_in.size()),
					 facing_in, TurnTerminals};
		if (!loose.isEmpty())
			issues << Issue {QStringLiteral("loose"), Hint, tr("Bornes soltos"),
					 tr("%n borne(s) sem encostar em nenhuma linha do desenho.", loose.size()),
					 loose, NoFix};
	}

	const int unnamed = TerminalNameCheck::unnamedCount(names);
	if (unnamed && connectable) {
		QList<QGraphicsItem *> items;
		for (PartTerminal *terminal : terminals)
			if (terminal->terminalName().trimmed().isEmpty())
				items << terminal;
		issues << Issue {QStringLiteral("unnamed"), Warning, tr("Bornes sem nome"),
				 tr("%n borne(s) sem nome: a lista de ligações não consegue identificá-los.", unnamed),
				 items, NoFix};
	}

		//The label (K1, Q2...) must be seen on the diagram
	if (!has_label && connectable && !terminals.isEmpty())
		issues << Issue {QStringLiteral("label"), Warning, tr("Sem campo de rótulo"),
				 tr("Nenhum campo de texto mostra o rótulo do elemento (K1, Q2...): ele não aparece no esquema."),
				 {}, AddLabelField};

		//Texts over a terminal: the conductor crosses them
	QList<QGraphicsItem *> over;
	for (QGraphicsItem *text : texts) {
		const QRectF rect = text->sceneBoundingRect().adjusted(1, 1, -1, -1);
		for (PartTerminal *terminal : terminals)
			if (rect.contains(terminal->scenePos())) {
				over << text;
				break;
			}
	}
	if (!over.isEmpty())
		issues << Issue {QStringLiteral("text_over"), Warning, tr("Texto em cima de borne"),
				 tr("%n texto(s) cobrindo o ponto de ligação de um borne.", over.size()),
				 over, NoFix};

	if (!useless.isEmpty())
		issues << Issue {QStringLiteral("useless"), Hint, tr("Partes vazias"),
				 tr("%n parte(s) sem tamanho ou sem texto, que não aparecem.", useless.size()),
				 useless, RemoveParts};

	std::stable_sort(issues.begin(), issues.end(), [](const Issue &a, const Issue &b) {
		return a.severity < b.severity;
	});
	return issues;
}

QString ElementChecker::fixName(Fix fix)
{
	switch (fix) {
		case TerminalsOnGrid: return tr("Colocar na grade");
		case TurnTerminals:   return tr("Virar para fora");
		case AddLabelField:   return tr("Adicionar campo de rótulo");
		case RemoveParts:     return tr("Remover");
		case NoFix:           break;
	}
	return QString();
}

/**
	@return the undo fixing issue, nullptr when it has no fix
*/
QUndoCommand *ElementChecker::fix(ElementScene *scene, const Issue &issue)
{
	switch (issue.fix) {
		case TerminalsOnGrid: {
			auto undo = new QUndoCommand(tr("Colocar bornes na grade"));
			for (QGraphicsItem *item : issue.items) {
				auto terminal = static_cast<PartTerminal *>(item);
				new QPropertyUndoCommand(terminal, "pos", terminal->pos(),
							 scene->snapTerminal(terminal->pos(), Qt::NoModifier), undo);
			}
			return undo;
		}
		case TurnTerminals: {
			const QRectF drawing = drawingRect(scene);
			auto undo = new QUndoCommand(tr("Virar bornes para fora"));
			for (QGraphicsItem *item : issue.items) {
				auto terminal = static_cast<PartTerminal *>(item);
				new QPropertyUndoCommand(terminal, "orientation", QVariant::fromValue(terminal->orientation()),
							 QVariant::fromValue(outward(drawing, terminal->scenePos())), undo);
			}
			return undo;
		}
		case AddLabelField: {
				//Above the top left corner of the drawing, on the grid
			const QRectF drawing = drawingRect(scene);
			auto field = new PartDynamicTextField(scene->editor());
			field->setTextFrom(DynamicElementTextItem::ElementInfo);
			field->setInfoName(QStringLiteral("label"));
			const QList<TextStyles::Style> styles = ElementTextStyles::load();
			const QString label_style = QCoreApplication::translate("ElementTextStyles", "Rótulo");
			if (const TextStyles::Style *style = TextStyles::find(styles, label_style)) {
				field->setFont(style->font);
				field->setColor(style->color);
				field->setElementStyle(style->name);
			}
			scene->addItem(field);
			const QPointF corner = drawing.isValid() ? drawing.topLeft() : QPointF();
			field->setPos(scene->snapToGrid(corner - QPointF(0, field->boundingRect().height() + 2)));
			return new AddPartCommand(tr("Adicionar campo de rótulo"), scene, field);
		}
		case RemoveParts:
			return new DeletePartsCommand(scene, QVector<QGraphicsItem *>(issue.items.begin(), issue.items.end()));
		case NoFix:
			break;
	}
	return nullptr;
}
