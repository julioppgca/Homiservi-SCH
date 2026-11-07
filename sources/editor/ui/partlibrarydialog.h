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
#ifndef PARTLIBRARYDIALOG_H
#define PARTLIBRARYDIALOG_H

#include "../partlibrary.h"

#include <QDialog>

class QFormLayout;
class QGraphicsScene;
class QGraphicsView;
class QTreeWidget;
class QWidget;

/**
	@brief The PartLibraryDialog class
	Choose a piece of PartLibrary, set its numbers and names while seeing
	it, then drawing() gives it to paste in the element.
*/
class PartLibraryDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit PartLibraryDialog(QWidget *parent = nullptr);

		QDomDocument drawing() const;

	protected:
		void showEvent(QShowEvent *event) override;

	private:
		void showEntry();
		void updatePreview();

		QList<PartLibrary::Entry> m_entries;
		QString m_id;
		QVariantMap m_values;
		QTreeWidget *m_tree = nullptr;
		QWidget *m_params = nullptr;
		QFormLayout *m_form = nullptr;
		QGraphicsScene *m_preview_scene = nullptr;
		QGraphicsView *m_preview = nullptr;
};

#endif // PARTLIBRARYDIALOG_H
