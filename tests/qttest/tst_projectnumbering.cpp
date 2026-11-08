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

		void xmlRoundTrip()
		{
			Settings s = twoSections();
			s.format = Sequential;
			s.separator = QStringLiteral("-");
			s.section_digits = 1;
			s.page_digits = 3;
			s.strip_titles = false;
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
