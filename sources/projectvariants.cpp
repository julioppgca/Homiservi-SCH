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
#include "projectvariants.h"

#include "diagram.h"
#include "qetgraphicsitem/element.h"
#include "undocommand/changeelementinformationcommand.h"
#include "undocommand/changetitleblockcommand.h"

#include <QCoreApplication>
#include <QPointer>

const QString ProjectVariants::Key = QStringLiteral("variant");
const QString ProjectVariants::ApplicationKey = QStringLiteral("application");
const QString ProjectVariants::Standard = QStringLiteral("X");

namespace {
	QString tr(const char *text)
	{
		return QCoreApplication::translate("ProjectVariants", text);
	}

		///context with code as its variant, without it when code is empty
	DiagramContext withVariant(DiagramContext context, const QString &code)
	{
		if (code.isEmpty())
			context.remove(ProjectVariants::Key);
		else
			context.addValue(ProjectVariants::Key, code);
		return context;
	}
}

QList<ProjectVariants::Kind> ProjectVariants::kinds()
{
	return {{Standard, tr("Padrão")},
		{QStringLiteral("O"), tr("Opcional")},
		{QStringLiteral("V"), tr("Variante")},
		{QStringLiteral("E"), tr("Especial")}};
}

	///"X = PADRÃO   O = OPCIONAL...", to explain the codes under a table
QString ProjectVariants::legend()
{
	QStringList parts;
	for (const Kind &kind : kinds())
		parts << QStringLiteral("%1 = %2").arg(kind.code, kind.name.toUpper());
	return parts.join(QStringLiteral("    "));
}

/**
	@return the command giving elements the variant code (none when code
	is empty), nullptr when none of them changes
*/
QUndoCommand *ProjectVariants::elementsCommand(const QList<Element *> &elements, const QString &code)
{
	QMap<QPointer<Element>, QPair<DiagramContext, DiagramContext>> changes;
	for (Element *element : elements)
	{
		const DiagramContext old_info = element->elementInformations();
		const DiagramContext new_info = withVariant(old_info, code);
		if (new_info != old_info)
			changes.insert(element, qMakePair(old_info, new_info));
	}
	if (changes.isEmpty())
		return nullptr;
	auto command = new ChangeElementInformationCommand(changes);
	command->setText(code.isEmpty() ? tr("Remover o tipo de aplicação")
					: tr("Marcar como tipo %1").arg(code));
	return command;
}

/**
	@return the command giving the folio of diagram the variant code (none
	when code is empty), nullptr when it has it already
*/
QUndoCommand *ProjectVariants::folioCommand(Diagram *diagram, const QString &code)
{
	const TitleBlockProperties old_properties = diagram->border_and_titleblock.exportTitleBlock();
	TitleBlockProperties new_properties = old_properties;
	new_properties.context = withVariant(old_properties.context, code);
	if (new_properties.context == old_properties.context)
		return nullptr;
	auto command = new ChangeTitleBlockCommand(diagram, old_properties, new_properties);
	command->setText(code.isEmpty() ? tr("Remover o tipo de aplicação da folha")
					: tr("Marcar a folha como tipo %1").arg(code));
	return command;
}
