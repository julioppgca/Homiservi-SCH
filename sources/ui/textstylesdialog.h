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
#ifndef TEXTSTYLESDIALOG_H
#define TEXTSTYLESDIALOG_H

#include "../textstyles.h"

#include <QDialog>
#include <QHash>

class QLabel;
class QPushButton;
class QTableWidget;

/**
	@brief The TextStylesDialog class
	Edit the text styles of a project: name, font, colour and the texts
	each one takes by itself. renames() tells which styles were renamed,
	for the texts that name them.
*/
class TextStylesDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit TextStylesDialog(const QList<TextStyles::Style> &styles,
					  QWidget *parent = nullptr);

		QList<TextStyles::Style> styles() const;
		QHash<QString, QString> renames() const;
		void setForElements(const QList<TextStyles::Style> &examples);

	private:
		void readNames();
		void rebuild();
		void addStyle(const TextStyles::Style &style, const QString &origin = QString());
		void moveCurrentRow(int offset);
		void validate();

		QTableWidget *m_table = nullptr;
		QLabel *m_help = nullptr;
		QPushButton *m_examples_button = nullptr;
			///The styles "Usar exemplos" adds, TextStyles::examples() by default
		QList<TextStyles::Style> m_examples;
		QList<TextStyles::Style> m_styles;
			///The name each style had when the dialog opened, empty for a new one
		QStringList m_origins;
};

#endif // TEXTSTYLESDIALOG_H
