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
#include "projectnumbering.h"

#include <QDomDocument>
#include <QDomElement>
#include <QRegularExpression>

#include <algorithm>
#include <numeric>

namespace
{
	const QString root_tag = QStringLiteral("project_numbering");

	QString padded(int number, int digits)
	{
		return QString::number(number).rightJustified(qMax(1, digits), QLatin1Char('0'));
	}

	bool isNumber(const QString &text)
	{
		static const QRegularExpression digits(QStringLiteral("^\\d+$"));
		return digits.match(text).hasMatch();
	}

		///"02.04" or "02-04" alone, as typed in the folio field
	const QRegularExpression &folioPattern()
	{
		static const QRegularExpression re(QStringLiteral("^\\s*(\\d+)\\s*([.\\-])\\s*(\\d+)\\s*$"));
		return re;
	}

		///"02.04 - Title" at the start of a title
	const QRegularExpression &titlePattern()
	{
		static const QRegularExpression re(
				QStringLiteral("^\\s*(\\d+)\\s*([.\\-])\\s*(\\d+)\\s*[\\x{2013}\\x{2014}\\-:]\\s*(.*)$"));
		return re;
	}
}

/**
	@return the index of the section numbered code, -1 if there is none
*/
int ProjectNumbering::Settings::sectionIndex(const QString &code) const
{
	for (int i = 0; i < sections.size(); ++i)
		if (sections.at(i).code == code)
			return i;
	return -1;
}

/**
	@brief ProjectNumbering::fromXml
	Read the <project_numbering> child of project_root.
*/
ProjectNumbering::Settings ProjectNumbering::fromXml(const QDomElement &project_root)
{
	Settings settings;
	const QDomElement root = project_root.firstChildElement(root_tag);
	if (root.isNull()) return settings;

	settings.enabled = root.attribute(QStringLiteral("enabled")) == QLatin1String("true");
	settings.format = root.attribute(QStringLiteral("format")) == QLatin1String("sequential")
			? Sequential : SectionPage;
	settings.separator = root.attribute(QStringLiteral("separator"), QStringLiteral("."));
	settings.section_digits = root.attribute(QStringLiteral("section_digits"), QStringLiteral("2")).toInt();
	settings.page_digits = root.attribute(QStringLiteral("page_digits"), QStringLiteral("2")).toInt();
	settings.strip_titles = root.attribute(QStringLiteral("strip_titles"), QStringLiteral("true"))
			== QLatin1String("true");

	for (QDomElement e = root.firstChildElement(QStringLiteral("section")); !e.isNull();
	     e = e.nextSiblingElement(QStringLiteral("section")))
		settings.sections << Section {e.attribute(QStringLiteral("code")), e.attribute(QStringLiteral("name"))};
	for (QDomElement e = root.firstChildElement(QStringLiteral("start")); !e.isNull();
	     e = e.nextSiblingElement(QStringLiteral("start")))
		settings.starts.insert(e.attribute(QStringLiteral("folio")), e.attribute(QStringLiteral("section")));
	return settings;
}

/**
	@brief ProjectNumbering::toXml
	Write settings as a <project_numbering> child of project_root, only when
	there is something to keep: a project without it saves as before.
*/
void ProjectNumbering::toXml(const Settings &settings, QDomElement &project_root)
{
	if (settings.isEmpty()) return;
	QDomDocument document = project_root.ownerDocument();
	QDomElement root = document.createElement(root_tag);
	root.setAttribute(QStringLiteral("enabled"), settings.enabled ? QStringLiteral("true") : QStringLiteral("false"));
	root.setAttribute(QStringLiteral("format"), settings.format == Sequential
			  ? QStringLiteral("sequential") : QStringLiteral("section_page"));
	root.setAttribute(QStringLiteral("separator"), settings.separator);
	root.setAttribute(QStringLiteral("section_digits"), settings.section_digits);
	root.setAttribute(QStringLiteral("page_digits"), settings.page_digits);
	root.setAttribute(QStringLiteral("strip_titles"), settings.strip_titles ? QStringLiteral("true") : QStringLiteral("false"));
	for (const Section &section : settings.sections) {
		QDomElement e = document.createElement(QStringLiteral("section"));
		e.setAttribute(QStringLiteral("code"), section.code);
		e.setAttribute(QStringLiteral("name"), section.name);
		root.appendChild(e);
	}
		//Sorted, so that saving twice gives the same file
	QStringList uuids = settings.starts.keys();
	uuids.sort();
	for (const QString &uuid : std::as_const(uuids)) {
		QDomElement e = document.createElement(QStringLiteral("start"));
		e.setAttribute(QStringLiteral("folio"), uuid);
		e.setAttribute(QStringLiteral("section"), settings.starts.value(uuid));
		root.appendChild(e);
	}
	project_root.appendChild(root);
}

/**
	@brief ProjectNumbering::sectionCodes
	@return the section of each folio: the one it starts, else the one of
	the folio before it. The folios before the first start are in the
	first section.
*/
QStringList ProjectNumbering::sectionCodes(const Settings &settings, const QList<FolioInfo> &folios)
{
	QStringList codes;
	QString current = settings.sections.isEmpty() ? QString() : settings.sections.first().code;
	for (const FolioInfo &folio : folios) {
		const auto it = settings.starts.constFind(folio.uuid);
		if (it != settings.starts.constEnd())
			current = it.value();
		codes << current;
	}
	return codes;
}

QStringList ProjectNumbering::folioLabels(const Settings &settings, const QList<FolioInfo> &folios)
{
	return folioLabels(settings, sectionCodes(settings, folios));
}

/**
	@brief ProjectNumbering::folioLabels
	@return the number of each folio, in the sections codes: its page
	counts the folios of its section before it.
*/
QStringList ProjectNumbering::folioLabels(const Settings &settings, const QStringList &codes)
{
	QStringList labels;
	QHash<QString, int> pages;
	for (int i = 0; i < codes.size(); ++i)
		labels << folioLabel(settings, codes.at(i), ++pages[codes.at(i)], i + 1);
	return labels;
}

/**
	@brief ProjectNumbering::folioLabel
	@param code : the section of the folio
	@param page : its rank in its section, from 1
	@param position : its rank in the project, from 1
	@return "02.04", or "38" for the sequential format or a folio without
	a section
*/
QString ProjectNumbering::folioLabel(const Settings &settings, const QString &code, int page, int position)
{
	if (settings.format == Sequential || code.isEmpty())
		return padded(position, settings.page_digits);
	const QString section = isNumber(code) ? padded(code.toInt(), settings.section_digits) : code;
	return section + settings.separator + padded(page, settings.page_digits);
}

/**
	@brief ProjectNumbering::startsOf
	@return the folios starting a section, for the section of every folio:
	the first folio and each one in another section than the folio before.
*/
QHash<QString, QString> ProjectNumbering::startsOf(const QList<FolioInfo> &folios, const QStringList &codes)
{
	QHash<QString, QString> starts;
	for (int i = 0; i < folios.size() && i < codes.size(); ++i)
		if (i == 0 || codes.at(i) != codes.at(i - 1))
			starts.insert(folios.at(i).uuid, codes.at(i));
	return starts;
}

/**
	@brief ProjectNumbering::groupedOrder
	@return the folio indexes in the order that puts the sections one after
	the other, in the order of settings.sections, keeping the order of the
	folios within a section. A section that is not in the list goes last.
*/
QList<int> ProjectNumbering::groupedOrder(const Settings &settings, const QStringList &codes)
{
	QList<int> order(codes.size());
	std::iota(order.begin(), order.end(), 0);
	auto rank = [&](int i) {
		const int index = settings.sectionIndex(codes.at(i));
		return index < 0 ? int(settings.sections.size()) : index;
	};
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return rank(a) < rank(b); });
	return order;
}

/**
	@return true if the folios already follow groupedOrder()
*/
bool ProjectNumbering::isGrouped(const Settings &settings, const QStringList &codes)
{
	const QList<int> order = groupedOrder(settings, codes);
	for (int i = 0; i < order.size(); ++i)
		if (order.at(i) != i)
			return false;
	return true;
}

/**
	@brief ProjectNumbering::strippedTitle
	@return title without the folio number it starts with:
	"02.04 - Fonte 24 Vcc" gives "Fonte 24 Vcc". A title made of the
	number only is kept.
*/
QString ProjectNumbering::strippedTitle(const QString &title)
{
	const QRegularExpressionMatch match = titlePattern().match(title);
	if (!match.hasMatch() || match.captured(4).trimmed().isEmpty())
		return title;
	return match.captured(4).trimmed();
}

/**
	@brief ProjectNumbering::importFrom
	Build settings from a project numbered by hand: the section of a folio
	is read from its number ("02.04") or from the start of its title
	("02.04 - ..."), a folio without one is in the section of the folio
	before it (the first ones in the first section found). The names of
	the sections are looked for in texts, written "02 - Alimentação": the
	list of the sections at the foot of the folios, for instance.
	@param folios : the folios of the project, in order
	@param texts : the texts of the folios
*/
ProjectNumbering::Settings ProjectNumbering::importFrom(const QList<FolioInfo> &folios, const QStringList &texts)
{
	Settings settings;
	settings.enabled = true;

	QStringList found;	//code of each folio, empty when not numbered
	bool separator_known = false;
	for (const FolioInfo &folio : folios) {
		QRegularExpressionMatch match = folioPattern().match(folio.folio);
		if (!match.hasMatch())
			match = titlePattern().match(folio.title);
		if (!match.hasMatch()) {
			found << QString();
			continue;
		}
		found << match.captured(1);
		if (!separator_known) {
			settings.separator = match.captured(2);
			settings.section_digits = qMax(1, int(match.captured(1).size()));
			settings.page_digits = qMax(1, int(match.captured(3).size()));
			separator_known = true;
		}
	}

	QString first;
	for (const QString &code : std::as_const(found))
		if (!code.isEmpty()) { first = code; break; }
	if (first.isEmpty())
		first = padded(1, settings.section_digits);

	QStringList codes;
	QString current = first;
	for (const QString &code : std::as_const(found)) {
		if (!code.isEmpty()) current = code;
		codes << current;
		if (settings.sectionIndex(current) < 0)
			settings.sections << Section {current, QString()};
	}
	if (settings.sections.isEmpty())
		settings.sections << Section {first, QString()};
	settings.starts = startsOf(folios, codes);

		//"02 - Alimentação e distribuição", not the end of "01.02 - Índice"
	static const QRegularExpression name_re(
			QStringLiteral("(?<![\\d.\\-])(\\d{1,3})\\s*[\\x{2013}\\x{2014}\\-]\\s*([^|\\n\\x{2029}]+)"));
	for (const QString &text : texts) {
		QRegularExpressionMatchIterator it = name_re.globalMatch(text);
		while (it.hasNext()) {
			const QRegularExpressionMatch match = it.next();
			for (Section &section : settings.sections) {
				if (section.name.isEmpty() && isNumber(section.code)
				    && section.code.toInt() == match.captured(1).toInt())
					section.name = match.captured(2).trimmed();
			}
		}
	}
	return settings;
}
