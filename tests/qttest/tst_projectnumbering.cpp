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
#include <QTest>

using namespace ProjectNumbering;

class tst_projectnumbering : public QObject
{
	Q_OBJECT

	private:
		static QList<FolioInfo> folios(const QStringList &numbers, const QStringList &titles = {})
		{
			QList<FolioInfo> list;
			for (int i = 0; i < numbers.size(); ++i)
				list << FolioInfo {QStringLiteral("{f%1}").arg(i), numbers.at(i), titles.value(i)};
			return list;
		}

		static ComponentInfo component(const QString &label, int folio, int column, qreal y,
					       const QString &section = QStringLiteral("02"),
					       const QString &folio_label = QStringLiteral("02.04"), bool terminal = false)
		{
			static int n = 0;
			ComponentInfo c;
			c.id = QString::number(++n);
			c.label = label;
			c.folio = folio;
			c.folio_label = folio_label;
			c.section = section;
			c.column = column;
			c.x = column * 50;
			c.y = y;
			c.terminal = terminal;
			return c;
		}

		static QStringList apply(const QList<ComponentInfo> &list, const QHash<QString, QString> &changes)
		{
			QStringList labels;
			for (const ComponentInfo &c : list)
				labels << changes.value(c.id, c.label);
			return labels;
		}

		static PotentialInfo potential(const QString &number, int folio, int column, qreal y,
					       const QString &folio_label = QStringLiteral("02.04"))
		{
			static int n = 0;
			PotentialInfo p;
			p.id = QStringLiteral("p%1").arg(++n);
			p.number = number;
			p.folio = folio;
			p.folio_label = folio_label;
			p.column = column;
			p.x = column * 50;
			p.y = y;
			return p;
		}

		static QStringList applyWires(const QList<PotentialInfo> &list, const QHash<QString, QString> &changes)
		{
			QStringList numbers;
			for (const PotentialInfo &p : list)
				numbers << changes.value(p.id, p.number);
			return numbers;
		}

		static Settings twoSections()
		{
			Settings s;
			s.enabled = true;
			s.sections = {{QStringLiteral("01"), QStringLiteral("Geral")},
				      {QStringLiteral("02"), QStringLiteral("Alimentação")}};
			return s;
		}

	private slots:
		void labelFormats()
		{
			Settings s;
			QCOMPARE(folioLabel(s, QStringLiteral("2"), 4, 9), QStringLiteral("02.04"));
			s.separator = QStringLiteral("-");
			QCOMPARE(folioLabel(s, QStringLiteral("02"), 12, 9), QStringLiteral("02-12"));
			s.separator.clear();
			s.page_digits = 3;
			QCOMPARE(folioLabel(s, QStringLiteral("2"), 4, 9), QStringLiteral("02004"));
				//A section that is not a number is kept as it is
			s.separator = QStringLiteral(".");
			QCOMPARE(folioLabel(s, QStringLiteral("A"), 1, 9), QStringLiteral("A.001"));
			s.format = Sequential;
			s.page_digits = 1;
			QCOMPARE(folioLabel(s, QStringLiteral("02"), 4, 9), QStringLiteral("9"));
		}

		void foliosInheritTheSectionOfThePreviousOne()
		{
			Settings s = twoSections();
			const QList<FolioInfo> list = folios({"a", "b", "c", "d", "e"});
			s.starts.insert(list.at(2).uuid, QStringLiteral("02"));
				//Before the first start: the first section
			QCOMPARE(sectionCodes(s, list), QStringList({"01", "01", "02", "02", "02"}));
			QCOMPARE(folioLabels(s, list), QStringList({"01.01", "01.02", "02.01", "02.02", "02.03"}));

				//A folio added in a section joins it, the next ones follow
			QList<FolioInfo> more = list;
			more.insert(1, FolioInfo {QStringLiteral("{new}"), QString(), QString()});
			QCOMPARE(folioLabels(s, more), QStringList({"01.01", "01.02", "01.03", "02.01", "02.02", "02.03"}));
		}

		void startsRoundTrip()
		{
			const QList<FolioInfo> list = folios({"a", "b", "c", "d"});
			const QStringList codes {"01", "02", "02", "01"};
			Settings s = twoSections();
			s.starts = startsOf(list, codes);
			QCOMPARE(s.starts.size(), 3);
			QCOMPARE(sectionCodes(s, list), codes);
				//Not grouped: section 01 comes back after 02
			QCOMPARE(folioLabels(s, codes), QStringList({"01.01", "02.01", "02.02", "01.02"}));
			QVERIFY(!isGrouped(s, codes));
			QCOMPARE(groupedOrder(s, codes), QList<int>({0, 3, 1, 2}));
		}

		void unknownSectionsGoLast()
		{
			const Settings s = twoSections();
			QCOMPARE(groupedOrder(s, {"09", "02", "01", "09"}), QList<int>({2, 1, 0, 3}));
			QVERIFY(isGrouped(s, {"01", "01", "02"}));
		}

		void strippedTitles()
		{
			QCOMPARE(strippedTitle(QStringLiteral("02.04 – Fonte 24 Vcc")), QStringLiteral("Fonte 24 Vcc"));
			QCOMPARE(strippedTitle(QStringLiteral("02-04 - Fonte")), QStringLiteral("Fonte"));
			QCOMPARE(strippedTitle(QStringLiteral("12.05: Lista FANUC")), QStringLiteral("Lista FANUC"));
			QCOMPARE(strippedTitle(QStringLiteral("Capa")), QStringLiteral("Capa"));
			QCOMPARE(strippedTitle(QStringLiteral("02.04 – ")), QStringLiteral("02.04 – "));
				//A voltage is not a folio number
			QCOMPARE(strippedTitle(QStringLiteral("24 - Vcc")), QStringLiteral("24 - Vcc"));
		}

		void importsAProjectNumberedByHand()
		{
				//As the PV500 FANUC project: a cover without number, then
				//numbers typed in the folio field and in the titles
			const QList<FolioInfo> list = folios(
					{"%id/%total", "01.02", "02.01", "02.02", "", "03.01"},
					{"Capa", "01.02 – Índice", "02.01 – Entrada",
					 "02.02 – Fonte", "02.03 – Transformador", "03.01 – CNC"});
			const QStringList texts {
				QStringLiteral("01 – GERAL | 02 – ALIMENTAÇÃO E DISTRIBUIÇÃO | 03 – CNC E OPERAÇÃO"),
				QStringLiteral("01.02 – Índice")};
			const Settings s = importFrom(list, texts);

			QVERIFY(s.enabled);
			QCOMPARE(s.separator, QStringLiteral("."));
			QCOMPARE(s.page_digits, 2);
			QCOMPARE(s.sections.size(), 3);
			QCOMPARE(s.sections.at(0), (Section {"01", "GERAL"}));
			QCOMPARE(s.sections.at(1), (Section {"02", "ALIMENTAÇÃO E DISTRIBUIÇÃO"}));
			QCOMPARE(s.sections.at(2), (Section {"03", "CNC E OPERAÇÃO"}));
				//The cover joins the first section: the numbers stay
			QCOMPARE(folioLabels(s, list),
				 QStringList({"01.01", "01.02", "02.01", "02.02", "02.03", "03.01"}));
		}

		void importsAProjectWithoutNumbers()
		{
			const Settings s = importFrom(folios({"%id/%total", "%id/%total"}), {});
			QCOMPARE(s.sections.size(), 1);
			QCOMPARE(s.sections.first().code, QStringLiteral("01"));
			QCOMPARE(folioLabels(s, folios({"x", "y"})), QStringList({"01.01", "01.02"}));
		}

		void tagPrefixes()
		{
			QCOMPARE(tagPrefix(QStringLiteral("-KA12")), QStringLiteral("-KA"));
			QCOMPARE(tagPrefix(QStringLiteral("-KA")), QStringLiteral("-KA"));
			QCOMPARE(tagPrefix(QStringLiteral("X0.0")), QString());
			QCOMPARE(tagPrefix(QStringLiteral("-A1-S1")), QString());
			QCOMPARE(tagPrefix(QStringLiteral("-X1:3")), QString());
		}

		void sequentialTagsInReadingOrder()
		{
			Settings s;
			s.tag_mode = TagsSequential;
				//Folio 1 before folio 0 in the list, column 3 before column 1
			const QList<ComponentInfo> list {
				component("-KA7", 1, 1, 10), component("-KA30", 0, 3, 10), component("-KA2", 0, 1, 50),
				component("-F12", 0, 1, 10), component("-KA", 0, 1, 20),
				component("X0.0", 0, 1, 30), component("-A1-S1", 0, 2, 10)};
			QCOMPARE(apply(list, componentLabels(s, list, RenumberAll)),
				 QStringList({"-KA04", "-KA03", "-KA02", "-F01", "-KA01", "X0.0", "-A1-S1"}));
		}

		void completeOnlyKeepsTheNumberedTags()
		{
			Settings s;
			s.tag_mode = TagsSequential;
			s.tag_digits = 1;
			const QList<ComponentInfo> list {
				component("-KA", 0, 1, 10), component("-KA7", 0, 2, 10), component("-KA", 1, 1, 10),
				component("-F", 0, 3, 10)};
			QCOMPARE(apply(list, componentLabels(s, list, CompleteOnly)),
				 QStringList({"-KA8", "-KA7", "-KA9", "-F1"}));
		}

		void tagsBySection()
		{
			Settings s;
			s.tag_mode = TagsBySection;
			const QList<ComponentInfo> list {
				component("-KA9", 0, 1, 10, "02"), component("-KA3", 1, 1, 10, "03"),
				component("-KA1", 1, 2, 10, "03"), component("-KA", 2, 1, 10, "03")};
			QCOMPARE(apply(list, componentLabels(s, list, RenumberAll)),
				 QStringList({"-KA0201", "-KA0301", "-KA0302", "-KA0303"}));
			const QList<ComponentInfo> done {
				component("-KA0301", 1, 1, 10, "03"), component("-KA0307", 1, 2, 10, "03"),
				component("-KA", 2, 1, 10, "03"), component("-KA", 0, 1, 10, "02")};
			QCOMPARE(apply(done, componentLabels(s, done, CompleteOnly)),
				 QStringList({"-KA0301", "-KA0307", "-KA0308", "-KA0201"}));
		}

		void tagsBySectionCompact()
		{
				//As the PV500 FANUC project: -KA62 is the 2nd -KA of section 06
			Settings s;
			s.tag_mode = TagsBySectionCompact;
			s.tag_digits = 1;
			const QList<ComponentInfo> list {
				component("-KA61", 0, 1, 10, "06"), component("-KA65", 0, 2, 10, "06"),
				component("-KA", 1, 1, 10, "06"), component("-F", 2, 1, 10, "11"),
					//Drawn in section 02 for section 08, on purpose
				component("-F83", 3, 1, 10, "02")};
			QCOMPARE(apply(list, componentLabels(s, list, CompleteOnly)),
				 QStringList({"-KA61", "-KA65", "-KA66", "-F111", "-F83"}));
			QCOMPARE(apply(list, componentLabels(s, list, RenumberAll)),
				 QStringList({"-KA61", "-KA62", "-KA63", "-F111", "-F21"}));
		}

		void guessesTheTagStyle()
		{
			int digits = 0;
			QCOMPARE(guessTagMode({component("-KA61", 0, 1, 1, "06"), component("-KA62", 0, 1, 2, "06"),
					       component("-B101", 0, 1, 3, "10"), component("-F83", 0, 1, 4, "02"),
					       component("X0.0", 0, 1, 5, "06")}, &digits), TagsBySectionCompact);
			QCOMPARE(digits, 1);
			QCOMPARE(guessTagMode({component("-KA0601", 0, 1, 1, "06"), component("-KA0602", 0, 1, 2, "06")},
					      &digits), TagsBySection);
			QCOMPARE(digits, 2);
			QCOMPARE(guessTagMode({component("-KA1", 0, 1, 1, "06"), component("-KA12", 0, 1, 2, "06")}),
				 TagsSequential);
			QCOMPARE(guessTagMode({component("X0.0", 0, 1, 1, "06"), component("-KA", 0, 1, 2, "06")}),
				 TagsUnchanged);
		}

		void tagsByPosition()
		{
			Settings s;
			s.tag_mode = TagsByPosition;
			const QList<ComponentInfo> list {
				component("-KA1", 0, 3, 10), component("-KA2", 0, 3, 40), component("-F1", 0, 3, 20),
				component("-Q1", 0, 5, 10, "02", "02.05")};
			QCOMPARE(apply(list, componentLabels(s, list, RenumberAll)),
				 QStringList({"-KA0204.3a", "-KA0204.3b", "-F0204.3", "-Q0205.5"}));
			const QList<ComponentInfo> more {component("-KA0204.3", 0, 3, 10), component("-KA", 0, 3, 40)};
			QCOMPARE(apply(more, componentLabels(s, more, CompleteOnly)),
				 QStringList({"-KA0204.3", "-KA0204.3a"}));
		}

		void terminalsByStrip()
		{
			Settings s;
			s.renumber_terminals = true;
			s.wire_mode = WiresByColumn;
			s.wire_digits = 3;
			s.wire_compact_folio = true;
			s.wire_separator = QStringLiteral(".");
			const QList<ComponentInfo> list {
				component("-X1:7", 0, 2, 10, "02", "02.04", true), component("-X1:3", 0, 1, 10, "02", "02.04", true),
				component("-X1:PE", 0, 1, 5, "02", "02.04", true), component("-X2:9", 0, 1, 20, "02", "02.04", true),
				component("-X1:", 1, 1, 10, "02", "02.04", true), component("-KA3", 0, 1, 10)};
				//The tags stay: tag_mode is TagsUnchanged
			QCOMPARE(apply(list, componentLabels(s, list, RenumberAll)),
				 QStringList({"-X1:2", "-X1:1", "-X1:PE", "-X2:1", "-X1:3", "-KA3"}));
			QCOMPARE(apply(list, componentLabels(s, list, CompleteOnly)),
				 QStringList({"-X1:7", "-X1:3", "-X1:PE", "-X2:9", "-X1:8", "-KA3"}));
		}

		void wireNumbersAndNames()
		{
			QVERIFY(isWireNumber(QStringLiteral("2000")));
			QVERIFY(isWireNumber(QStringLiteral("54.1")));
			QVERIFY(isWireNumber(QStringLiteral("02.04-01")));
			QVERIFY(isWireNumber(QStringLiteral("0204-3a")));
			QVERIFY(!isWireNumber(QStringLiteral("0V")));
			QVERIFY(!isWireNumber(QStringLiteral("+24V-DI")));
			QVERIFY(!isWireNumber(QStringLiteral("1L1")));
			QVERIFY(!isWireNumber(QStringLiteral("X1.2")));
			QVERIFY(!isWireNumber(QString()));
		}

		void wiresByFolio()
		{
			Settings s;
			s.wire_mode = WiresByFolio;
			const QList<PotentialInfo> list {
				potential("", 0, 3, 10), potential("4301", 0, 1, 10), potential("0V", 0, 1, 5),
				potential("+24V-DI", 0, 2, 10), potential("", 1, 1, 10, "02.05")};
				//The names stay, the numbers are given again in reading order
			QCOMPARE(applyWires(list, wireNumbers(s, list, RenumberAll)),
				 QStringList({"02.04-02", "02.04-01", "0V", "+24V-DI", "02.05-01"}));
				//Completing: the numbered ones stay
			QCOMPARE(applyWires(list, wireNumbers(s, list, CompleteOnly)),
				 QStringList({"02.04-01", "4301", "0V", "+24V-DI", "02.05-01"}));
			s.wire_compact_folio = true;
			s.wire_separator.clear();
			s.wire_digits = 1;
			const QList<PotentialInfo> more {potential("02041", 0, 1, 10), potential("", 0, 2, 10)};
			QCOMPARE(applyWires(more, wireNumbers(s, more, CompleteOnly)), QStringList({"02041", "02042"}));
		}

		void wiresSequentialAndByColumn()
		{
			Settings s;
			s.wire_mode = WiresSequential;
			s.wire_digits = 3;
			const QList<PotentialInfo> list {potential("", 1, 1, 10), potential("2000", 0, 1, 10),
							 potential("", 0, 1, 20)};
			QCOMPARE(applyWires(list, wireNumbers(s, list, CompleteOnly)), QStringList({"2002", "2000", "2001"}));
			QCOMPARE(applyWires(list, wireNumbers(s, list, RenumberAll)), QStringList({"003", "001", "002"}));

			s.wire_mode = WiresByColumn;
			const QList<PotentialInfo> columns {potential("", 0, 3, 10), potential("", 0, 3, 30),
							    potential("", 0, 4, 10), potential("02.04-4", 1, 4, 10)};
			QCOMPARE(applyWires(columns, wireNumbers(s, columns, CompleteOnly)),
				 QStringList({"02.04-3", "02.04-3a", "02.04-4a", "02.04-4"}));
			QCOMPARE(applyWires(columns, wireNumbers(s, columns, RenumberAll)),
				 QStringList({"02.04-3a", "02.04-3b", "02.04-4a", "02.04-4b"}));
		}

		void xmlRoundTrip()
		{
			Settings s = twoSections();
			s.format = Sequential;
			s.separator = QStringLiteral("-");
			s.section_digits = 1;
			s.page_digits = 3;
			s.strip_titles = false;
			s.tag_mode = TagsByPosition;
			s.tag_digits = 3;
			s.renumber_terminals = true;
			s.starts.insert(QStringLiteral("{b}"), QStringLiteral("02"));
			s.starts.insert(QStringLiteral("{a}"), QStringLiteral("01"));

			QDomDocument document;
			QDomElement root = document.createElement(QStringLiteral("project"));
			document.appendChild(root);
			toXml(s, root);
			QCOMPARE(fromXml(root), s);

				//Nothing to keep: nothing written, as before
			QDomElement empty = document.createElement(QStringLiteral("project"));
			toXml(Settings(), empty);
			QVERIFY(empty.firstChildElement().isNull());
			QCOMPARE(fromXml(empty), Settings());
		}
};

QTEST_GUILESS_MAIN(tst_projectnumbering)
#include "tst_projectnumbering.moc"
