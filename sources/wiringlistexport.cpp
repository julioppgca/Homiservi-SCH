#include "wiringlistexport.h"
#include "qetproject.h"
#include "diagram.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDomDocument>
#include <QFile>
#include <QTextDocument>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QPageLayout>
#include <algorithm>

WiringListExport::WiringListExport(QETProject *project, QWidget *parent) :
QObject(parent),
m_project(project),
m_parent(parent)
{
}

QString WiringListExport::normalizeUuid(const QString &u) const
{
    QString res = u;
    res.remove('{').remove('}');
    return res.trimmed().toLower();
}

QString WiringListExport::findDiagramFolio(const QDomElement &diagramElem) const
{
    if (diagramElem.isNull()) return "";
    // The folio number as the folio shows it ("3/12"). The saved "folio"
    // attribute is its template ("%id/%total").
    const int index = diagramElem.attribute("order").toInt();
    if (m_project && index >= 1 && index <= m_project->diagrams().size()) {
        return m_project->diagrams().at(index - 1)->border_and_titleblock
                .titleblockInformation().value("folio").toString();
    }
    if (diagramElem.hasAttribute("folio")) return diagramElem.attribute("folio");
    if (diagramElem.hasAttribute("title")) return diagramElem.attribute("title");
    return "";
}

QDomElement WiringListExport::climbToDiagram(QDomNode node) const
{
    while (!node.isNull()) {
        if (node.isElement() && node.toElement().tagName().toLower() == "diagram") {
            return node.toElement();
        }
        node = node.parentNode();
    }
    return QDomElement();
}

QMap<QString, ElementInfo> WiringListExport::collectElementsInfo(const QDomElement &root) const
{
    QMap<QString, ElementInfo> infoMap;

    QSet<QString> placeholderTypes;
    QDomElement collection = root.firstChildElement("collection");
    if (!collection.isNull()) {
        QDomNodeList defs = collection.elementsByTagName("definition");
        for (int i = 0; i < defs.size(); ++i) {
            QDomElement def = defs.at(i).toElement();
            QString ltype = def.attribute("link_type");
            if (ltype == "next_report" || ltype == "previous_report") {
                QDomElement parentEl = def.parentNode().toElement();
                if (parentEl.tagName().toLower() == "element") {
                    QString name = parentEl.attribute("name");
                    if (!name.isEmpty()) {
                        placeholderTypes.insert(name);
                    }
                }
            }
        }
    }

    QDomNodeList elements = root.elementsByTagName("element");
    for (int i = 0; i < elements.size(); ++i) {
        QDomElement el = elements.at(i).toElement();
        QString uuid = normalizeUuid(el.attribute("uuid", el.attribute("id", "")));
        if (uuid.isEmpty()) continue;

        ElementInfo info;
        info.folio = findDiagramFolio(climbToDiagram(el));

        QDomElement linksNode = el.firstChildElement("links_uuids");
        if (!linksNode.isNull()) {
            QDomNodeList linkUuids = linksNode.elementsByTagName("link_uuid");
            for (int j = 0; j < linkUuids.size(); ++j) {
                QString luuid = normalizeUuid(linkUuids.at(j).toElement().attribute("uuid"));
                if (!luuid.isEmpty()) info.links.append(luuid);
            }
        }

        QDomElement elInfoNode = el.firstChildElement("elementInformations");
        if (!elInfoNode.isNull()) {
            QDomNodeList eics = elInfoNode.elementsByTagName("elementInformation");
            for (int j = 0; j < eics.size(); ++j) {
                QDomElement eic = eics.at(j).toElement();
                QString nameAttr = eic.attribute("name").toLower();
                if (nameAttr == "label") info.label = eic.text().trimmed();
                if (nameAttr == "name") info.name = eic.text().trimmed();
            }
        }

        QString typeVal = el.attribute("type");
        info.isPlaceholder = false;
        for (const QString &ptype : placeholderTypes) {
            if (typeVal.endsWith(ptype)) {
                info.isPlaceholder = true;
                break;
            }
        }

        infoMap.insert(uuid, info);
    }
    return infoMap;
}

QList<ConductorData> WiringListExport::collectConductors(const QDomElement &root) const
{
    QList<ConductorData> conductors;
    QDomNodeList conductorNodes = root.elementsByTagName("conductor");

    for (int i = 0; i < conductorNodes.size(); ++i) {
        QDomElement cond = conductorNodes.at(i).toElement();

        if (cond.attribute("num") == "Brücke") continue;

        ConductorData data;
        data.index = i;
        data.el1_uuid = normalizeUuid(cond.attribute("element1", cond.attribute("element1id", "")));
        data.el2_uuid = normalizeUuid(cond.attribute("element2", cond.attribute("element2id", "")));

        data.element1_label = cond.attribute("element1_label");
        if (data.element1_label.isEmpty()) {
            data.element1_label = cond.attribute("element1_linked");
        }

        data.element2_label = cond.attribute("element2_label");
        if (data.element2_label.isEmpty()) {
            data.element2_label = cond.attribute("element2_linked");
        }

        data.terminalname1 = cond.attribute("terminalname1");
        data.terminalname2 = cond.attribute("terminalname2");
        data.tension_protocol = cond.attribute("tension_protocol");
        data.conductor_color = cond.attribute("conductor_color");
        data.conductor_section = cond.attribute("conductor_section");
        data.function = cond.attribute("function");
        data.cable = cond.attribute("cable");

        QDomElement diag = climbToDiagram(cond);
        data.folio = findDiagramFolio(diag);
        data.folio_index = diag.attribute("order").toInt();
        if (data.folio.isEmpty()) data.folio = cond.attribute("folio", cond.attribute("page", ""));

        conductors.append(data);
    }
    return conductors;
}

void WiringListExport::toCsv()
{
    if (!m_project) return;

    const QString csv = toCsvString();
    if (csv.isEmpty()) {
        QMessageBox::warning(m_parent, tr("Erreur"), tr("Impossible de lire la structure en mémoire du projet."));
        return;
    }

    QFileDialog dialog(m_parent);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setWindowTitle(tr("Exporter le plan de câblage"));
    dialog.setDefaultSuffix("csv");
    dialog.setNameFilter(tr("Fichiers CSV (*.csv)"));

    if (dialog.exec() != QDialog::Accepted) return;
    QString fileName = dialog.selectedFiles().first();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(m_parent, tr("Erreur"), tr("Impossible d'ouvrir le fichier pour l'écriture."));
        return;
    }
    QTextStream out(&file);
    out << csv;
    file.close();
    QMessageBox::information(m_parent, tr("Export réussi"), tr("Le plan de câblage a été exporté avec succès !"));
}

QList<ConductorData> WiringListExport::buildUniqueConductorList() const
{
    if (!m_project) return {};

    QDomDocument doc = m_project->toXml();
    if (doc.isNull()) return {};

    QSet<QString> conductorDefinitionTypes;
    QDomElement rootElem = doc.documentElement();
    QDomElement collection = rootElem.firstChildElement("collection");
    if (!collection.isNull()) {
        QDomNodeList defs = collection.elementsByTagName("definition");
        for (int i = 0; i < defs.size(); ++i) {
            QDomElement def = defs.at(i).toElement();
            if (def.attribute("link_type") == "conductor_definition") {
                QDomElement parentEl = def.parentNode().toElement();
                if (parentEl.tagName().toLower() == "element") {
                    QString name = parentEl.attribute("name");
                    if (!name.isEmpty()) {
                        conductorDefinitionTypes.insert(name);
                    }
                }
            }
        }
    }

    QSet<QString> conductorDefinitionUuids;
    QDomNodeList projectElements = rootElem.elementsByTagName("element");
    for (int i = 0; i < projectElements.size(); ++i) {
        QDomElement el = projectElements.at(i).toElement();
        QString typeVal = el.attribute("type");
        bool isCondDef = false;
        for (const QString &cType : conductorDefinitionTypes) {
            if (typeVal.endsWith(cType)) {
                isCondDef = true;
                break;
            }
        }
        if (isCondDef) {
            QString uuid = normalizeUuid(el.attribute("uuid", el.attribute("id", "")));
            if (!uuid.isEmpty()) {
                conductorDefinitionUuids.insert(uuid);
            }
        }
    }

    QMap<QString, ElementInfo> elementsInfo = collectElementsInfo(doc.documentElement());
    QList<ConductorData> conductors = collectConductors(doc.documentElement());

    QList<ConductorData> uniqueConductors;
    // Conductors with one end on a folio report, by that report. All of them
    // are collected before any is merged, so the result does not depend on
    // the order the folios are saved in.
    QMap<QString, QList<ConductorData>> partialWires;

    auto normalizePartial = [](ConductorData c, const QString &ph_uuid) {
        if (c.el1_uuid == ph_uuid) {
            std::swap(c.el1_uuid, c.el2_uuid);
            std::swap(c.element1_label, c.element2_label);
            std::swap(c.terminalname1, c.terminalname2);
        }
        return c;
    };

    auto mergeField = [](const QString &a, const QString &b) {
        QString at = a.trimmed();
        QString bt = b.trimmed();
        if (at.isEmpty()) return bt;
        if (bt.isEmpty()) return at;
        if (at == bt) return at;
        return at + ", " + bt;
    };

    for (int i = 0; i < conductors.size(); ++i) {
        ConductorData c = conductors[i];

        if (conductorDefinitionUuids.contains(c.el1_uuid) || conductorDefinitionUuids.contains(c.el2_uuid)) {
            continue;
        }

        if (c.element1_label.isEmpty() && elementsInfo.contains(c.el1_uuid)) {
            c.element1_label = elementsInfo[c.el1_uuid].label;
            if (c.element1_label.isEmpty()) c.element1_label = elementsInfo[c.el1_uuid].name;
        }
        if (c.element2_label.isEmpty() && elementsInfo.contains(c.el2_uuid)) {
            c.element2_label = elementsInfo[c.el2_uuid].label;
            if (c.element2_label.isEmpty()) c.element2_label = elementsInfo[c.el2_uuid].name;
        }

        bool el1_ph = elementsInfo.value(c.el1_uuid).isPlaceholder;
        bool el2_ph = elementsInfo.value(c.el2_uuid).isPlaceholder;

        if (!el1_ph && !el2_ph) {
            uniqueConductors.append(c);
            continue;
        }

        if (el1_ph && el2_ph) {
            uniqueConductors.append(c);
            continue;
        }

        QString ph_uuid = el1_ph ? c.el1_uuid : c.el2_uuid;
        partialWires[ph_uuid].append(normalizePartial(c, ph_uuid));
    }

    // Two halves are one wire only when each report of a linked pair carries
    // exactly one conductor. With several on a side, the diagram does not say
    // which terminal is wired to which, so each conductor gets its own row,
    // ending at the report.
    QSet<QString> written;
    for (auto it = partialWires.cbegin(); it != partialWires.cend(); ++it) {
        const QString &ph_uuid = it.key();
        if (written.contains(ph_uuid)) continue;
        written.insert(ph_uuid);

        const QStringList links = elementsInfo.value(ph_uuid).links;
        const QString matching_ph_uuid = links.isEmpty() ? QString() : links.first();
        const QList<ConductorData> others = partialWires.value(matching_ph_uuid);

        if (it.value().size() != 1 || others.size() != 1 || written.contains(matching_ph_uuid)) {
            uniqueConductors.append(it.value());
            continue;
        }
        written.insert(matching_ph_uuid);

        const ConductorData &otherHalf = it.value().first();
        const ConductorData &normC = others.first();

        ConductorData merged;
        merged.folio = mergeField(otherHalf.folio, normC.folio);
        merged.folio_index = std::min(otherHalf.folio_index, normC.folio_index);

        merged.el1_uuid = otherHalf.el1_uuid;
        merged.element1_label = otherHalf.element1_label;
        merged.terminalname1 = otherHalf.terminalname1;

        merged.el2_uuid = normC.el1_uuid;
        merged.element2_label = normC.element1_label;
        merged.terminalname2 = normC.terminalname1;

        merged.tension_protocol = mergeField(otherHalf.tension_protocol, normC.tension_protocol);
        merged.conductor_color = mergeField(otherHalf.conductor_color, normC.conductor_color);
        merged.conductor_section = mergeField(otherHalf.conductor_section, normC.conductor_section);
        merged.function = mergeField(otherHalf.function, normC.function);
        merged.cable = mergeField(otherHalf.cable, normC.cable);

        uniqueConductors.append(merged);
    }

    for (ConductorData &c : uniqueConductors) {
        if (!c.element2_label.isEmpty() && (c.element1_label.isEmpty() || c.element2_label.toLower() < c.element1_label.toLower())) {
            std::swap(c.element1_label, c.element2_label);
            std::swap(c.terminalname1, c.terminalname2);
            std::swap(c.el1_uuid, c.el2_uuid);
        }
    }

    std::sort(uniqueConductors.begin(), uniqueConductors.end(), [](const ConductorData &a, const ConductorData &b) {
        // By the folio's position in the project: its number is free text
        // ("3/12", "A-2") and does not sort.
        if (a.folio_index != b.folio_index) return a.folio_index < b.folio_index;

        int el1Cmp = a.element1_label.toLower().compare(b.element1_label.toLower());
        if (el1Cmp != 0) return el1Cmp < 0;

        int el2Cmp = a.element2_label.toLower().compare(b.element2_label.toLower());
        if (el2Cmp != 0) return el2Cmp < 0;

        int term1Cmp = a.terminalname1.compare(b.terminalname1);
        if (term1Cmp != 0) return term1Cmp < 0;

        return a.terminalname2 < b.terminalname2;
    });

    return uniqueConductors;
}

QString WiringListExport::toCsvString() const
{
    const QList<ConductorData> uniqueConductors = buildUniqueConductorList();
    if (uniqueConductors.isEmpty()) return QString();

    QString csv;
    QTextStream out(&csv);
    out << tr("Page", "Wiring list CSV header") << ";"
    << tr("Composant 1", "Wiring list CSV header") << ";"
    << tr("Borne 1", "Wiring list CSV header") << ";"
    << tr("Composant 2", "Wiring list CSV header") << ";"
    << tr("Borne 2", "Wiring list CSV header") << ";"
    << tr("Tension / Protocole", "Wiring list CSV header") << ";"
    << tr("Couleur du fil", "Wiring list CSV header") << ";"
    << tr("Section du fil", "Wiring list CSV header") << ";"
    << tr("Fonction", "Wiring list CSV header") << ";"
    << tr("Câble", "Wiring list CSV header") << "\n";

    for (const ConductorData &c : uniqueConductors) {
        out << c.folio << ";"
        << c.element1_label << ";"
        << c.terminalname1 << ";"
        << c.element2_label << ";"
        << c.terminalname2 << ";"
        << c.tension_protocol << ";"
        << c.conductor_color << ";"
        << c.conductor_section << ";"
        << c.function << ";"
        << c.cable << "\n";
    }

    return csv;
}

QString WiringListExport::formatTag(const QString &label, const QString &terminal)
{
    return QStringLiteral("-") + label + terminal;
}

QString WiringListExport::toTagListHtml() const
{
    const QList<ConductorData> uniqueConductors = buildUniqueConductorList();
    if (uniqueConductors.isEmpty()) return QString();

    QString html;
    QTextStream out(&html);
    out << "<html><head><style>"
           "body { font-family: sans-serif; font-size: 9pt; }"
           "h2 { font-size: 12pt; }"
           "table { border-collapse: collapse; width: 100%; }"
           "th, td { border: 1px solid #888888; padding: 3px 6px; text-align: left; }"
           "th { background-color: #dddddd; }"
           "</style></head><body>";
    out << "<h2>" << tr("Lista de marcação de fios") << " — " << m_project->title() << "</h2>";
    out << "<table><thead><tr>"
           "<th>" << tr("Folha") << "</th>"
           "<th>" << tr("Ponta (local)") << "</th>"
           "<th>" << tr("Marcação a imprimir") << "</th>"
           "<th>" << tr("Tensão/Protocolo") << "</th>"
           "<th>" << tr("Cor do fio") << "</th>"
           "<th>" << tr("Seção do fio") << "</th>"
           "<th>" << tr("Função") << "</th>"
           "</tr></thead><tbody>";

        // One row per conductor END: each row is what goes on the physical
        // ferrule/tag for that end - "local" says where this specific end
        // plugs in, "target" is the printed marking, i.e. the identifier of
        // the OTHER end (EPLAN "Ziel"/target-designation convention, same
        // as the on-screen labels from ConductorEndLabeling).
    for (const ConductorData &c : uniqueConductors) {
        const QString local1 = formatTag(c.element1_label, c.terminalname1);
        const QString local2 = formatTag(c.element2_label, c.terminalname2);

        out << "<tr><td>" << c.folio << "</td><td>" << local1 << "</td><td>" << local2 << "</td>"
            << "<td>" << c.tension_protocol << "</td><td>" << c.conductor_color << "</td>"
            << "<td>" << c.conductor_section << "</td><td>" << c.function << "</td></tr>";
        out << "<tr><td>" << c.folio << "</td><td>" << local2 << "</td><td>" << local1 << "</td>"
            << "<td>" << c.tension_protocol << "</td><td>" << c.conductor_color << "</td>"
            << "<td>" << c.conductor_section << "</td><td>" << c.function << "</td></tr>";
    }

    out << "</tbody></table></body></html>";
    return html;
}

void WiringListExport::printTagList()
{
    if (!m_project) return;

    const QString html = toTagListHtml();
    if (html.isEmpty()) {
        QMessageBox::warning(m_parent, tr("Erro"), tr("Não foi possível ler a estrutura do projeto, ou não há condutores para listar."));
        return;
    }

    QTextDocument document;
    document.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);

    QPrintPreviewDialog preview(&printer, m_parent);
    preview.setWindowTitle(tr("Lista de marcação de fios"));
    preview.resize(1000, 700);
    connect(&preview, &QPrintPreviewDialog::paintRequested, &preview, [&document](QPrinter *p) {
        document.print(p);
    });
    preview.exec();
}

QString WiringListExport::toTagListCsvString() const
{
    const QList<ConductorData> uniqueConductors = buildUniqueConductorList();
    if (uniqueConductors.isEmpty()) return QString();

    QString csv;
    QTextStream out(&csv);
    for (const ConductorData &c : uniqueConductors) {
        out << formatTag(c.element1_label, c.terminalname1) << "\n";
        out << formatTag(c.element2_label, c.terminalname2) << "\n";
    }
    return csv;
}

void WiringListExport::toTagListCsv()
{
    if (!m_project) return;

    const QString csv = toTagListCsvString();
    if (csv.isEmpty()) {
        QMessageBox::warning(m_parent, tr("Erro"), tr("Não foi possível ler a estrutura do projeto, ou não há condutores para listar."));
        return;
    }

    QFileDialog dialog(m_parent);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setWindowTitle(tr("Exportar tags para impressora de etiquetas"));
    dialog.setDefaultSuffix("csv");
    dialog.setNameFilter(tr("Arquivos CSV (*.csv)"));

    if (dialog.exec() != QDialog::Accepted) return;
    const QString fileName = dialog.selectedFiles().first();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(m_parent, tr("Erro"), tr("Não foi possível abrir o arquivo para escrita."));
        return;
    }
    QTextStream out(&file);
    out << csv;
    file.close();
    QMessageBox::information(m_parent, tr("Exportação concluída"), tr("As tags foram exportadas com sucesso!"));
}
