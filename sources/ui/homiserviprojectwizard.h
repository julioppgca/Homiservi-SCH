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
#ifndef HOMISERVIPROJECTWIZARD_H
#define HOMISERVIPROJECTWIZARD_H

#include "../homiservitemplate.h"

#include <QWizard>

class QCheckBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;

/**
	@brief The HomiserviProjectWizard class
	Ask the data of a Homiservi project (HomiserviTemplate) in three steps:
	the project, the people and the revision, then the folios to create
	for a new project or how to apply the data to an existing one.
*/
class HomiserviProjectWizard : public QWizard
{
	Q_OBJECT

	public:
		enum Mode { NewProject, ExistingProject };

		HomiserviProjectWizard(Mode mode, const HomiserviTemplate::Data &data,
				       int folio_count = 0, QWidget *parent = nullptr);

		HomiserviTemplate::Data data() const;
		HomiserviTemplate::Folios folios() const;
		bool useTitleBlock() const;
		bool loadStyles() const;

		void accept() override;

	private:
		QWizardPage *projectPage(const HomiserviTemplate::Data &data);
		QWizardPage *peoplePage(const HomiserviTemplate::Data &data);
		QWizardPage *foliosPage();
		QWizardPage *applyPage(int folio_count);

		Mode m_mode;
		QLineEdit *m_title = nullptr;
		QLineEdit *m_client = nullptr;
		QLineEdit *m_plant = nullptr;
		QLineEdit *m_location = nullptr;
		QLineEdit *m_drawing_number = nullptr;
		QLineEdit *m_filename = nullptr;
		QLineEdit *m_author = nullptr;
		QLineEdit *m_checker = nullptr;
		QDateEdit *m_date = nullptr;
		QLineEdit *m_revision = nullptr;
		QCheckBox *m_cover = nullptr;
		QCheckBox *m_contents = nullptr;
		QCheckBox *m_materials = nullptr;
		QPlainTextEdit *m_schematics = nullptr;
		QCheckBox *m_use_title_block = nullptr;
		QCheckBox *m_load_styles = nullptr;
};

#endif // HOMISERVIPROJECTWIZARD_H
