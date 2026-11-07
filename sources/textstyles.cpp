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
#include "textstyles.h"

#include "diagram.h"
#include "qetapp.h"
#include "qetinformation.h"
#include "qetproject.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/elementtextitemgroup.h"
#include "qetgraphicsitem/independenttextitem.h"
#include "utils/qetutils.h"

#include <QCoreApplication>

const QString TextStyles::NoStyle = QStringLiteral("-");
const QString TextStyles::FreeTexts = QStringLiteral("@free");
const QString TextStyles::AllElementTexts = QStringLiteral("@element");

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("TextStyles", text);
	}

	TextStyles::Style style(const QString &name, qreal size, bool bold, bool italic,
				const QColor &color, const QString &target)
	{
		TextStyles::Style style;
		style.name = name;
		style.font = QETApp::diagramTextsFont(size);
		style.font.setBold(bold);
		style.font.setItalic(italic);
		style.color = color;
		style.target = target;
		return style;
	}
}

bool TextStyles::Style::operator==(const Style &other) const
{
	return name == other.name && font == other.font
			&& color == other.color && target == other.target;
}

/**
	@return a starting set: labels, functions, locations and free notes
*/
QList<TextStyles::Style> TextStyles::examples()
{
	return {
		style(tr("Identificação"), 9, true, false, Qt::black, QStringLiteral("label")),
		style(tr("Função"), 7, false, true, QColor(0x40, 0x40, 0x40), QStringLiteral("function")),
		style(tr("Localização"), 7, false, false, QColor(0x40, 0x40, 0x40), QStringLiteral("location")),
		style(tr("Notas"), 9, false, false, QColor(0x1f, 0x4e, 0x9a), FreeTexts)
	};
}

/**
	@return every target a style can take by itself, the first one being
	none
*/
QStringList TextStyles::targets()
{
	QStringList list {QString(), FreeTexts, AllElementTexts};
	list << QETInformation::elementInfoKeys();
	return list;
}

QString TextStyles::targetName(const QString &target)
{
	if (target.isEmpty()) return tr("Nenhum (só os textos escolhidos)");
	if (target == FreeTexts) return tr("Todos os textos livres");
	if (target == AllElementTexts) return tr("Todos os textos de elementos");
	return tr("Textos de elemento: %1").arg(QETInformation::translatedInfoKey(target));
}

QString TextStyles::fontDescription(const QFont &font)
{
	QString text = QStringLiteral("%1 %2").arg(font.family()).arg(font.pointSizeF());
	if (font.bold()) text += tr(", negrito");
	if (font.italic()) text += tr(", itálico");
	if (font.underline()) text += tr(", sublinhado");
	return text;
}

const TextStyles::Style *TextStyles::find(const QList<Style> &styles, const QString &name)
{
	for (const Style &style : styles)
		if (style.name == name) return &style;
	return nullptr;
}

/**
	@return the name of the style text follows: its own one, or else the
	style that takes it by itself (a style for its element information
	before one for every text of element). Empty if none.
*/
QString TextStyles::effectiveStyle(const QList<Style> &styles, const DiagramTextItem *text)
{
	const QString own = text->textStyle();
	if (own == NoStyle) return QString();
	if (!own.isEmpty()) return own;

	if (text->type() == IndependentTextItem::Type)
	{
		for (const Style &style : styles)
			if (style.target == FreeTexts) return style.name;
		return QString();
	}

	if (text->type() != DynamicElementTextItem::Type) return QString();
	auto deti = static_cast<const DynamicElementTextItem *>(text);
	QString every_text;
	for (const Style &style : styles)
	{
		if (style.target == AllElementTexts && every_text.isEmpty())
			every_text = style.name;
		else if (!style.target.isEmpty()
			 && deti->textFrom() == DynamicElementTextItem::ElementInfo
			 && deti->infoName() == style.target)
			return style.name;
	}
	return every_text;
}

/**
	@brief TextStyles::applyToText
	Give text the font and the colour of the style it follows, if any.
*/
void TextStyles::applyToText(const QList<Style> &styles, DiagramTextItem *text)
{
	if (const Style *style = find(styles, effectiveStyle(styles, text)))
		text->setStyledLook(style->font, style->color);
	else
		text->setStyleBound(false);
}

/**
	@brief TextStyles::applyToProject
	Bring every text of every folio of project to its style.
*/
void TextStyles::applyToProject(QETProject *project)
{
	if (!project) return;
	const QList<Style> styles = project->textStyles();
	for (Diagram *diagram : project->diagrams())
	{
		for (QGraphicsItem *item : diagram->items())
		{
			if (item->type() == IndependentTextItem::Type
			    || item->type() == DynamicElementTextItem::Type)
				applyToText(styles, static_cast<DiagramTextItem *>(item));
		}
	}
}

/**
	@return the texts a style can be given to among items: free texts,
	texts of element, and every text of the elements of items
*/
QList<DiagramTextItem *> TextStyles::styledTexts(const QList<QGraphicsItem *> &items)
{
	QList<DiagramTextItem *> texts;
	auto add = [&texts](DiagramTextItem *text) {
		if (!texts.contains(text)) texts << text;
	};
	for (QGraphicsItem *item : items)
	{
		if (item->type() == IndependentTextItem::Type
		    || item->type() == DynamicElementTextItem::Type)
			add(static_cast<DiagramTextItem *>(item));
		else if (auto group = dynamic_cast<ElementTextItemGroup *>(item))
		{
			for (DynamicElementTextItem *text : group->texts())
				add(text);
		}
		else if (item->type() == Element::Type)
		{
			auto element = static_cast<Element *>(item);
			for (DynamicElementTextItem *text : element->dynamicTextItems())
				add(text);
			for (ElementTextItemGroup *group : element->textGroups())
				for (DynamicElementTextItem *text : group->texts())
					add(text);
		}
	}
	return texts;
}

/**
	@return the styles of the <text_styles> child of project_root
*/
QList<TextStyles::Style> TextStyles::fromXml(const QDomElement &project_root)
{
	QList<Style> styles;
	const QDomElement root = project_root.firstChildElement(QStringLiteral("text_styles"));
	for (QDomElement e = root.firstChildElement(QStringLiteral("style")); !e.isNull();
	     e = e.nextSiblingElement(QStringLiteral("style")))
	{
		Style style;
		style.name = e.attribute(QStringLiteral("name"));
		if (style.name.isEmpty() || find(styles, style.name)) continue;
		style.font = QETApp::diagramTextsFont();
		QETUtils::fontFromString(style.font, e.attribute(QStringLiteral("font")));
		style.color = QColor(e.attribute(QStringLiteral("color"), QStringLiteral("#000000")));
		style.target = e.attribute(QStringLiteral("target"));
		styles << style;
	}
	return styles;
}

/**
	@brief TextStyles::toXml
	Write styles as a <text_styles> child of project_root, only when there
	is one: a project without styles saves as before.
*/
void TextStyles::toXml(const QList<Style> &styles, QDomElement &project_root)
{
	if (styles.isEmpty()) return;
	QDomDocument document = project_root.ownerDocument();
	QDomElement root = document.createElement(QStringLiteral("text_styles"));
	for (const Style &style : styles)
	{
		QDomElement e = document.createElement(QStringLiteral("style"));
		e.setAttribute(QStringLiteral("name"), style.name);
		e.setAttribute(QStringLiteral("font"), QETUtils::fontToString(style.font));
		e.setAttribute(QStringLiteral("color"), style.color.name());
		if (!style.target.isEmpty())
			e.setAttribute(QStringLiteral("target"), style.target);
		root.appendChild(e);
	}
	project_root.appendChild(root);
}

TextStyles::SetStyleCommand::SetStyleCommand(QETProject *project,
					     const QList<DiagramTextItem *> &texts,
					     const QString &style, QUndoCommand *parent) :
	QUndoCommand(parent),
	m_project(project),
	m_style(style)
{
	for (DiagramTextItem *text : texts)
		m_saved << Saved{text, text->textStyle(), text->font(), text->color()};
	if (style.isEmpty())
		setText(QObject::tr("Textos seguem o estilo automático"));
	else if (style == NoStyle)
		setText(QObject::tr("Desligar textos do estilo"));
	else
		setText(QObject::tr("Aplicar o estilo de texto %1").arg(style));
}

void TextStyles::SetStyleCommand::undo()
{
	const QList<Style> styles = m_project ? m_project->textStyles() : QList<Style>();
	for (const Saved &saved : m_saved)
	{
		if (!saved.text) continue;
		saved.text->setTextStyle(saved.style);
		saved.text->setStyledLook(saved.font, saved.color);
		applyToText(styles, saved.text);
	}
}

void TextStyles::SetStyleCommand::redo()
{
	const QList<Style> styles = m_project ? m_project->textStyles() : QList<Style>();
	for (const Saved &saved : m_saved)
	{
		if (!saved.text) continue;
		saved.text->setTextStyle(m_style);
		applyToText(styles, saved.text);
	}
}
