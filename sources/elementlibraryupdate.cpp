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
#include "elementlibraryupdate.h"

#include "ElementsCollection/xmlelementcollection.h"
#include "diagram.h"
#include "factory/elementpicturefactory.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"

#include <QDateTime>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTextStream>

namespace {
	const QString import_prefix = QStringLiteral("import/");

		///project -> path of a symbol in its collection -> the definition
		///its placed elements were built with
	QHash<const QETProject *, QHash<QString, QString>> built_with;
		///set while update() applies the changes itself
	bool updating = false;

	QString definitionText(const QDomElement &definition)
	{
		QString text;
		QTextStream stream(&text);
		definition.save(stream, 0);
		return text;
	}

	QDomElement definitionFromText(const QString &text)
	{
		QDomDocument document;
		if (!document.setContent(text))
			return QDomElement();
		return document.documentElement();
	}

		///Every symbol of the collection, path -> definition
	void collect(const QDomElement &dir, const QString &path, QHash<QString, QString> &out)
	{
		for (QDomElement child = dir.firstChildElement() ; !child.isNull() ; child = child.nextSiblingElement())
		{
			const QString name = child.attribute(QStringLiteral("name"));
			const QString child_path = path.isEmpty() ? name : path + QLatin1Char('/') + name;
			if (child.tagName() == QLatin1String("category"))
				collect(child, child_path, out);
			else if (child.tagName() == QLatin1String("element"))
				out.insert(child_path, definitionText(child.firstChildElement(QStringLiteral("definition"))));
		}
	}

	QDomElement embeddedDefinition(QETProject *project, const QString &path)
	{
		return project->embeddedElementCollection()->element(path).firstChildElement(QStringLiteral("definition"));
	}

	QList<Element *> placedElements(QETProject *project, const QString &path)
	{
		QList<Element *> placed;
		for (Diagram *diagram : project->diagrams())
			for (Element *element : diagram->elements())
				if (element->location().collectionPath(false) == path)
					placed << element;
		return placed;
	}

		///Rebuild the elements of the symbol at path from its definition
		///in the project, the texts following the change from the
		///definition they were built with. Returns false when some
		///elements could not take it (their geometry changed).
	bool apply(QETProject *project, const QString &path, ElementLibraryUpdate::Result &result)
	{
		const QDomElement definition = embeddedDefinition(project, path);
		if (definition.isNull())
			return true;
		const QString text = definitionText(definition);
		QHash<QString, QString> &known = built_with[project];
		const QString old_text = known.value(path, text);
		if (old_text == text)
			return true;
		const QDomElement old_definition = definitionFromText(old_text);

		const QList<Element *> placed = placedElements(project, path);
		QStringList folios;
		for (Element *element : placed)
			if (!element->definitionGeometryMatches(definition))
				folios << element->diagram()->border_and_titleblock.finalfolio();
		if (!folios.isEmpty())
		{
			folios.removeDuplicates();
			result.geometry_changed << QStringLiteral("%1 (%2)").arg(
				path.section(QLatin1Char('/'), -1), folios.join(QStringLiteral(", ")));
			return false;
		}

		ElementPictureFactory::instance()->dropCache(placed.isEmpty()
			? ElementsLocation() : placed.first()->location());
		for (Element *element : placed)
		{
			if (element->reloadPicture() == Element::ReloadPictureResult::Reloaded)
				++result.elements_redrawn;
			result.texts_changed += element->applyDefinitionTexts(old_definition, definition);
		}
		known.insert(path, text);
		++result.symbols_updated;
		project->setModified(true);
		return true;
	}
}

void ElementLibraryUpdate::watch(QETProject *project)
{
	if (!project || built_with.contains(project))
		return;
	XmlElementCollection *collection = project->embeddedElementCollection();
	QHash<QString, QString> &known = built_with[project];
	collect(collection->root(), QString(), known);

	QObject::connect(project, &QObject::destroyed, [project]() { built_with.remove(project); });
	QObject::connect(collection, &XmlElementCollection::elementAdded, project, [project](const QString &path) {
		const QDomElement definition = embeddedDefinition(project, path);
		if (!definition.isNull())
			built_with[project].insert(path, definitionText(definition));
	});
		//The symbol editor saved the copy of the project: rebuild its
		//elements now (a change of geometry is left for update() to report)
	QObject::connect(collection, &XmlElementCollection::elementChanged, project, [project](const QString &path) {
		if (updating)
			return;
		Result ignored;
		apply(project, path, ignored);
	});
}

ElementsLocation ElementLibraryUpdate::librarySource(const ElementsLocation &embedded)
{
	if (!embedded.isProject() || !embedded.isElement())
		return ElementsLocation();
	QString path = embedded.collectionPath(false);
	if (!path.startsWith(import_prefix))
		return ElementsLocation();
	path.remove(0, import_prefix.size());

	ElementsLocation newest;
	QDateTime newest_time;
	for (const QString &protocol : {QStringLiteral("custom://"),
					QStringLiteral("company://"),
					QStringLiteral("common://")})
	{
		ElementsLocation location(protocol + path);
		if (!location.exist() || !location.isElement())
			continue;
		const QDateTime time = QFileInfo(location.fileSystemPath()).lastModified();
		if (newest_time.isNull() || time > newest_time) {
			newest = location;
			newest_time = time;
		}
	}
	return newest;
}

QList<ElementsLocation> ElementLibraryUpdate::symbolsOf(const QList<Element *> &elements)
{
	QList<ElementsLocation> symbols;
	QSet<QString> seen;
	for (Element *element : elements)
	{
		const ElementsLocation location = element->location();
		if (!location.isProject() || seen.contains(location.collectionPath(false)))
			continue;
		seen.insert(location.collectionPath(false));
		symbols << location;
	}
	return symbols;
}

ElementLibraryUpdate::Result ElementLibraryUpdate::update(QETProject *project,
							   const QList<ElementsLocation> &symbols)
{
	Result result;
	if (!project)
		return result;
	watch(project);
	XmlElementCollection *collection = project->embeddedElementCollection();
	updating = true;

	for (const ElementsLocation &symbol : symbols)
	{
		const QString path = symbol.collectionPath(false);
		const QString name = path.section(QLatin1Char('/'), -1);
		const QDomElement current = embeddedDefinition(project, path);
		if (current.isNull())
			continue;

			//A newer version in the library replaces the copy of the
			//project, unless its geometry does not fit the elements
			//placed (their wires hang on the terminals)
		ElementsLocation source = librarySource(symbol);
		if (source.exist())
		{
				//read here: ElementsLocation::xml() hands an element of a
				//document gone when it returns
			QDomDocument library_document;
			QFile file(source.fileSystemPath());
			if (file.open(QIODevice::ReadOnly | QIODevice::Text))
				library_document.setContent(&file);
			const QDomElement library = library_document.documentElement();
			if (!library.isNull() && definitionText(library) != definitionText(current))
			{
				QStringList folios;
				for (Element *element : placedElements(project, path))
					if (!element->definitionGeometryMatches(library))
						folios << element->diagram()->border_and_titleblock.finalfolio();
				if (!folios.isEmpty()) {
					folios.removeDuplicates();
					result.geometry_changed << QStringLiteral("%1 (%2)").arg(name, folios.join(QStringLiteral(", ")));
					continue;
				}
				ElementsLocation parent = symbol.parent();
				collection->copy(source, parent, symbol.fileName());
				result.from_library << name;
			}
		}

		const int before = result.symbols_updated;
		const int geometry = result.geometry_changed.size();
		apply(project, path, result);
		if (result.symbols_updated == before && result.geometry_changed.size() == geometry)
			result.already_up_to_date << name;
	}

	updating = false;
	return result;
}
