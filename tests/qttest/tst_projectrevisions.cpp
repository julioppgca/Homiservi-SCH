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
#include "projectrevisions.h"

#include <QDomDocument>
#include <QTest>

using ProjectRevisions::Revision;

namespace {
Revision revision(const QString &edition, const QStringList &folios = {},
		  const QHash<QString, QString> &snapshot = {})
{
	Revision r;
	r.edition = edition;
	r.description = QStringLiteral("Modificação ") + edition;
	r.modified_by = QStringLiteral("Homiservi");
	r.date = QStringLiteral("01/01/2026");
	r.folios = folios;
	r.snapshot = snapshot;
	return r;
}
}

class TestProjectRevisions : public QObject
{
	Q_OBJECT

	private slots:
		void nextEdition_data()
		{
			QTest::addColumn<QString>("edition");
			QTest::addColumn<QString>("next");
			QTest::newRow("empty") << QString() << QStringLiteral("A");
			QTest::newRow("A") << QStringLiteral("A") << QStringLiteral("B");
			QTest::newRow("lower") << QStringLiteral("c") << QStringLiteral("D");
			QTest::newRow("skip I") << QStringLiteral("H") << QStringLiteral("J");
			QTest::newRow("skip O") << QStringLiteral("N") << QStringLiteral("P");
			QTest::newRow("Z") << QStringLiteral("Z") << QStringLiteral("AA");
			QTest::newRow("AZ") << QStringLiteral("AZ") << QStringLiteral("BA");
			QTest::newRow("number") << QStringLiteral("0") << QStringLiteral("1");
			QTest::newRow("number 9") << QStringLiteral("9") << QStringLiteral("10");
		}
		void nextEdition()
		{
			QFETCH(QString, edition);
			QFETCH(QString, next);
			QCOMPARE(ProjectRevisions::nextEdition(edition), next);
		}

		void forFolio()
		{
				//Newest first: C changes f2, B changes f1, A is general
			const QList<Revision> list {revision("C", {"f2"}), revision("B", {"f1"}), revision("A")};
			auto editions = [](const QList<Revision> &l) {
				QStringList e; for (const Revision &r : l) e << r.edition; return e;
			};
			QCOMPARE(editions(ProjectRevisions::forFolio(list, "f1")), QStringList({"B", "A"}));
			QCOMPARE(editions(ProjectRevisions::forFolio(list, "f2")), QStringList({"C", "A"}));
			QCOMPARE(editions(ProjectRevisions::forFolio(list, "f3")), QStringList({"A"}));
				//No folio: the general revisions only
			QCOMPARE(editions(ProjectRevisions::forFolio(list, QString())), QStringList({"A"}));
		}

		void folioEdition()
		{
			const QList<Revision> list {revision("C", {"f2"}), revision("B", {"f1", "f2"}), revision("A")};
			QCOMPARE(ProjectRevisions::folioEdition(list, "f1"), QStringLiteral("B"));
			QCOMPARE(ProjectRevisions::folioEdition(list, "f2"), QStringLiteral("C"));
			QCOMPARE(ProjectRevisions::folioEdition(list, "f3"), QStringLiteral("A"));
			QCOMPARE(ProjectRevisions::folioEdition({revision("B", {"f1"})}, "f3"), QString());
			QCOMPARE(ProjectRevisions::folioEdition({}, "f1"), QString());
		}

		void affects()
		{
			QVERIFY(revision("A").affects("any"));
			QVERIFY(revision("A").isGeneral());
			QVERIFY(revision("B", {"f1"}).affects("f1"));
			QVERIFY(!revision("B", {"f1"}).affects("f2"));
			QVERIFY(Revision().isEmpty());
			QVERIFY(!revision("A").isEmpty());
		}

		void changedFolios()
		{
			const QHash<QString, QString> before {{"f1", "h1"}, {"f2", "h2"}, {"f3", "h3"}};
			const QList<Revision> list {revision("A", {}, before)};
				//f2 changed, f3 removed, f4 added
			const QHash<QString, QString> now {{"f1", "h1"}, {"f2", "x2"}, {"f4", "h4"}};
			QCOMPARE(ProjectRevisions::changedFolios(list, now), QStringList({"f2", "f4"}));
			QCOMPARE(ProjectRevisions::changedFolios(list, before), QStringList());
				//Nothing to compare with
			QCOMPARE(ProjectRevisions::changedFolios({}, now), QStringList());
			QCOMPARE(ProjectRevisions::changedFolios({revision("A")}, now), QStringList());
				//Only the newest revision counts
			const QList<Revision> two {revision("B", {"f2"}, now), revision("A", {}, before)};
			QCOMPARE(ProjectRevisions::changedFolios(two, now), QStringList());
		}

		void xmlRoundTrip()
		{
			const QList<Revision> list {
				revision("B", {"f2", "f1"}, {{"f1", "h1"}, {"f2", "h2"}}),
				revision("A")};
			QDomDocument document;
			QDomElement root = document.createElement("project");
			document.appendChild(root);
			ProjectRevisions::toXml(list, root);

			bool found = false;
			QCOMPARE(ProjectRevisions::fromXml(root, &found), list);
			QVERIFY(found);

				//Saving twice gives the same text
			QDomDocument again;
			QDomElement root2 = again.createElement("project");
			again.appendChild(root2);
			ProjectRevisions::toXml(ProjectRevisions::fromXml(root), root2);
			QCOMPARE(again.toString(), document.toString());

			QDomDocument empty;
			QDomElement bare = empty.createElement("project");
			QVERIFY(ProjectRevisions::fromXml(bare, &found).isEmpty());
			QVERIFY(!found);
		}
};

QTEST_GUILESS_MAIN(TestProjectRevisions)
#include "tst_projectrevisions.moc"
