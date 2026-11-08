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
#ifndef PROJECTNUMBERINGDIALOG_H
#define PROJECTNUMBERINGDIALOG_H

#include "../projectnumbering.h"

#include <QDialog>

class Conductor;
class Element;
class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QTabWidget;
class QSpinBox;
class QTableWidget;
class QETProject;
class ProjectView;
class QDialogButtonBox;
class QVBoxLayout;

/**
	@brief The ProjectNumberingDialog class
	Homiservi-SCH: the sections of a project and how its folios, the tags
	of its components and its wires are numbered (ProjectNumbering), one
	tab each, and what changes in the last one. Any tab can be edited
	alone: "Aplicar" applies everything as one undo step and keeps the
	dialog open on the project as it now is, "OK" applies and closes.
	Opened on the preview, it applies the settings of the project again.

	A section without folio gets one, with its name as title, after the
	folios of the sections before it. A project numbered by hand
	("02.04 - Title") gets its sections, and their names when a text of a
	folio lists them, read from it.
*/
class ProjectNumberingDialog : public QDialog
{
	Q_OBJECT

	public:
		enum Tab { SectionsTab, FoliosTab, ComponentsTab, WiresTab, PreviewTab };

		ProjectNumberingDialog(QETProject *project, ProjectView *project_view,
				       bool reapply, QWidget *parent = nullptr);

		bool apply();
		void accept() override;
		QTabWidget *tabs() const {return m_tabs;}

		ProjectNumbering::Settings settings() const;
			///The folio indexes in their new order
		QList<int> order() const;
			///The new title of every folio, in the current order
		QStringList titles() const;
			///The new number of every folio, in the new order
		QStringList labels() const;
			///The components whose tag changes, and their new tag
		QList<QPair<Element *, QString>> componentChanges() const;
			///The wires of every potential whose number changes, and its number
		QList<QPair<QList<Conductor *>, QString>> wireChanges() const;

	private:
		QWidget *sectionsPage();
		QWidget *foliosPage();
		QWidget *componentsPage();
		QWidget *wiresPage();
		QWidget *previewPage();
		QVBoxLayout *pageLayout(QWidget *page, const QString &title, const QString &subtitle);

		void load();
		void tabChanged(int index);
		bool validSections();
		void readAll();
		void refreshTab(int index);
		void createMissingFolios();
		QStringList missingSections() const;

		void importSections();
		void readSectionsTable();
		void fillSectionsTable();
		void fillFoliosTable();
		void updateNewNumbers();
		void readFormat();
		void setFolioSection(int row, const QString &code);
		void fillPreview();
		void readTagSettings();
		void fillPrefixTable();
		QList<ProjectNumbering::ComponentInfo> components() const;
		ProjectNumbering::TagScope tagScope() const;
		void readWireSettings();
		void fillWireSummary();
		QList<ProjectNumbering::PotentialInfo> potentials() const;
		ProjectNumbering::TagScope wireScope() const;
		QHash<QString, QString> wireNumbers(const QList<ProjectNumbering::PotentialInfo> &list) const;
		QStringList orderedCodes() const;
		QString sectionText(const ProjectNumbering::Section &section) const;

		QETProject *m_project;
		ProjectView *m_project_view;
		QTabWidget *m_tabs = nullptr;
		QDialogButtonBox *m_buttons = nullptr;
		QCheckBox *m_create_folios = nullptr;
		int m_last_tab = SectionsTab;
		QList<ProjectNumbering::FolioInfo> m_folios;
		QStringList m_texts;
		ProjectNumbering::Settings m_settings;
			///The section of every folio, in the current order
		QStringList m_codes;
			///The components that can be tagged, with the index of their
			///folio and what ProjectNumbering needs to know of them
		struct Component { Element *element; int folio; ProjectNumbering::ComponentInfo info; };
		QList<Component> m_components;
		struct Potential { QList<Conductor *> wires; int folio; ProjectNumbering::PotentialInfo info; };
		QList<Potential> m_potentials;
		bool m_reapply = false;
		bool m_has_revisions = false;

		QTableWidget *m_sections_table = nullptr;
		QTableWidget *m_folios_table = nullptr;
		QCheckBox *m_enabled = nullptr;
		QComboBox *m_format = nullptr;
		QSpinBox *m_section_digits = nullptr;
		QSpinBox *m_page_digits = nullptr;
		QCheckBox *m_strip_titles = nullptr;
		QCheckBox *m_group = nullptr;
		QLabel *m_summary = nullptr;
		QLabel *m_revision_warning = nullptr;
		QTabWidget *m_preview_tabs = nullptr;
		QTableWidget *m_preview_table = nullptr;
		QTableWidget *m_components_table = nullptr;
		QComboBox *m_tag_mode = nullptr;
		QSpinBox *m_tag_digits = nullptr;
		QCheckBox *m_terminals = nullptr;
		QRadioButton *m_renumber_all = nullptr;
		QRadioButton *m_complete_only = nullptr;
		QTableWidget *m_prefix_table = nullptr;
		QLabel *m_other_labels = nullptr;
		QComboBox *m_wire_mode = nullptr;
		QSpinBox *m_wire_digits = nullptr;
		QCheckBox *m_wire_compact = nullptr;
		QComboBox *m_wire_separator = nullptr;
		QCheckBox *m_wire_text_near = nullptr;
		QRadioButton *m_wires_all = nullptr;
		QRadioButton *m_wires_complete = nullptr;
		QLabel *m_wires_summary = nullptr;
		QTableWidget *m_wires_table = nullptr;
		bool m_filling = false;
};

#endif
