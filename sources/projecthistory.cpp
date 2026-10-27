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
#include "projecthistory.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace {
	const QString max_versions_key = QStringLiteral("projecthistory/max-versions");
	const int default_max_versions = 20;
		/// File names are the time the version was saved, so sorting
		/// them sorts the versions.
	const QString name_format = QStringLiteral("yyyyMMdd-HHmmss");

	QByteArray fileHash(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QByteArray();
		}
		QCryptographicHash hash(QCryptographicHash::Sha1);
		hash.addData(&file);
		return hash.result();
	}

		/// Versions of the history folder, newest first
	QFileInfoList versionFiles(const QString &dir)
	{
		QFileInfoList files = QDir(dir).entryInfoList(
					QDir::Files, QDir::Name | QDir::Reversed);
		files.erase(std::remove_if(files.begin(), files.end(),
					   [](const QFileInfo &info) {
			return !QDateTime::fromString(info.completeBaseName().left(15),
						      name_format).isValid();
		}), files.end());
		return files;
	}

	void prune(const QString &dir, int keep)
	{
		const QFileInfoList files = versionFiles(dir);
		for (int i = keep ; i < files.size() ; ++i) {
			QFile::remove(files.at(i).absoluteFilePath());
		}
	}
}

/**
	@brief ProjectHistory::maxVersions
	@return how many versions are kept for each project, 0 when the
	history is disabled
*/
int ProjectHistory::maxVersions()
{
	return std::max(0, QSettings().value(max_versions_key,
					     default_max_versions).toInt());
}

/**
	@brief ProjectHistory::setMaxVersions
	@param count : how many versions to keep for each project, 0 disables
	the history. Versions already stored are trimmed on the next save.
*/
void ProjectHistory::setMaxVersions(int count)
{
	QSettings().setValue(max_versions_key, std::max(0, count));
}

/**
	@brief ProjectHistory::historyDir
	@param project_path : path of the project file
	@return the folder holding the versions of this project. It is named
	after a hash of the path, which is also written to "source.txt" in it
	so the folder can be told apart by hand.
*/
QString ProjectHistory::historyDir(const QString &project_path)
{
	QString key = QDir::cleanPath(QFileInfo(project_path).absoluteFilePath());
#ifdef Q_OS_WIN
		//Same file whatever the case of its path
	key = key.toLower();
#endif
	const QString id = QString::fromLatin1(
				QCryptographicHash::hash(key.toUtf8(),
							 QCryptographicHash::Sha1)
				.toHex().left(16));
	return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
			+ QStringLiteral("/project-history/") + id;
}

/**
	@brief ProjectHistory::snapshot
	Copy the version of the project currently on disk to its history,
	unless it is the same as the last version stored, then drop the
	versions beyond maxVersions().
	Call it just before the project file is overwritten.
	@param project_path : path of the project file
	@return true if a version was added
*/
bool ProjectHistory::snapshot(const QString &project_path)
{
	const int max_versions = maxVersions();
	const QFileInfo project_info(project_path);
	if (max_versions == 0 || !project_info.isFile()) {
		return false;
	}

	const QString dir = historyDir(project_path);
	if (!QDir().mkpath(dir)) {
		return false;
	}

	QFile source(dir + QStringLiteral("/source.txt"));
	if (source.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		source.write(project_info.absoluteFilePath().toUtf8());
	}

	const QFileInfoList existing = versionFiles(dir);
	if (!existing.isEmpty()
	    && existing.first().size() == project_info.size()
	    && fileHash(existing.first().absoluteFilePath()) == fileHash(project_path)) {
		return false;
	}

		//Named after when this version was saved, not after now
	const QString base = project_info.lastModified().toString(name_format);
	const QString suffix = project_info.suffix().isEmpty()
			? QString()
			: QStringLiteral(".") + project_info.suffix();
	QString target = dir + QLatin1Char('/') + base + suffix;
	for (int i = 2 ; QFileInfo::exists(target) ; ++i) {
		target = dir + QLatin1Char('/') + base
				+ QStringLiteral("-%1").arg(i) + suffix;
	}

	const bool copied = QFile::copy(project_path, target);
	prune(dir, max_versions);
	return copied;
}

/**
	@brief ProjectHistory::entries
	@param project_path : path of the project file
	@return the versions stored for this project, newest first
*/
QList<ProjectHistory::Entry> ProjectHistory::entries(const QString &project_path)
{
	QList<Entry> list;
	for (const QFileInfo &info : versionFiles(historyDir(project_path))) {
		Entry entry;
		entry.file_path = info.absoluteFilePath();
		entry.saved_at = QDateTime::fromString(
					info.completeBaseName().left(15), name_format);
		entry.size = info.size();
		list << entry;
	}
	return list;
}

/**
	@brief ProjectHistory::restore
	Overwrite the project file with a version of its history. The version
	on disk is added to the history first, so the restore can itself be
	undone from the history. The project must not be open.
	@param entry : the version to restore
	@param project_path : path of the project file
	@param error_message : set when the restore fails
	@return true on success
*/
bool ProjectHistory::restore(const Entry &entry, const QString &project_path,
			     QString *error_message)
{
	QFile version(entry.file_path);
	if (!version.open(QIODevice::ReadOnly)) {
		if (error_message) *error_message = version.errorString();
		return false;
	}
	const QByteArray content = version.readAll();

	snapshot(project_path);

	QSaveFile target(project_path);
	if (!target.open(QIODevice::WriteOnly)
	    || target.write(content) != content.size()
	    || !target.commit()) {
		if (error_message) *error_message = target.errorString();
		return false;
	}
	return true;
}
