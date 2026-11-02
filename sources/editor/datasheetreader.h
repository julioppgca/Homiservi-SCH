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
#ifndef DATASHEETREADER_H
#define DATASHEETREADER_H

#include "modulegenerator.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>
#include <QUrl>

/**
	@brief The DatasheetReader namespace
	Reads the datasheet (a PDF) of a component: the information of the
	element (manufacturer, reference, ratings, size...) and its pins.
	Two ways: rules on the text of the PDF, offline but only right on tidy
	datasheets, or the Claude API, which reads the PDF itself and needs an
	API key. Either way the result is a proposal the user reviews
	(DatasheetDialog) before it reaches the element; drawing() makes the
	box with the pins (ModuleGenerator).
*/
namespace DatasheetReader
{
	struct Pin
	{
		QString name;
		QString function;
		ModuleGenerator::Side side = ModuleGenerator::Left;
	};

	struct Result
	{
		QString name;	///< name of the component, for the element
		QMap<QString, QString> info;	///< element information key -> value
		QList<Pin> pins;
		bool isEmpty() const { return name.isEmpty() && info.isEmpty() && pins.isEmpty(); }
	};

	QStringList infoKeys();
	QUrl url(const QString &datasheet);
	bool isLink(const QString &datasheet);

	QString pdfText(const QString &file, QString *error);
	Result readByRules(const QString &text);

	QString apiKey();
	bool apiKeyFromEnvironment();
	void setApiKey(const QString &key);
	QNetworkRequest aiRequest();
	QByteArray aiRequestBody(const QByteArray &pdf);
	Result fromAiReply(const QByteArray &reply, QString *error);
	QByteArray aiBody(const QJsonObject &source, const QString &prompt, const QJsonObject &schema);
	QJsonObject aiData(const QByteArray &reply, QString *error);

	QString sideName(ModuleGenerator::Side side);
	ModuleGenerator::Settings drawing(const QList<Pin> &pins, const QString &body_text);
}

#endif // DATASHEETREADER_H
