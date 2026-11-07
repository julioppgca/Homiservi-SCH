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
#ifndef WIRINGEDITORDOCK_H
#define WIRINGEDITORDOCK_H

#include <QAbstractTableModel>
#include <QDockWidget>
#include <QPointer>

class Conductor;
class QCheckBox;
class QLabel;
class QETProject;
class QSortFilterProxyModel;
class QTableView;

/**
	@brief The WiringTableModel class
	One row per conductor of a project: folio, number, the two ends and
	the wire properties (function, colour, section, cable, voltage /
	protocol, bus). The wire properties are editable: setValues() writes
	one of them to several conductors, and by default to every conductor
	of their potentials, as one undo step.
*/
class WiringTableModel : public QAbstractTableModel
{
	Q_OBJECT

	public:
		enum Column {
			Folio, Number, From, To, Function, WireColor, Section,
			Cable, Voltage, Bus, ColumnCount
		};

		explicit WiringTableModel(QObject *parent = nullptr);

		void setProject(QETProject *project);
		QETProject *project() const {return m_project;}
		void reload();
		Conductor *conductor(int row) const;
		static bool isEditable(int column);

		bool setValues(const QList<int> &rows, int column, const QString &value,
			       bool whole_potential);

		int rowCount(const QModelIndex &parent = QModelIndex()) const override;
		int columnCount(const QModelIndex &parent = QModelIndex()) const override;
		QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
		QVariant headerData(int section, Qt::Orientation orientation,
				    int role = Qt::DisplayRole) const override;
		Qt::ItemFlags flags(const QModelIndex &index) const override;
		bool setData(const QModelIndex &index, const QVariant &value,
			     int role = Qt::EditRole) override;

		bool m_whole_potential = true;

	private:
		QPointer<QETProject> m_project;
		QList<QPointer<Conductor>> m_conductors;
};

/**
	@brief The WiringEditorDock class
	The wiring list of the current project as an editable table: sort and
	filter it, edit a cell for all the selected rows at once, double-click
	a folio or an end to show the wire on its folio, export it as CSV.
*/
class WiringEditorDock : public QDockWidget
{
	Q_OBJECT

	public:
		explicit WiringEditorDock(QWidget *parent = nullptr);
		void setProject(QETProject *project);

	private:
		void showConductor(const QModelIndex &proxy_index);
		void exportCsv();
		void updateSummary();

		WiringTableModel *m_model = nullptr;
		QSortFilterProxyModel *m_proxy = nullptr;
		QTableView *m_view = nullptr;
		QLabel *m_summary = nullptr;
		QCheckBox *m_whole_potential = nullptr;
		QMetaObject::Connection m_undo_connection;
};

#endif // WIRINGEDITORDOCK_H
