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
#include <QFileInfo>
#include <QSet>
#include <QTextStream>

namespace {
	const QString import_prefix = QStringLiteral("import/");

	QString definitionText(const QDomElement &definition)
	{
		QString text;
		QTextStream stream(&text);
		definition.save(stream, 0);
		return text;
	}
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

QList<ElementsLocation> ElementLibraryUpdate::updatableSymbols(const QList<Element *> &elements)
{
	QList<ElementsLocation> symbols;
	QSet<QString> seen;
	for (Element *element : elements)
	{
		const ElementsLocation location = element->location();
		const QString key = location.toString();
		if (seen.contains(key))
			continue;
		seen.insert(key);
		if (librarySource(location).exist())
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
	XmlElementCollection *collection = project->embeddedElementCollection();

	for (const ElementsLocation &symbol : symbols)
	{
		ElementsLocation source = librarySource(symbol);
		if (!source.exist())
			continue;
		const QDomElement new_definition = source.xml();
		const QDomElement old_definition = symbol.xml().cloneNode(true).toElement();
		if (new_definition.isNull())
			continue;

		QList<Element *> placed;
		for (Diagram *diagram : project->diagrams())
			for (Element *element : diagram->elements())
				if (element->location() == symbol)
					placed << element;

		if (definitionText(new_definition) == definitionText(old_definition)) {
			result.already_up_to_date << symbol.name();
			continue;
		}

			//A changed size, hotspot or terminal cannot be applied to the
			//placed elements (their wires hang on the terminals): leave
			//the copy of the project as it is, or the next opening would
			//drop the wires of the terminals that moved
		QStringList folios;
		for (Element *element : placed)
			if (!element->definitionGeometryMatches(new_definition))
				folios << element->diagram()->border_and_titleblock.finalfolio();
		if (!folios.isEmpty()) {
			folios.removeDuplicates();
			result.geometry_changed << QStringLiteral("%1 (%2)").arg(symbol.name(), folios.join(QStringLiteral(", ")));
			continue;
		}

			//Replace the copy of the project (the terminals keep their
			//uuid, the wires stay on them)
		ElementsLocation parent = symbol.parent();
		collection->copy(source, parent, symbol.fileName());
		const QDomElement definition = symbol.xml();
		if (definitionText(definition) == definitionText(old_definition))
			continue;       //the copy failed
		++result.symbols_updated;

		ElementPictureFactory::instance()->dropCache(symbol);
		for (Element *element : placed)
		{
			if (element->reloadPicture() == Element::ReloadPictureResult::Reloaded)
				++result.elements_redrawn;
			result.texts_changed += element->applyDefinitionTexts(old_definition, definition);
		}
	}

	if (result.symbols_updated)
		project->setModified(true);
	return result;
}
