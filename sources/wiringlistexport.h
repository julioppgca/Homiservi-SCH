#ifndef WIRINGLISTEXPORT_H
#define WIRINGLISTEXPORT_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QStringList>

class QETProject;
class QWidget;
class QDomElement;
class QDomNode;

struct ElementInfo {
    QString folio;
    QStringList links;
    QString label;
    QString name;
    bool isPlaceholder = false;
};

struct ConductorData {
    int index = 0;
    QString el1_uuid;
    QString el2_uuid;
    QString element1_label;
    QString element2_label;
    QString terminalname1;
    QString terminalname2;
    QString tension_protocol;
    QString conductor_color;
    QString conductor_section;
    QString function;
    QString cable;
    QString folio;
    int folio_index = 0; // position of the folio in the project, from 1
};

/**
 * @brief The WiringListExport class
 * Exports the wiring diagram from QElectroTech as a CSV file.
 */
class WiringListExport : public QObject
{
    Q_OBJECT
public:
    explicit WiringListExport(QETProject *project, QWidget *parent = nullptr);
    void toCsv();
    /**
        Build the wiring-list CSV and return it as a string (no GUI).
        Used by toCsv() and by the headless command-line export.
    */
    QString toCsvString() const;

    /**
        Open a print preview for the wire-end (ferrule) tag list: one row
        per conductor END (two rows per wire), each showing that end's own
        position and the target-designation tag to print/crimp there - the
        same "-<element><pin>" text shown on screen by ConductorEndLabeling.
    */
    void printTagList();
    /**
        Build the tag-list report as an HTML table and return it as a
        string (no GUI). Used by printTagList() and available for testing.
    */
    QString toTagListHtml() const;

    /**
        Save a minimal, single-column CSV with just the printed tag text -
        one row per conductor end, no other fields - ready to feed into
        ferrule/label-printer software (Phoenix Contact, Weidmüller,
        HellermannTyton, Brady...), which typically expect exactly this
        shape rather than the general documentation CSV from toCsv().
    */
    void toTagListCsv();
    /**
        Build the single-column tag CSV and return it as a string (no GUI).
        Used by toTagListCsv() and available for testing.
    */
    QString toTagListCsvString() const;

private:
    static QString formatTag(const QString &label, const QString &terminal);

    QETProject *m_project;
    QWidget *m_parent;

    QString normalizeUuid(const QString &u) const;
    QString findDiagramFolio(const QDomElement &diagramElem) const;
    QDomElement climbToDiagram(QDomNode node) const;

    QMap<QString, ElementInfo> collectElementsInfo(const QDomElement &root) const;
    QList<ConductorData> collectConductors(const QDomElement &root) const;
    /**
        Shared by toCsvString() and toTagListHtml(): parses the project XML,
        merges each pair of "half-wires" that cross a folio through a
        report/off-page connector into one logical conductor between the
        two real devices, and returns the resulting deduplicated, sorted
        list.
    */
    QList<ConductorData> buildUniqueConductorList() const;
};

#endif // WIRINGLISTEXPORT_H
