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
#ifndef PROJECTHISTORY_H
#define PROJECTHISTORY_H

#include <QDateTime>
#include <QList>
#include <QString>

/**
	@brief The ProjectHistory namespace
	Keeps the previous versions of a project file: each time the user
	saves, the version still on disk is copied to a folder of the
	application data (one folder per project path) before being
	overwritten, and only the most recent ones are kept.
	The autosave does not add versions, so it cannot push the user's own
	saves out of the history.
*/
namespace ProjectHistory
{
	struct Entry
	{
		QString file_path;	///< the copy in the history folder
		QDateTime saved_at;	///< when this version was written
		qint64 size = 0;
	};

	int maxVersions();
	void setMaxVersions(int count);

	QString historyDir(const QString &project_path);
	bool snapshot(const QString &project_path);
	QList<Entry> entries(const QString &project_path);
	bool restore(const Entry &entry, const QString &project_path,
		     QString *error_message = nullptr);
}

#endif // PROJECTHISTORY_H
