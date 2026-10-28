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
#ifndef TERMINALTABLEDIALOG_H
#define TERMINALTABLEDIALOG_H

#include <QDialog>
#include <QPointer>

class ElementScene;
class PartTerminal;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QUndoCommand;

/**
	@brief The TerminalTableDialog class
	Every terminal of the edited element in a table: name, position,
	orientation and type, editable, sortable, with a renaming in series of
	the selected rows. Selecting a row selects the terminal in the editor.
	undoCommand() gives the changes as one undo step.
*/
class TerminalTableDialog : public QDialog
{
	Q_OBJECT

	public:
		explicit TerminalTableDialog(ElementScene *scene, QWidget *parent = nullptr);
		QUndoCommand *undoCommand() const;

	private:
		PartTerminal *terminal(int row) const;
		void renameSelection();
		void checkNames();
		void selectInScene();

		QPointer<ElementScene> m_scene;
		QTableWidget *m_table = nullptr;
		QLineEdit *m_prefix = nullptr;
		QComboBox *m_numbering = nullptr;
		QSpinBox *m_start = nullptr;
		QSpinBox *m_step = nullptr;
		QLabel *m_status = nullptr;
};

#endif // TERMINALTABLEDIALOG_H
