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
#ifndef DATASHEETDIALOG_H
#define DATASHEETDIALOG_H

#include "../datasheetreader.h"
#include "../../properties/elementdata.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QTableWidget;

/**
	@brief The DatasheetDialog class
	The datasheet of the element being edited: where it is (file or
	link), and what reading it gave (DatasheetReader) for the user to
	review before applying: the information to fill, row by row, and the
	pins to draw.
*/
class DatasheetDialog : public QDialog
{
	Q_OBJECT

	public:
		DatasheetDialog(const ElementData &data, bool has_drawing, QWidget *parent = nullptr);

		QString datasheet() const;
		QString name() const;
		QMap<QString, QString> information() const;
		bool drawPins() const;
		bool replaceDrawing() const;
		QList<DatasheetReader::Pin> pins() const;
		QString bodyText() const;

	private:
		enum Method { Automatic, Ai, Rules };

		void browse();
		void openDatasheet();
		void editApiKey();
		void read();
		void readBytes(const QByteArray &pdf);
		void readWithAi(const QByteArray &pdf);
		void readWithRules(const QByteArray &pdf, const QString &why);
		void showResult(const DatasheetReader::Result &result, const QString &status);
		void addPinRow(const DatasheetReader::Pin &pin);
		void setBusy(bool busy, const QString &status = QString());
		void updateApiKeyButton();

		ElementData m_data;
		QNetworkAccessManager *m_network;
		QLineEdit *m_datasheet;
		QComboBox *m_method;
		QPushButton *m_api_key;
		QPushButton *m_read;
		QLabel *m_status;
		QTableWidget *m_info;
		QTableWidget *m_pins;
		QCheckBox *m_draw;
		QCheckBox *m_replace;
		QPushButton *m_ok;
};

#endif // DATASHEETDIALOG_H
