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
#include "stylepainter.h"

#include "QPropertyUndoCommand/qpropertyundocommand.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/conductortextitem.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/elementtextitemgroup.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "qetgraphicsitem/qetshapeitem.h"
#include "qetproject.h"
#include "textstyles.h"

#include <QCoreApplication>
#include <QPointer>
#include <QUndoCommand>

using namespace StylePainter;

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("StylePainter", text);
	}

	bool isText(const QGraphicsItem *item)
	{
		return item->type() == IndependentTextItem::Type
				|| item->type() == DynamicElementTextItem::Type;
	}

		///Every text of element, its own ones first, then those of its groups
	QList<DynamicElementTextItem *> textsOf(const ::Element *element)
	{
		QList<DynamicElementTextItem *> texts = element->dynamicTextItems();
		for (ElementTextItemGroup *group : element->textGroups())
			texts << group->texts();
		return texts;
	}

	QString keyOf(const DynamicElementTextItem *text)
	{
		switch (text->textFrom()) {
			case DynamicElementTextItem::ElementInfo:
				return QStringLiteral("info:") + text->infoName();
			case DynamicElementTextItem::CompositeText:
				return QStringLiteral("composite:") + text->compositeText();
			default:
				return QStringLiteral("text:") + text->text();
		}
	}

		///The style text follows, written as its own: a text that follows
		///the automatic style of a label passes that style on to a function
	QString ownStyle(const DiagramTextItem *text, const QList<TextStyles::Style> &styles)
	{
		const QString style = TextStyles::effectiveStyle(styles, text);
		return style.isEmpty() ? TextStyles::NoStyle : style;
	}

	TextLook lookOf(const DiagramTextItem *text, const QList<TextStyles::Style> &styles)
	{
		TextLook look;
		look.style = ownStyle(text, styles);
		look.font = text->font();
		look.color = text->color();
		look.pos = text->pos();
		look.rotation = text->rotation();
		if (text->type() == DynamicElementTextItem::Type)
		{
			auto deti = static_cast<const DynamicElementTextItem *>(text);
			look.key = keyOf(deti);
			look.has_frame = true;
			look.frame = deti->frame();
				//The place of a text of a group is the group's business
			look.has_place = deti->parentGroup() == nullptr;
		}
		return look;
	}

	/**
		Give texts a look and bring them back, see TextLook. The place is
		only given to the texts that have one in the new look.
	*/
	class TextLookCommand : public QUndoCommand
	{
		public:
			TextLookCommand(QETProject *project, QUndoCommand *parent) :
				QUndoCommand(parent),
				m_project(project)
			{}

			void add(DiagramTextItem *text, const TextLook &look)
			{
				const QList<TextStyles::Style> styles = m_project
						? m_project->textStyles() : QList<TextStyles::Style>();
				TextLook old = lookOf(text, styles);
				old.style = text->textStyle();
				old.has_frame = look.has_frame && old.has_frame;
				old.has_place = look.has_place;
				TextLook now = look;
				now.has_frame = old.has_frame;
				m_texts << text;
				m_old << old;
				m_new << now;
			}

			bool isEmpty() const { return m_texts.isEmpty(); }
			void undo() override { set(m_old); }
			void redo() override { set(m_new); }

		private:
			void set(const QList<TextLook> &looks)
			{
				const QList<TextStyles::Style> styles = m_project
						? m_project->textStyles() : QList<TextStyles::Style>();
				for (int i = 0; i < m_texts.size(); ++i)
				{
					DiagramTextItem *text = m_texts.at(i);
					if (!text) continue;
					const TextLook &look = looks.at(i);
					text->setTextStyle(look.style);
					text->setStyledLook(look.font, look.color);
					if (look.has_frame)
						static_cast<DynamicElementTextItem *>(text)->setFrame(look.frame);
					if (look.has_place)
					{
							//As DynamicElementTextItem::fromXml(): the
							//position read back, not one moved by alignment
						text->QGraphicsTextItem::setRotation(look.rotation);
						text->QGraphicsTextItem::setPos(look.pos);
					}
					TextStyles::applyToText(styles, text);
				}
			}

			QPointer<QETProject> m_project;
			QList<QPointer<DiagramTextItem>> m_texts;
			QList<TextLook> m_old, m_new;
	};

		///command named caption, or nullptr if it has no text to change
	QUndoCommand *finished(TextLookCommand *command, const QString &caption)
	{
		if (command->isEmpty()) {
			delete command;
			return nullptr;
		}
		command->setText(caption);
		return command;
	}

		///The look of conductor that source gives
	ConductorProperties paintedConductor(const ConductorProperties &source,
					     ConductorProperties target)
	{
		target.type = source.type;
		target.color = source.color;
		target.m_color_2 = source.m_color_2;
		target.m_bicolor = source.m_bicolor;
		target.m_dash_size = source.m_dash_size;
		target.style = source.style;
		target.cond_size = source.cond_size;
		target.m_wire_color = source.m_wire_color;
		target.m_wire_section = source.m_wire_section;
		target.text_color = source.text_color;
		target.text_size = source.text_size;
		target.m_show_text = source.m_show_text;
		target.m_one_text_per_folio = source.m_one_text_per_folio;
		target.verti_rotate_text = source.verti_rotate_text;
		target.horiz_rotate_text = source.horiz_rotate_text;
		target.m_horizontal_alignment = source.m_horizontal_alignment;
		target.m_vertical_alignment = source.m_vertical_alignment;
		return target;
	}
}

/**
	@brief StylePainter::capture
	@return the look of the item of selection to copy: a lone text or
	element text before the rest, then an element, a conductor, a shape.
	Kind Nothing if the selection has none of them.
*/
Source StylePainter::capture(const QList<QGraphicsItem *> &selection)
{
	Source source;
	QGraphicsItem *chosen = nullptr;
	for (QGraphicsItem *item : selection)
		if (isText(item)) { chosen = item; break; }
	if (!chosen) {
		for (QGraphicsItem *item : selection)
		{
			if (item->type() == ::Element::Type || item->type() == ::Conductor::Type
			    || item->type() == QetShapeItem::Type)
			{
				chosen = item;
				break;
			}
		}
	}
	if (!chosen) return source;

	QETProject *project = nullptr;
	if (auto diagram = qobject_cast<Diagram *>(chosen->scene()))
		project = diagram->project();
	const QList<TextStyles::Style> styles = project ? project->textStyles()
							: QList<TextStyles::Style>();

	if (isText(chosen))
	{
		source.kind = Text;
		source.text = lookOf(static_cast<DiagramTextItem *>(chosen), styles);
	}
	else if (chosen->type() == ::Element::Type)
	{
		auto element = static_cast<::Element *>(chosen);
		source.kind = Element;
		source.location = element->location();
		for (DynamicElementTextItem *text : textsOf(element))
			source.element_texts << lookOf(text, styles);
	}
	else if (chosen->type() == ::Conductor::Type)
	{
		source.kind = Conductor;
		source.conductor = static_cast<::Conductor *>(chosen)->properties();
	}
	else
	{
		auto shape = static_cast<QetShapeItem *>(chosen);
		source.kind = Shape;
		source.pen = shape->pen();
		source.brush = shape->brush();
	}
	return source;
}

QString StylePainter::kindName(Kind kind)
{
	switch (kind) {
		case Text:      return tr("texto");
		case Element:   return tr("componente");
		case Conductor: return tr("fio");
		case Shape:     return tr("forma");
		case Nothing:   break;
	}
	return QString();
}

/**
	@return the item that source can be given to under item, the item under
	the mouse: item itself, or the element or conductor it belongs to. A
	text look also goes to an element, onto all its texts.
*/
QGraphicsItem *StylePainter::target(const Source &source, QGraphicsItem *item)
{
	if (!item) return nullptr;
	switch (source.kind)
	{
		case Text:
			if (isText(item)) return item;
			if (item->type() == ::Element::Type) return item;
			if (auto group = dynamic_cast<ElementTextItemGroup *>(item))
				return group->parentElement();
			return nullptr;

		case Element:
			if (item->type() == ::Element::Type) return item;
			if (item->type() == DynamicElementTextItem::Type)
				return static_cast<DynamicElementTextItem *>(item)->parentElement();
			if (auto group = dynamic_cast<ElementTextItemGroup *>(item))
				return group->parentElement();
			return nullptr;

		case Conductor:
			if (item->type() == ::Conductor::Type) return item;
			if (item->type() == ConductorTextItem::Type)
				return static_cast<ConductorTextItem *>(item)->parentConductor();
			return nullptr;

		case Shape:
			return item->type() == QetShapeItem::Type ? item : nullptr;

		case Nothing:
			break;
	}
	return nullptr;
}

/**
	@brief StylePainter::paint
	@return the command giving target the look of source, or nullptr if
	that changes nothing. target is what StylePainter::target() returned.
*/
QUndoCommand *StylePainter::paint(const Source &source, QGraphicsItem *target, QETProject *project)
{
	if (!target) return nullptr;
	const QString caption = tr("Copiar o estilo (%1)").arg(kindName(source.kind));

	if (source.kind == Text)
	{
		auto command = new TextLookCommand(project, nullptr);
		TextLook look = source.text;
		look.has_place = false;
		if (isText(target))
			command->add(static_cast<DiagramTextItem *>(target), look);
		else
			for (DynamicElementTextItem *text : textsOf(static_cast<::Element *>(target)))
				command->add(text, look);
		return finished(command, caption);
	}

	if (source.kind == Element)
	{
		auto element = static_cast<::Element *>(target);
		const bool same_definition = element->location() == source.location;
		auto command = new TextLookCommand(project, nullptr);
		QList<int> used;
		for (DynamicElementTextItem *text : textsOf(element))
		{
			const QString key = keyOf(text);
			for (int i = 0; i < source.element_texts.size(); ++i)
			{
				if (used.contains(i) || source.element_texts.at(i).key != key) continue;
				used << i;
				TextLook look = source.element_texts.at(i);
				look.has_place = same_definition && look.has_place
						&& text->parentGroup() == nullptr;
				command->add(text, look);
				break;
			}
		}
		return finished(command, caption);
	}

	auto undo = new QUndoCommand(caption);
	if (source.kind == Conductor)
	{
		auto conductor = static_cast<::Conductor *>(target);
		QSet<::Conductor *> conductors = conductor->relatedPotentialConductors();
		conductors << conductor;
		for (::Conductor *c : std::as_const(conductors))
		{
			const ConductorProperties old_properties = c->properties();
			const ConductorProperties new_properties = paintedConductor(source.conductor, old_properties);
			if (new_properties == old_properties) continue;
			QVariant old_value, new_value;
			old_value.setValue(old_properties);
			new_value.setValue(new_properties);
			new QPropertyUndoCommand(c, "properties", old_value, new_value, undo);
		}
	}
	else if (source.kind == Shape)
	{
		auto shape = static_cast<QetShapeItem *>(target);
		if (shape->pen() != source.pen)
			new QPropertyUndoCommand(shape, "pen", QVariant::fromValue(shape->pen()),
						 QVariant::fromValue(source.pen), undo);
		if (shape->brush() != source.brush)
			new QPropertyUndoCommand(shape, "brush", QVariant::fromValue(shape->brush()),
						 QVariant::fromValue(source.brush), undo);
	}

	if (undo->childCount() == 0) {
		delete undo;
		return nullptr;
	}
	return undo;
}
