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

		///How the components are tagged: -KA01, -KA0201, -KA0204.3
	enum TagMode {
			///The tags stay as they are
		TagsUnchanged,
			///One counter per prefix through the project: -KA01, -KA02
		TagsSequential,
			///One counter per prefix in each section: -KA0201 (section 02)
		TagsBySection,
			///The same, the section without its zeros: -KA21 (section 02)
		TagsBySectionCompact,
			///The folio and the column of the component: -KA0204.3
		TagsByPosition
	};

		///How the control wires are numbered (a potential at a time)
	enum WireMode {
			///The wire numbers stay as they are
		WiresUnchanged,
			///One number through the project: 001, 002...
		WiresSequential,
			///The folio and a number in it: 02.04-01
		WiresByFolio,
			///The folio and the column the potential starts in: 02.04-3
		WiresByColumn
	};

		///Which tags applying gives a number
	enum TagScope {
			///Every tag
		RenumberAll,
			///Only the tags without a number ("-KA"), after the others
		CompleteOnly
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
		TagMode tag_mode = TagsUnchanged;
			///Zero padding of the number of a tag
		int tag_digits = 2;
			///Number the terminals of each strip too: -X1:1, -X1:2...
		bool renumber_terminals = false;
		WireMode wire_mode = WiresUnchanged;
		int wire_digits = 2;
			///The folio in a wire number without its separator: 0204
		bool wire_compact_folio = false;
			///Between the folio and the number of a wire
		QString wire_separator = QStringLiteral("-");
			///The number of a wire near the component it starts from,
			///instead of the middle of the wire
		bool wire_text_near_terminal = false;

		bool operator==(const Settings &other) const {
			return enabled == other.enabled && format == other.format
					&& separator == other.separator
					&& section_digits == other.section_digits
					&& page_digits == other.page_digits
					&& strip_titles == other.strip_titles
					&& sections == other.sections && starts == other.starts
					&& tag_mode == other.tag_mode && tag_digits == other.tag_digits
					&& renumber_terminals == other.renumber_terminals
					&& wire_mode == other.wire_mode && wire_digits == other.wire_digits
					&& wire_compact_folio == other.wire_compact_folio
					&& wire_separator == other.wire_separator
					&& wire_text_near_terminal == other.wire_text_near_terminal;
		}
		bool operator!=(const Settings &other) const { return !(*this == other); }

		bool isEmpty() const {
			return !enabled && sections.isEmpty() && tag_mode == TagsUnchanged && !renumber_terminals
					&& wire_mode == WiresUnchanged && !wire_text_near_terminal;
		}
		int sectionIndex(const QString &code) const;
	};

	struct FolioInfo
	{
		QString uuid;
			///The folio number in the title block
		QString folio;
		QString title;
	};

		///A component of a project to tag
	struct ComponentInfo
	{
			///Any key unique in the list
		QString id;
		QString label;
			///Rank of its folio in the project, from 0
		int folio = 0;
			///Number and section of its folio
		QString folio_label;
		QString section;
			///Column of the folio it is in, and its place, for the order
		int column = 0;
		qreal x = 0;
		qreal y = 0;
			///A terminal: -X1:12
		bool terminal = false;
	};

		///A potential of a project: the wires connected together, which
		///share a number
	struct PotentialInfo
	{
		QString id;
		QString number;
			///Where its first wire starts, in reading order
		int folio = 0;
		QString folio_label;
		int column = 0;
		qreal x = 0;
		qreal y = 0;
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
	QString tagPrefix(const QString &label);
	TagMode guessTagMode(const QList<ComponentInfo> &components, int *digits = nullptr);
	bool isWireNumber(const QString &number);
		///No number: empty, or "_", the text QElectroTech gives a new wire
	inline bool isNoWireNumber(const QString &number) {
		const QString trimmed = number.trimmed();
		return trimmed.isEmpty() || trimmed == QLatin1String("_");
	}
	QHash<QString, QString> wireNumbers(const Settings &settings,
					    const QList<PotentialInfo> &potentials,
					    TagScope scope);
	QHash<QString, QString> componentLabels(const Settings &settings,
						 const QList<ComponentInfo> &components,
						 TagScope scope);
	Settings importFrom(const QList<FolioInfo> &folios, const QStringList &texts);
}

#endif
