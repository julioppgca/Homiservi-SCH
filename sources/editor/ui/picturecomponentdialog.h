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
#ifndef PICTURECOMPONENTDIALOG_H
#define PICTURECOMPONENTDIALOG_H

#include "../picturecomponent.h"
#include "../../properties/elementdata.h"

#include <QImage>
#include <QWizard>

class QCheckBox;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableWidget;

/**
	@brief The PictureComponentDialog class
	Wizard making the element being edited from a picture of the
	component (PictureComponent): the picture (a file, the clipboard or a
	page of a PDF), its terminals (read by the AI or clicked one by one),
	the contacts between them and the drawing, then the information.
*/
class PictureComponentDialog : public QWizard
{
	Q_OBJECT

	public:
		PictureComponentDialog(const ElementData &data, bool has_drawing, QWidget *parent = nullptr);

		QDomDocument drawing() const;
		bool replaceDrawing() const;
		QString name() const;
		QMap<QString, QString> information() const;

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override;

	private:
		QWizardPage *picturePage();
		QWizardPage *pinsPage();
		QWizardPage *partsPage();
		QWizardPage *dataPage();

		void openPicture();
		void pastePicture();
		void showPdfPage(int page);
		void setPicture(const QImage &image);
		void readWithAi();
		void editApiKey();
		void updateApiKeyButton();
		void addClickedPin(const QPointF &point);
		void removeSelectedPins();
		void clearPins();
		void guessParts();
		void addPart();
		void removeSelectedParts();

		void fillPins();
		void readPins();
		void fillParts();
		void readParts();
		void fillData();
		void updateMarkers();
		void updatePreview();
		void setBusy(bool busy, const QString &status = QString());

		ElementData m_data;
		bool m_has_drawing = false;
		PictureComponent::Result m_result;
		QImage m_picture;
		QString m_pdf_file;
		QNetworkAccessManager *m_network = nullptr;

		QLabel *m_picture_preview = nullptr;
		QLabel *m_picture_status = nullptr;
		QSpinBox *m_pdf_page = nullptr;

		QGraphicsScene *m_picture_scene = nullptr;
		QGraphicsView *m_picture_view = nullptr;
		QTableWidget *m_pins = nullptr;
		QPushButton *m_read_ai = nullptr;
		QPushButton *m_api_key = nullptr;
		QLabel *m_pins_status = nullptr;

		QTableWidget *m_parts = nullptr;
		QRadioButton *m_black_box = nullptr;
		QRadioButton *m_with_parts = nullptr;
		QLineEdit *m_body_text = nullptr;
		QGraphicsScene *m_preview_scene = nullptr;
		QGraphicsView *m_preview_view = nullptr;
		QLabel *m_parts_status = nullptr;

		QLineEdit *m_name = nullptr;
		QTableWidget *m_info = nullptr;
		QCheckBox *m_replace = nullptr;
};

#endif // PICTURECOMPONENTDIALOG_H
