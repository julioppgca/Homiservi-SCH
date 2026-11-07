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
#include "elementtextstyles.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../qetapp.h"
#include "../utils/qetutils.h"
#include "graphicspart/partdynamictextfield.h"
#include "graphicspart/parttext.h"

#include <QCoreApplication>
#include <QSettings>
#include <QUndoCommand>

namespace {
	const QString settings_key = QStringLiteral("elementeditor/text_styles");

	QString tr(const char *text)
	{
		return QCoreApplication::translate("ElementTextStyles", text);
	}

	TextStyles::Style style(const QString &name, qreal size, bool bold, bool italic,
				const QColor &color)
	{
		TextStyles::Style style;
		style.name = name;
		style.font = QETApp::diagramTextsFont(size);
		style.font.setBold(bold);
		style.font.setItalic(italic);
		style.color = color;
		return style;
	}

		///Give text the font and colour of style and its name, under undo
	void applyTo(QGraphicsObject *text, const TextStyles::Style &style, QUndoCommand *undo)
	{
		const QFont font = text->property("font").value<QFont>();
		if (font != style.font)
			new QPropertyUndoCommand(text, "font", font, style.font, undo);
		const QColor color = text->property("color").value<QColor>();
		if (color != style.color)
			new QPropertyUndoCommand(text, "color", color, style.color, undo);
		const QString name = text->property("elementStyle").toString();
		if (name != style.name)
			new QPropertyUndoCommand(text, "elementStyle", name, style.name, undo);
	}
}

/**
	@return a starting set: the names of the terminals, the text of the
	box, the label of the element and notes
*/
QList<TextStyles::Style> ElementTextStyles::examples()
{
	return {
		style(tr("Nome do borne"), 7, false, false, Qt::black),
		style(tr("Texto da caixa"), 9, false, false, Qt::black),
		style(tr("Rótulo"), 9, true, false, Qt::black),
		style(tr("Notas"), 7, false, true, QColor(0x40, 0x40, 0x40))
	};
}

/**
	@return the styles kept in the settings, examples() the first time
*/
QList<TextStyles::Style> ElementTextStyles::load()
{
	QSettings settings;
	if (!settings.contains(settings_key + QStringLiteral("/size")))
		return examples();

	QList<TextStyles::Style> styles;
	const int size = settings.beginReadArray(settings_key);
	for (int i = 0; i < size; ++i) {
		settings.setArrayIndex(i);
		TextStyles::Style style;
		style.name = settings.value("name").toString();
		style.font = QETApp::diagramTextsFont(9);
		QETUtils::fontFromString(style.font, settings.value("font").toString());
		style.color = QColor(settings.value("color", "#000000").toString());
		if (!style.name.isEmpty())
			styles << style;
	}
	settings.endArray();
	return styles;
}

void ElementTextStyles::save(const QList<TextStyles::Style> &styles)
{
	QSettings settings;
	settings.remove(settings_key);
	settings.beginWriteArray(settings_key, styles.size());
	for (int i = 0; i < styles.size(); ++i) {
		settings.setArrayIndex(i);
		settings.setValue("name", styles.at(i).name);
		settings.setValue("font", QETUtils::fontToString(styles.at(i).font));
		settings.setValue("color", styles.at(i).color.name());
	}
	settings.endArray();
}

/**
	@return the texts of an element among items: static texts and
	dynamic text fields
*/
QList<QGraphicsObject *> ElementTextStyles::texts(const QList<QGraphicsItem *> &items)
{
	QList<QGraphicsObject *> texts;
	for (QGraphicsItem *item : items)
		if (item->type() == PartText::Type || item->type() == PartDynamicTextField::Type)
			texts << item->toGraphicsObject();
	return texts;
}

/**
	@return the undo giving texts the font, colour and name of style, or
	nullptr when there is nothing to change
*/
QUndoCommand *ElementTextStyles::apply(const TextStyles::Style &style, const QList<QGraphicsObject *> &texts)
{
	auto undo = new QUndoCommand(tr("Aplicar o estilo %1").arg(style.name));
	for (QGraphicsObject *text : texts)
		applyTo(text, style, undo);
	if (undo->childCount())
		return undo;
	delete undo;
	return nullptr;
}

/**
	@brief ElementTextStyles::update
	After the styles were edited: the texts naming a style take its new
	font and colour (and its new name, see renames, old name to new);
	those naming a style removed are freed of it.
	@return the undo, or nullptr when there is nothing to change
*/
QUndoCommand *ElementTextStyles::update(const QList<TextStyles::Style> &styles,
					const QHash<QString, QString> &renames,
					const QList<QGraphicsObject *> &texts)
{
	auto undo = new QUndoCommand(tr("Atualizar os estilos de texto"));
	for (QGraphicsObject *text : texts) {
		const QString name = text->property("elementStyle").toString();
		if (name.isEmpty())
			continue;
		if (const TextStyles::Style *style = TextStyles::find(styles, renames.value(name, name)))
			applyTo(text, *style, undo);
		else
			new QPropertyUndoCommand(text, "elementStyle", name, QString(), undo);
	}
	if (undo->childCount())
		return undo;
	delete undo;
	return nullptr;
}
