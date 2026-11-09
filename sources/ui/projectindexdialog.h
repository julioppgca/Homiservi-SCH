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
#ifndef PROJECTINDEXDIALOG_H
#define PROJECTINDEXDIALOG_H

#include "../projectindex.h"

#include <QDialog>
#include <QHash>

class Diagram;
class QetGraphicsTableItem;
class QGroupBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QRadioButton;
class QSqlQueryModel;
class QTableView;

/**
	@brief The ProjectIndexDialog class
	Homiservi-SCH: insert an index of the project (ProjectIndex): the folio
	index or the section index, its columns in their order, grouped by
	section or not, on the current folio or a new one, with a preview of
	its rows. "Avançado..." opens the table dialog of QElectroTech on the
	same query, for its appearance or a query of one's own.
*/
class ProjectIndexDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit ProjectIndexDialog(Diagram *current, QWidget *parent = nullptr);
		explicit ProjectIndexDialog(QetGraphicsTableItem *table, QWidget *parent = nullptr);

		static bool isIndex(QetGraphicsTableItem *table);
		static bool editIndex(QetGraphicsTableItem *table, QWidget *parent = nullptr);

		void accept() override;
		QString currentQuery() const;
		ProjectIndex::Kind kind() const;

	private:
		void fillColumns();
		QStringList checkedColumns() const;
		void updatePreview();
		void moveColumn(int delta);
		void advanced();
		Diagram *targetFolio();
		void saveChoices() const;
		void build(Diagram *current);
		void updateTable();
		QString baseName() const;

		Diagram *m_current = nullptr;
			///The first table of the index edited, null to insert one
		QetGraphicsTableItem *m_table = nullptr;
		QGroupBox *m_where_box = nullptr;
		QPushButton *m_advanced = nullptr;
		QRadioButton *m_folios = nullptr;
		QRadioButton *m_sections = nullptr;
		QRadioButton *m_revisions = nullptr;
		QListWidget *m_columns = nullptr;
		QCheckBox *m_grouped = nullptr;
		QCheckBox *m_adjust = nullptr;
		QCheckBox *m_more_folios = nullptr;
		QRadioButton *m_new_folio = nullptr;
		QRadioButton *m_current_folio = nullptr;
		QLineEdit *m_name = nullptr;
		QTableView *m_preview = nullptr;
		QSqlQueryModel *m_preview_model = nullptr;
		QLabel *m_preview_label = nullptr;
			///The columns checked of each kind, in their order
		QHash<int, QStringList> m_chosen;
		bool m_filling = false;
};

#endif
