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
#ifndef TEXTSTYLES_H
#define TEXTSTYLES_H

#include <QColor>
#include <QDomElement>
#include <QFont>
#include <QList>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

class DiagramTextItem;
class QGraphicsItem;
class QETProject;

/**
	@brief The TextStyles namespace
	Named text styles of a project (font and colour), used like the layers
	of a CAD program: a text names its style and follows it, and a style
	can also take every text of a kind by itself (every label of element,
	every free text...). Changing a style changes all its texts, in every
	folio. Changing the font or colour of one text by hand frees it from
	its style.

	A text without a style of its own (DiagramTextItem::textStyle() empty)
	follows the style whose target matches it, if any; NoStyle keeps a text
	out of every style.
*/
namespace TextStyles
{
		///The style name of a text kept out of every style, even automatic
	extern const QString NoStyle;

	struct Style
	{
		QString name;
		QFont font;
		QColor color = Qt::black;
			///The texts the style takes by itself: empty (none), FreeTexts,
			///AllElementTexts or the name of an element information
			///("label", "function"...)
		QString target;

		bool operator==(const Style &other) const;
		bool operator!=(const Style &other) const { return !(*this == other); }
	};

	extern const QString FreeTexts;
	extern const QString AllElementTexts;

	QList<Style> examples();
	QStringList targets();
	QString targetName(const QString &target);
	QString fontDescription(const QFont &font);

	const Style *find(const QList<Style> &styles, const QString &name);
	QString effectiveStyle(const QList<Style> &styles, const DiagramTextItem *text);
	void applyToText(const QList<Style> &styles, DiagramTextItem *text);
	void applyToProject(QETProject *project);
	QList<DiagramTextItem *> styledTexts(const QList<QGraphicsItem *> &items);

	QList<Style> fromXml(const QDomElement &project_root);
	void toXml(const QList<Style> &styles, QDomElement &project_root);

	/**
		@brief The SetStyleCommand class
		Give texts a style (or NoStyle, or an empty name to follow the
		automatic styles again), undoable.
	*/
	class SetStyleCommand : public QUndoCommand
	{
		public:
			SetStyleCommand(QETProject *project, const QList<DiagramTextItem *> &texts,
					const QString &style, QUndoCommand *parent = nullptr);
			void undo() override;
			void redo() override;

		private:
			struct Saved
			{
				QPointer<DiagramTextItem> text;
				QString style;
				QFont font;
				QColor color;
			};
			QPointer<QETProject> m_project;
			QList<Saved> m_saved;
			QString m_style;
	};
}

#endif // TEXTSTYLES_H
