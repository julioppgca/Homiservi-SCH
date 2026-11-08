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
#include <QSet>

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

		///A tag: "-KA12", or its prefix alone "-KA"
	const QRegularExpression &tagPattern()
	{
		static const QRegularExpression re(QStringLiteral("^([-=+]?[A-Za-z]+)(\\d*)$"));
		return re;
	}

		///A terminal of a strip: "-X1:12", or "-X1:" to number
	const QRegularExpression &terminalPattern()
	{
		static const QRegularExpression re(QStringLiteral("^(.+):(\\d*)$"));
		return re;
	}

		///"a", "b"... "z", "aa"...
	QString letterSuffix(int index)
	{
		QString suffix;
		for (++index; index > 0; index = (index - 1) / 26)
			suffix.prepend(QChar(u'a' + (index - 1) % 26));
		return suffix;
	}

		///"02.04" -> "0204"
	QString compactFolio(const QString &folio)
	{
		QString compact;
		for (const QChar c : folio)
			if (c.isLetterOrNumber()) compact += c;
		return compact;
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
	const QString tags = root.attribute(QStringLiteral("tags"));
	settings.tag_mode = tags == QLatin1String("sequential") ? TagsSequential
			  : tags == QLatin1String("section") ? TagsBySection
			  : tags == QLatin1String("section_compact") ? TagsBySectionCompact
			  : tags == QLatin1String("position") ? TagsByPosition : TagsUnchanged;
	settings.tag_digits = root.attribute(QStringLiteral("tag_digits"), QStringLiteral("2")).toInt();
	settings.renumber_terminals = root.attribute(QStringLiteral("terminals")) == QLatin1String("true");
	const QString wires = root.attribute(QStringLiteral("wires"));
	settings.wire_mode = wires == QLatin1String("sequential") ? WiresSequential
			   : wires == QLatin1String("folio") ? WiresByFolio
			   : wires == QLatin1String("column") ? WiresByColumn : WiresUnchanged;
	settings.wire_digits = root.attribute(QStringLiteral("wire_digits"), QStringLiteral("2")).toInt();
	settings.wire_compact_folio = root.attribute(QStringLiteral("wire_compact_folio")) == QLatin1String("true");
	settings.wire_separator = root.attribute(QStringLiteral("wire_separator"), QStringLiteral("-"));
	settings.wire_text_near_terminal = root.attribute(QStringLiteral("wire_text")) == QLatin1String("near_terminal");

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
	static const QStringList tag_keys {QStringLiteral("unchanged"), QStringLiteral("sequential"),
					   QStringLiteral("section"), QStringLiteral("section_compact"),
					   QStringLiteral("position")};
	root.setAttribute(QStringLiteral("tags"), tag_keys.value(settings.tag_mode));
	root.setAttribute(QStringLiteral("tag_digits"), settings.tag_digits);
	root.setAttribute(QStringLiteral("terminals"), settings.renumber_terminals ? QStringLiteral("true") : QStringLiteral("false"));
	static const QStringList wire_keys {QStringLiteral("unchanged"), QStringLiteral("sequential"),
					    QStringLiteral("folio"), QStringLiteral("column")};
	root.setAttribute(QStringLiteral("wires"), wire_keys.value(settings.wire_mode));
	root.setAttribute(QStringLiteral("wire_digits"), settings.wire_digits);
	root.setAttribute(QStringLiteral("wire_compact_folio"), settings.wire_compact_folio ? QStringLiteral("true") : QStringLiteral("false"));
	root.setAttribute(QStringLiteral("wire_separator"), settings.wire_separator);
	root.setAttribute(QStringLiteral("wire_text"), settings.wire_text_near_terminal
			  ? QStringLiteral("near_terminal") : QStringLiteral("middle"));
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

/**
	@brief ProjectNumbering::tagPrefix
	@return the prefix of a tag: "-KA" for "-KA12" or "-KA", empty for a
	label that is not a prefix followed by a number (X0.0, -A1-S1, -X1:3)
*/
QString ProjectNumbering::tagPrefix(const QString &label)
{
	const QRegularExpressionMatch match = tagPattern().match(label.trimmed());
	return match.hasMatch() ? match.captured(1) : QString();
}

/**
	@brief ProjectNumbering::componentLabels
	Tag the components as settings say, in the reading order of the
	project: folio after folio, column after column, top to bottom.
	Only a label made of a prefix and a number ("-KA12") or of a prefix
	alone ("-KA") is a tag; any other label (an I/O address X0.0, -A1-S1)
	stays as it is. The terminals ("-X1:12") are numbered from 1 in each
	strip when settings.renumber_terminals, their other names (-X1:PE)
	kept.
	@param scope : RenumberAll numbers every tag; CompleteOnly keeps the
	numbered ones and gives the others the next free number
	@return the new label of every component whose label changes, by id
*/
QHash<QString, QString> ProjectNumbering::componentLabels(const Settings &settings,
							   const QList<ComponentInfo> &components,
							   TagScope scope)
{
	QHash<QString, QString> result;
	auto assign = [&result](const ComponentInfo &component, const QString &label) {
		if (label != component.label)
			result.insert(component.id, label);
	};

	QList<int> order(components.size());
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
		const ComponentInfo &ca = components.at(a), &cb = components.at(b);
		if (ca.folio != cb.folio) return ca.folio < cb.folio;
		if (ca.column != cb.column) return ca.column < cb.column;
		if (!qFuzzyCompare(ca.y, cb.y)) return ca.y < cb.y;
		return ca.x < cb.x;
	});
	const bool complete_only = scope == CompleteOnly;

	if (settings.renumber_terminals) {
		QHash<QString, int> last;
		if (complete_only)
			for (const ComponentInfo &component : components) {
				const QRegularExpressionMatch match = terminalPattern().match(component.label);
				if (component.terminal && match.hasMatch() && !match.captured(2).isEmpty())
					last[match.captured(1)] = qMax(last.value(match.captured(1)), match.captured(2).toInt());
			}
		for (int index : std::as_const(order)) {
			const ComponentInfo &component = components.at(index);
			const QRegularExpressionMatch match = terminalPattern().match(component.label);
			if (!component.terminal || !match.hasMatch()) continue;
			if (complete_only && !match.captured(2).isEmpty()) continue;
			assign(component, match.captured(1) + QLatin1Char(':') + QString::number(++last[match.captured(1)]));
		}
	}
	if (settings.tag_mode == TagsUnchanged) return result;

	struct Tag { int index; QString prefix; QString number; };
	QList<Tag> tags;
	for (int index : std::as_const(order)) {
		const ComponentInfo &component = components.at(index);
		const QRegularExpressionMatch match = tagPattern().match(component.label.trimmed());
		if (!component.terminal && match.hasMatch())
			tags << Tag {index, match.captured(1), match.captured(2)};
	}

	if (settings.tag_mode == TagsByPosition) {
		auto base = [&](const Tag &tag) {
			const ComponentInfo &component = components.at(tag.index);
			return tag.prefix + compactFolio(component.folio_label)
					+ QLatin1Char('.') + QString::number(component.column);
		};
		if (complete_only) {
			QSet<QString> taken;
			for (const ComponentInfo &component : components)
				taken.insert(component.label.trimmed());
			for (const Tag &tag : std::as_const(tags)) {
				if (!tag.number.isEmpty()) continue;
				QString label = base(tag);
				for (int i = 0; taken.contains(label); ++i)
					label = base(tag) + letterSuffix(i);
				taken.insert(label);
				assign(components.at(tag.index), label);
			}
			return result;
		}
			//Two components of a prefix in a column: a, b... in order
		QHash<QString, int> count, rank;
		for (const Tag &tag : std::as_const(tags))
			++count[base(tag)];
		for (const Tag &tag : std::as_const(tags)) {
			const QString b = base(tag);
			assign(components.at(tag.index), count.value(b) > 1 ? b + letterSuffix(rank[b]++) : b);
		}
		return result;
	}

	const bool by_section = settings.tag_mode == TagsBySection
				|| settings.tag_mode == TagsBySectionCompact;
		//The section as it opens the number: "02", or "2" when compact
	auto section_text = [&](const Tag &tag) {
		const QString section = components.at(tag.index).section;
		return settings.tag_mode == TagsBySectionCompact && isNumber(section)
				? QString::number(section.toInt()) : section;
	};
	auto key = [&](const Tag &tag) {
		return by_section ? tag.prefix + QLatin1Char('|') + section_text(tag) : tag.prefix;
	};
	auto head = [&](const Tag &tag) {
		return by_section ? tag.prefix + section_text(tag) : tag.prefix;
	};
	QHash<QString, int> last;
	if (complete_only)
		for (const Tag &tag : std::as_const(tags)) {
			if (tag.number.isEmpty()) continue;
			const QString section = section_text(tag);
			int number = tag.number.toInt();
			if (by_section) {
				if (!tag.number.startsWith(section) || tag.number.size() <= section.size()) continue;
				number = tag.number.mid(section.size()).toInt();
			}
			last[key(tag)] = qMax(last.value(key(tag)), number);
		}
	for (const Tag &tag : std::as_const(tags)) {
		if (complete_only && !tag.number.isEmpty()) continue;
		assign(components.at(tag.index), head(tag) + padded(++last[key(tag)], settings.tag_digits));
	}
	return result;
}

/**
	@brief ProjectNumbering::guessTagMode
	@return how the tags of components seem to be numbered: by section
	(compact, -KA62 in section 06, or not, -KA0602) when most of them start
	with the number of their section, else one counter per prefix when
	there are tags, else TagsUnchanged
	@param digits : set to the usual number of digits after the section
	(or of the whole number)
*/
ProjectNumbering::TagMode ProjectNumbering::guessTagMode(const QList<ComponentInfo> &components, int *digits)
{
	int tags = 0, compact = 0, padded_section = 0;
	QHash<int, int> lengths;
	for (const ComponentInfo &component : components) {
		const QRegularExpressionMatch match = tagPattern().match(component.label.trimmed());
		if (component.terminal || !match.hasMatch() || match.captured(2).isEmpty()) continue;
		++tags;
		const QString number = match.captured(2);
		const QString section = component.section;
		const QString short_section = isNumber(section) ? QString::number(section.toInt()) : section;
		if (!section.isEmpty() && number.startsWith(section) && number.size() > section.size()) {
			++padded_section;
			++lengths[int(number.size() - section.size())];
		} else if (!short_section.isEmpty() && number.startsWith(short_section)
			   && number.size() > short_section.size()) {
			++compact;
			++lengths[int(number.size() - short_section.size())];
		} else {
			++lengths[int(number.size())];
		}
	}
	int usual = 2, best = 0;
	for (auto it = lengths.constBegin(); it != lengths.constEnd(); ++it)
		if (it.value() > best) { best = it.value(); usual = it.key(); }
	if (digits) *digits = usual;
	if (!tags) return TagsUnchanged;
	if (padded_section * 10 >= tags * 7) return TagsBySection;
	if ((compact + padded_section) * 10 >= tags * 7) return TagsBySectionCompact;
	return TagsSequential;
}

/**
	@brief ProjectNumbering::isWireNumber
	@return true if number is a wire number, which the numbering may give
	again: digits with dots or dashes (2000, 54.1, 02.04-01), maybe ended
	by a letter (02.04-3a). A name (0V, +24V-DI, 1L1, X1.2) is not.
*/
bool ProjectNumbering::isWireNumber(const QString &number)
{
	static const QRegularExpression re(QStringLiteral("^\\d[\\d.\\-]*[a-z]?$"));
	return re.match(number.trimmed()).hasMatch();
}

/**
	@brief ProjectNumbering::wireNumbers
	Number the potentials as settings say, in reading order: folio after
	folio, column after column, top to bottom. A potential with a name
	(0V, +24V-DI, X1.2) keeps it; one without number, or with a wire number
	(isWireNumber()) when scope is RenumberAll, gets one.
	@return the new number of every potential whose number changes, by id
*/
QHash<QString, QString> ProjectNumbering::wireNumbers(const Settings &settings,
						      const QList<PotentialInfo> &potentials,
						      TagScope scope)
{
	QHash<QString, QString> result;
	if (settings.wire_mode == WiresUnchanged) return result;

	QList<int> order(potentials.size());
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
		const PotentialInfo &pa = potentials.at(a), &pb = potentials.at(b);
		if (pa.folio != pb.folio) return pa.folio < pb.folio;
		if (pa.column != pb.column) return pa.column < pb.column;
		if (!qFuzzyCompare(pa.y, pb.y)) return pa.y < pb.y;
		return pa.x < pb.x;
	});
	const bool complete_only = scope == CompleteOnly;
	auto numbered = [&](const PotentialInfo &potential) {
		const QString number = potential.number.trimmed();
		if (number.isEmpty()) return false;
		return complete_only || !isWireNumber(number);	//kept
	};
	auto folio_text = [&](const PotentialInfo &potential) {
		return settings.wire_compact_folio ? compactFolio(potential.folio_label) : potential.folio_label;
	};
	auto assign = [&result](const PotentialInfo &potential, const QString &number) {
		if (number != potential.number.trimmed())
			result.insert(potential.id, number);
	};

	if (settings.wire_mode == WiresByColumn) {
		auto base = [&](const PotentialInfo &potential) {
			return folio_text(potential) + settings.wire_separator + QString::number(potential.column);
		};
		QSet<QString> taken;
		for (const PotentialInfo &potential : potentials)
			if (numbered(potential)) taken.insert(potential.number.trimmed());
		if (complete_only) {
			for (int index : std::as_const(order)) {
				const PotentialInfo &potential = potentials.at(index);
				if (numbered(potential)) continue;
				QString number = base(potential);
				for (int i = 0; taken.contains(number); ++i)
					number = base(potential) + letterSuffix(i);
				taken.insert(number);
				assign(potential, number);
			}
			return result;
		}
		QHash<QString, int> count, rank;
		for (int index : std::as_const(order))
			if (!numbered(potentials.at(index))) ++count[base(potentials.at(index))];
		for (int index : std::as_const(order)) {
			const PotentialInfo &potential = potentials.at(index);
			if (numbered(potential)) continue;
			const QString b = base(potential);
			QString number = count.value(b) > 1 ? b + letterSuffix(rank[b]++) : b;
			while (taken.contains(number)) number = b + letterSuffix(rank[b]++);
			taken.insert(number);
			assign(potential, number);
		}
		return result;
	}

	const bool by_folio = settings.wire_mode == WiresByFolio;
	auto head = [&](const PotentialInfo &potential) {
		return by_folio ? folio_text(potential) + settings.wire_separator : QString();
	};
	QHash<QString, int> last;
	for (const PotentialInfo &potential : potentials) {
		if (!numbered(potential)) continue;
		const QString number = potential.number.trimmed();
		const QString h = head(potential);
		if (!number.startsWith(h)) continue;
		bool ok = false;
		const int n = number.mid(h.size()).toInt(&ok);
		if (ok) last[h] = qMax(last.value(h), n);
	}
	for (int index : std::as_const(order)) {
		const PotentialInfo &potential = potentials.at(index);
		if (numbered(potential)) continue;
		const QString h = head(potential);
		assign(potential, h + padded(++last[h], settings.wire_digits));
	}
	return result;
}
