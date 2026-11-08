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
#ifndef PROJECTNUMBERING_H
#define PROJECTNUMBERING_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QDomElement;

/**
	@brief The ProjectNumbering namespace
	Homiservi-SCH: the folios of a project grouped in sections ("02 -
	Alimentação e distribuição") and numbered by section: 02.01, 02.02,
	03.01... The settings are saved in the project, so the numbers are kept
	up to date when a folio is added, moved or removed, and can be applied
	again (ProjectNumberingWizard).

	A folio is in the section given to it, or else in the section of the
	folio before it: a section starts at the folio given to it and goes on
	until the next one starting a section, so a folio added in a section
	joins it. The settings only name the folios starting a section.

	The functions work on FolioInfo, the folios of a project in their
	order, and need no project: QETProject feeds them.
*/
namespace ProjectNumbering
{
	struct Section
	{
			///The section number as shown: "02"
		QString code;
			///"Alimentação e distribuição"
		QString name;

		bool operator==(const Section &other) const {
			return code == other.code && name == other.name;
		}
	};

	enum Format {
			///Section and page: 02.04
		SectionPage,
			///One number through the project: 1, 2, 3...
		Sequential
	};

	struct Settings
	{
			///The folio numbers are computed and kept up to date
		bool enabled = false;
		Format format = SectionPage;
			///Between the section and the page: ".", "-" or nothing
		QString separator = QStringLiteral(".");
			///Zero padding of a numeric section and of the page
		int section_digits = 2;
		int page_digits = 2;
			///Take "02.04 - " off the start of the titles when applying
		bool strip_titles = true;
		QList<Section> sections;
			///Uuid of the folios starting a section -> its code
		QHash<QString, QString> starts;

		bool operator==(const Settings &other) const {
			return enabled == other.enabled && format == other.format
					&& separator == other.separator
					&& section_digits == other.section_digits
					&& page_digits == other.page_digits
					&& strip_titles == other.strip_titles
					&& sections == other.sections && starts == other.starts;
		}
		bool operator!=(const Settings &other) const { return !(*this == other); }

		bool isEmpty() const { return !enabled && sections.isEmpty(); }
		int sectionIndex(const QString &code) const;
	};

	struct FolioInfo
	{
		QString uuid;
			///The folio number in the title block
		QString folio;
		QString title;
	};

	Settings fromXml(const QDomElement &project_root);
	void toXml(const Settings &settings, QDomElement &project_root);

	QStringList sectionCodes(const Settings &settings, const QList<FolioInfo> &folios);
	QStringList folioLabels(const Settings &settings, const QList<FolioInfo> &folios);
	QStringList folioLabels(const Settings &settings, const QStringList &codes);
	QString folioLabel(const Settings &settings, const QString &code, int page, int position);

	QHash<QString, QString> startsOf(const QList<FolioInfo> &folios, const QStringList &codes);
	QList<int> groupedOrder(const Settings &settings, const QStringList &codes);
	bool isGrouped(const Settings &settings, const QStringList &codes);

	QString strippedTitle(const QString &title);
	Settings importFrom(const QList<FolioInfo> &folios, const QStringList &texts);
}

#endif
