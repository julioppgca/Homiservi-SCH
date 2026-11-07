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
#include "picturecomponentdialog.h"

#include "../../qetapp.h"
#include "../../qetinformation.h"
#include "../datasheetreader.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#ifdef QET_HAS_QTPDF
#include <QPdfDocument>
#endif

#include <functional>
#include <utility>

namespace {
		///A wizard page whose Next button waits for complete()
	class Page : public QWizardPage
	{
		public:
			std::function<bool()> complete;
			bool isComplete() const override { return complete ? complete() : true; }
			void refresh() { emit completeChanged(); }
	};

	enum PageId { PicturePage, PinsPage, PartsPage, DataPage };
	enum PinColumn { PinName, PinSide, PinPosition, PinFunction };
	enum PartColumn { PartKindColumn, PartPinA, PartPinB, PartLabel, PartDrawn };

		///Longest side of the picture sent to the AI: larger is scaled down
		///by the API anyway
	constexpr int ai_picture_size = 1568;

	QComboBox *sideCombo(ModuleGenerator::Side side)
	{
		auto *combo = new QComboBox;
		for (int s = 0; s < ModuleGenerator::SideCount; ++s)
			combo->addItem(DatasheetReader::sideName(ModuleGenerator::Side(s)), s);
		combo->setCurrentIndex(combo->findData(int(side)));
		return combo;
	}

		///Draw the parts of an element definition on scene, for the preview
	void drawDefinition(QGraphicsScene *scene, const QDomDocument &document)
	{
		const QPen pen(Qt::black, 1);
		QPen dashed = pen;
		dashed.setStyle(Qt::DashLine);
		const QDomElement description = document.documentElement().firstChildElement(QStringLiteral("description"));
		for (QDomElement e = description.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
			auto number = [&e](const char *name) { return e.attribute(QString::fromLatin1(name)).toDouble(); };
			const QString tag = e.tagName();
			if (tag == QLatin1String("line")) {
				scene->addLine(number("x1"), number("y1"), number("x2"), number("y2"),
					       e.attribute(QStringLiteral("style")).contains(QLatin1String("dashed")) ? dashed : pen);
			} else if (tag == QLatin1String("rect")) {
				scene->addRect(number("x"), number("y"), number("width"), number("height"), pen);
			} else if (tag == QLatin1String("terminal")) {
				scene->addEllipse(number("x") - 1.5, number("y") - 1.5, 3, 3, QPen(Qt::red, 0.5), QBrush(Qt::red));
			} else if (tag == QLatin1String("text")) {
				QFont font;
				font.fromString(e.attribute(QStringLiteral("font")));
				auto *item = scene->addSimpleText(e.attribute(QStringLiteral("text")), font);
					//The part's position is the left end of its baseline
				const qreal ascent = QFontMetricsF(font).ascent();
				item->setTransformOriginPoint(0, ascent);
				item->setPos(number("x"), number("y") - ascent);
				item->setRotation(number("rotation"));
			}
		}
	}
}

PictureComponentDialog::PictureComponentDialog(const ElementData &data, bool has_drawing, QWidget *parent) :
	QWizard(parent),
	m_data(data),
	m_has_drawing(has_drawing),
	m_network(new QNetworkAccessManager(this))
{
	setWindowTitle(tr("Componente a partir de imagem"));
	setWizardStyle(QWizard::ModernStyle);
	setOption(QWizard::NoBackButtonOnStartPage);
	resize(1000, 680);

	setPage(PicturePage, picturePage());
	setPage(PinsPage, pinsPage());
	setPage(PartsPage, partsPage());
	setPage(DataPage, dataPage());
	setButtonText(QWizard::FinishButton, tr("Aplicar"));

		//Carry the edits of a page to the next one
	connect(this, &QWizard::currentIdChanged, this, [this](int id) {
		if (id == PinsPage) {
			updateMarkers();
		} else if (id == PartsPage) {
			readPins();
			if (m_result.parts.isEmpty())
				m_result.parts = PictureComponent::guessParts(m_result.pins);
			fillParts();
			updatePreview();
		} else if (id == DataPage) {
			readParts();
			fillData();
		}
	});
}

	///The drawing chosen, with the contacts or not
QDomDocument PictureComponentDialog::drawing() const
{
	PictureComponent::Result result = m_result;
	result.body_text = m_body_text->text().trimmed();
	return PictureComponent::build(result, m_with_parts->isChecked());
}

bool PictureComponentDialog::replaceDrawing() const
{
	return m_has_drawing && m_replace->isChecked();
}

QString PictureComponentDialog::name() const
{
	return m_name->text().trimmed();
}

	///The information whose value the user changed
QMap<QString, QString> PictureComponentDialog::information() const
{
	QMap<QString, QString> information;
	for (int row = 0; row < m_info->rowCount(); ++row) {
		const QString key = m_info->item(row, 0)->data(Qt::UserRole).toString();
		const QString value = m_info->item(row, 1)->text().trimmed();
		if (value != m_data.m_informations.value(key).toString())
			information.insert(key, value);
	}
	return information;
}

QWizardPage *PictureComponentDialog::picturePage()
{
	auto *page = new Page;
	page->setTitle(tr("Imagem do componente"));
	page->setSubTitle(tr("O diagrama de ligação do componente, como impresso nele ou no datasheet:"
			     " um arquivo de imagem, um recorte colado (Ctrl+V) ou uma página de um PDF."));
	page->complete = [this]() { return !m_picture.isNull(); };

	auto *open = new QPushButton(tr("Abrir imagem ou PDF..."), page);
	auto *paste = new QPushButton(tr("Colar (Ctrl+V)"), page);
	paste->setShortcut(QKeySequence::Paste);
	m_pdf_page = new QSpinBox(page);
	m_pdf_page->setPrefix(tr("Página "));
	m_pdf_page->setVisible(false);
	connect(open, &QPushButton::clicked, this, &PictureComponentDialog::openPicture);
	connect(paste, &QPushButton::clicked, this, &PictureComponentDialog::pastePicture);
	connect(m_pdf_page, qOverload<int>(&QSpinBox::valueChanged), this, [this](int page) { showPdfPage(page - 1); });

	auto *buttons = new QHBoxLayout;
	buttons->addWidget(open);
	buttons->addWidget(paste);
	buttons->addWidget(m_pdf_page);
	buttons->addStretch();

	m_picture_preview = new QLabel(page);
	m_picture_preview->setAlignment(Qt::AlignCenter);
	m_picture_preview->setMinimumHeight(300);
	m_picture_preview->setFrameShape(QFrame::StyledPanel);
	m_picture_status = new QLabel(tr("Nenhuma imagem ainda."), page);
	m_picture_status->setWordWrap(true);

	auto *layout = new QVBoxLayout(page);
	layout->addLayout(buttons);
	layout->addWidget(m_picture_preview, 1);
	layout->addWidget(m_picture_status);
	return page;
}

QWizardPage *PictureComponentDialog::pinsPage()
{
	auto *page = new Page;
	page->setTitle(tr("Bornes"));
	page->setSubTitle(tr("Leia os bornes com a IA ou clique em cada borne na imagem e digite o nome."
			     " Bornes de cima e de baixo na mesma posição ficam alinhados no desenho."));
	page->complete = [this]() { return m_pins->rowCount() > 0; };

	m_picture_scene = new QGraphicsScene(this);
	m_picture_view = new QGraphicsView(m_picture_scene, page);
	m_picture_view->setRenderHint(QPainter::SmoothPixmapTransform);
	m_picture_view->setCursor(Qt::CrossCursor);
	m_picture_view->viewport()->installEventFilter(this);

	m_pins = new QTableWidget(0, 4, page);
	m_pins->setHorizontalHeaderLabels({tr("Borne"), tr("Lado"), tr("Posição"), tr("Função")});
	m_pins->horizontalHeader()->setStretchLastSection(true);
	m_pins->verticalHeader()->setVisible(false);
	m_pins->setSelectionBehavior(QAbstractItemView::SelectRows);
	connect(m_pins->model(), &QAbstractItemModel::rowsInserted, page, &Page::refresh);
	connect(m_pins->model(), &QAbstractItemModel::rowsRemoved, page, &Page::refresh);

	m_read_ai = new QPushButton(tr("Ler com IA"), page);
	m_read_ai->setToolTip(tr("Envia a imagem para a API Claude da Anthropic; cada leitura tem um custo."));
	m_api_key = new QPushButton(page);
	auto *remove = new QPushButton(tr("Remover"), page);
	auto *clear = new QPushButton(tr("Limpar"), page);
	connect(m_read_ai, &QPushButton::clicked, this, &PictureComponentDialog::readWithAi);
	connect(m_api_key, &QPushButton::clicked, this, &PictureComponentDialog::editApiKey);
	connect(remove, &QPushButton::clicked, this, &PictureComponentDialog::removeSelectedPins);
	connect(clear, &QPushButton::clicked, this, &PictureComponentDialog::clearPins);
	updateApiKeyButton();

	auto *buttons = new QHBoxLayout;
	buttons->addWidget(m_read_ai);
	buttons->addWidget(m_api_key);
	buttons->addStretch();
	buttons->addWidget(remove);
	buttons->addWidget(clear);

	m_pins_status = new QLabel(page);
	m_pins_status->setWordWrap(true);

	auto *right = new QVBoxLayout;
	right->addLayout(buttons);
	right->addWidget(m_pins, 1);
	right->addWidget(m_pins_status);

	auto *layout = new QHBoxLayout(page);
	layout->addWidget(m_picture_view, 3);
	layout->addLayout(right, 2);
	return page;
}

QWizardPage *PictureComponentDialog::partsPage()
{
	auto *page = new Page;
	page->setTitle(tr("Contatos e desenho"));
	page->setSubTitle(tr("Os contatos e bobinas entre dois bornes, sugeridos pela numeração"
			     " (13-14 NA, 11-12 NF, A1-A2 bobina). Confira e escolha o desenho."));

	m_parts = new QTableWidget(0, 5, page);
	m_parts->setHorizontalHeaderLabels({tr("Tipo"), tr("Borne 1"), tr("Borne 2"), tr("Rótulo"), tr("Desenho")});
	m_parts->horizontalHeader()->setStretchLastSection(true);
	m_parts->verticalHeader()->setVisible(false);
	m_parts->setSelectionBehavior(QAbstractItemView::SelectRows);
	connect(m_parts, &QTableWidget::itemChanged, this, [this]() { readParts(); updatePreview(); });

	auto *guess = new QPushButton(tr("Sugerir pelos nomes"), page);
	auto *add = new QPushButton(tr("Adicionar"), page);
	auto *remove = new QPushButton(tr("Remover"), page);
	connect(guess, &QPushButton::clicked, this, &PictureComponentDialog::guessParts);
	connect(add, &QPushButton::clicked, this, &PictureComponentDialog::addPart);
	connect(remove, &QPushButton::clicked, this, &PictureComponentDialog::removeSelectedParts);
	auto *buttons = new QHBoxLayout;
	buttons->addWidget(guess);
	buttons->addStretch();
	buttons->addWidget(add);
	buttons->addWidget(remove);

	m_black_box = new QRadioButton(tr("Caixa preta: só a caixa e os bornes"), page);
	m_with_parts = new QRadioButton(tr("Caixa com os contatos e bobinas desenhados"), page);
	m_with_parts->setChecked(true);
	m_body_text = new QLineEdit(page);
	m_body_text->setPlaceholderText(tr("Ex.: Lógica de comando"));
	connect(m_with_parts, &QRadioButton::toggled, this, &PictureComponentDialog::updatePreview);
	connect(m_body_text, &QLineEdit::textChanged, this, &PictureComponentDialog::updatePreview);

	m_parts_status = new QLabel(page);
	m_parts_status->setWordWrap(true);

	auto *form = new QFormLayout;
	form->addRow(m_black_box);
	form->addRow(m_with_parts);
	form->addRow(tr("Texto da caixa :"), m_body_text);

	auto *left = new QVBoxLayout;
	left->addLayout(buttons);
	left->addWidget(m_parts, 1);
	left->addLayout(form);
	left->addWidget(m_parts_status);

	m_preview_scene = new QGraphicsScene(this);
	m_preview_view = new QGraphicsView(m_preview_scene, page);
	m_preview_view->setRenderHint(QPainter::Antialiasing);
	m_preview_view->setBackgroundBrush(Qt::white);

	auto *layout = new QHBoxLayout(page);
	layout->addLayout(left, 1);
	layout->addWidget(m_preview_view, 1);
	return page;
}

QWizardPage *PictureComponentDialog::dataPage()
{
	auto *page = new Page;
	page->setTitle(tr("Dados do componente"));
	page->setSubTitle(tr("Os valores lidos pela IA já estão preenchidos; os outros são os atuais"
			     " do componente. Só os valores alterados são aplicados."));

	m_name = new QLineEdit(page);
	m_info = new QTableWidget(0, 2, page);
	m_info->setHorizontalHeaderLabels({tr("Campo"), tr("Valor")});
	m_info->horizontalHeader()->setStretchLastSection(true);
	m_info->verticalHeader()->setVisible(false);
	m_replace = new QCheckBox(tr("Substituir o desenho atual (senão, o novo é colado junto)"), page);
	m_replace->setChecked(true);
	m_replace->setVisible(m_has_drawing);

	auto *form = new QFormLayout;
	form->addRow(tr("Nome :"), m_name);
	auto *layout = new QVBoxLayout(page);
	layout->addLayout(form);
	layout->addWidget(m_info, 1);
	layout->addWidget(m_replace);
	return page;
}

void PictureComponentDialog::openPicture()
{
	const QString file = QFileDialog::getOpenFileName(
				this, tr("Imagem do componente"), QETApp::documentDir(),
				tr("Imagens e PDF (*.png *.jpg *.jpeg *.bmp *.gif *.pdf)"));
	if (file.isEmpty())
		return;
	if (QFileInfo(file).suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) == 0) {
#ifdef QET_HAS_QTPDF
		m_pdf_file = file;
		QPdfDocument pdf;
		if (pdf.load(file) != QPdfDocument::Error::None || pdf.pageCount() < 1) {
			m_picture_status->setText(tr("Não foi possível abrir o PDF %1.").arg(file));
			return;
		}
		const QSignalBlocker blocker(m_pdf_page);
		m_pdf_page->setRange(1, pdf.pageCount());
		m_pdf_page->setValue(1);
		m_pdf_page->setSuffix(tr(" de %1").arg(pdf.pageCount()));
		m_pdf_page->setVisible(true);
		showPdfPage(0);
#else
		m_picture_status->setText(tr("Esta versão não abre PDF; salve a página como imagem."));
#endif
		return;
	}
	m_pdf_file.clear();
	m_pdf_page->setVisible(false);
	const QImage image(file);
	if (image.isNull()) {
		m_picture_status->setText(tr("Não foi possível abrir a imagem %1.").arg(file));
		return;
	}
	setPicture(image);
}

void PictureComponentDialog::pastePicture()
{
	const QMimeData *mime = QApplication::clipboard()->mimeData();
	QImage image;
	if (mime && mime->hasImage())
		image = qvariant_cast<QImage>(mime->imageData());
	if (image.isNull()) {
		m_picture_status->setText(tr("A área de transferência não tem uma imagem. Copie um recorte"
					     " do diagrama (ex.: Win+Shift+S) e cole de novo."));
		return;
	}
	m_pdf_file.clear();
	m_pdf_page->setVisible(false);
	setPicture(image);
}

	///Render page (from 0) of the PDF opened, at 150 dpi
void PictureComponentDialog::showPdfPage(int page)
{
#ifdef QET_HAS_QTPDF
	QPdfDocument pdf;
	if (m_pdf_file.isEmpty() || pdf.load(m_pdf_file) != QPdfDocument::Error::None)
		return;
	const QSizeF points = pdf.pagePointSize(page);
	const QImage image = pdf.render(page, (points * 150.0 / 72.0).toSize());
	if (!image.isNull()) {
		QImage opaque(image.size(), QImage::Format_RGB32);
		opaque.fill(Qt::white);
		QPainter painter(&opaque);
		painter.drawImage(0, 0, image);
		painter.end();
		setPicture(opaque);
	}
#else
	Q_UNUSED(page)
#endif
}

	///A new picture: the pins read on the former one are dropped
void PictureComponentDialog::setPicture(const QImage &image)
{
	m_picture = image;
	m_result.pins.clear();
	m_result.parts.clear();
	m_pins->setRowCount(0);
	m_picture_preview->setPixmap(QPixmap::fromImage(image).scaled(
					     m_picture_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	m_picture_status->setText(tr("Imagem de %1 × %2 pixels.").arg(image.width()).arg(image.height()));
	m_picture_scene->clear();
	m_picture_scene->addPixmap(QPixmap::fromImage(image));
	m_picture_scene->setSceneRect(image.rect());
	static_cast<Page *>(page(PicturePage))->refresh();
}

void PictureComponentDialog::readWithAi()
{
	if (DatasheetReader::apiKey().isEmpty()) {
		editApiKey();
		if (DatasheetReader::apiKey().isEmpty())
			return;
	}
	QImage image = m_picture;
	if (std::max(image.width(), image.height()) > ai_picture_size)
		image = image.scaled(ai_picture_size, ai_picture_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	QByteArray png;
	QBuffer buffer(&png);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "PNG");

	setBusy(true, tr("A IA está lendo a imagem; pode levar até um minuto..."));
	QNetworkReply *reply = m_network->post(DatasheetReader::aiRequest(),
					       PictureComponent::aiRequestBody(png, QStringLiteral("image/png")));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		setBusy(false);
		QString error;
		const PictureComponent::Result result = PictureComponent::fromAiReply(reply->readAll(), &error);
		if (error.isEmpty() && reply->error() != QNetworkReply::NoError)
			error = reply->errorString();
		if (!error.isEmpty() || result.pins.isEmpty()) {
			m_pins_status->setText(tr("A IA não conseguiu: %1").arg(error.isEmpty() ? tr("nenhum borne encontrado") : error));
			return;
		}
		m_result = result;
		m_body_text->setText(result.body_text);
		fillPins();
		updateMarkers();
		m_pins_status->setText(tr("%n borne(s) lido(s) pela IA. Confira lado e posição: a IA pode errar.",
					  nullptr, int(result.pins.size())));
	});
}

void PictureComponentDialog::editApiKey()
{
	if (DatasheetReader::apiKeyFromEnvironment()) {
		QMessageBox::information(this, windowTitle(),
					 tr("A chave vem da variável de ambiente ANTHROPIC_API_KEY. Para trocar,"
					    " altere a variável."));
		return;
	}
	bool ok = false;
	const QString key = QInputDialog::getText(
				this, tr("Chave da API Claude"),
				tr("Chave da API da Anthropic (console.anthropic.com). Fica guardada nas"
				   " configurações deste usuário do Windows; deixe vazio para apagar."),
				QLineEdit::Password, DatasheetReader::apiKey(), &ok);
	if (!ok)
		return;
	DatasheetReader::setApiKey(key);
	updateApiKeyButton();
}

void PictureComponentDialog::updateApiKeyButton()
{
	m_api_key->setText(DatasheetReader::apiKey().isEmpty() ? tr("Configurar chave da IA...")
							       : tr("Chave da IA ✓"));
}

	///Clicks on the picture add a pin there
bool PictureComponentDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_picture_view->viewport() && event->type() == QEvent::MouseButtonPress) {
		auto *mouse = static_cast<QMouseEvent *>(event);
		if (mouse->button() == Qt::LeftButton && !m_picture.isNull()) {
			const QPointF point = m_picture_view->mapToScene(mouse->position().toPoint());
			if (QRectF(m_picture.rect()).contains(point))
				addClickedPin(point);
			return true;
		}
	}
	return QWizard::eventFilter(watched, event);
}

void PictureComponentDialog::addClickedPin(const QPointF &point)
{
	readPins();
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Borne"), tr("Nome do borne clicado:"),
						   QLineEdit::Normal, QString(), &ok).trimmed();
	if (!ok || name.isEmpty())
		return;
	for (const PictureComponent::Pin &pin : std::as_const(m_result.pins)) {
		if (pin.name == name) {
			m_pins_status->setText(tr("Já existe um borne %1.").arg(name));
			return;
		}
	}
	PictureComponent::Pin pin;
	pin.name = name;
	pin.point = point;
	m_result.pins << pin;
	PictureComponent::placeClickedPins(m_result.pins, m_picture.size());
	fillPins();
	updateMarkers();
	m_pins_status->setText(tr("%n borne(s). Lado e posição vêm de onde você clicou; corrija na tabela se preciso.",
				  nullptr, int(m_result.pins.size())));
}

void PictureComponentDialog::removeSelectedPins()
{
	readPins();
	QList<int> rows;
	for (const QModelIndex &index : m_pins->selectionModel()->selectedRows())
		rows << index.row();
	std::sort(rows.rbegin(), rows.rend());
	for (int row : rows)
		m_result.pins.removeAt(row);
	fillPins();
	updateMarkers();
}

void PictureComponentDialog::clearPins()
{
	m_result.pins.clear();
	m_result.parts.clear();
	fillPins();
	updateMarkers();
}

void PictureComponentDialog::guessParts()
{
	readPins();
	m_result.parts = PictureComponent::guessParts(m_result.pins);
	fillParts();
	updatePreview();
}

void PictureComponentDialog::addPart()
{
	readParts();
	m_result.parts << PictureComponent::Part();
	fillParts();
}

void PictureComponentDialog::removeSelectedParts()
{
	readParts();
	QList<int> rows;
	for (const QModelIndex &index : m_parts->selectionModel()->selectedRows())
		rows << index.row();
	std::sort(rows.rbegin(), rows.rend());
	for (int row : rows)
		m_result.parts.removeAt(row);
	fillParts();
	updatePreview();
}

void PictureComponentDialog::fillPins()
{
	m_pins->setRowCount(0);
	for (const PictureComponent::Pin &pin : std::as_const(m_result.pins)) {
		const int row = m_pins->rowCount();
		m_pins->insertRow(row);
		m_pins->setItem(row, PinName, new QTableWidgetItem(pin.name));
		m_pins->setCellWidget(row, PinSide, sideCombo(pin.side));
		auto *position = new QSpinBox;
		position->setRange(1, 999);
		position->setValue(pin.position + 1);
		m_pins->setCellWidget(row, PinPosition, position);
		m_pins->setItem(row, PinFunction, new QTableWidgetItem(pin.function));
	}
	m_pins->resizeColumnsToContents();
}

	///The table back into m_result.pins. A pin whose side or position the
	///user changed is no longer placed from its click.
void PictureComponentDialog::readPins()
{
	QList<PictureComponent::Pin> pins;
	for (int row = 0; row < m_pins->rowCount(); ++row) {
		PictureComponent::Pin pin = row < m_result.pins.size() ? m_result.pins.at(row) : PictureComponent::Pin();
		pin.name = m_pins->item(row, PinName)->text().trimmed();
		const auto side = ModuleGenerator::Side(static_cast<QComboBox *>(m_pins->cellWidget(row, PinSide))->currentData().toInt());
		const int position = static_cast<QSpinBox *>(m_pins->cellWidget(row, PinPosition))->value() - 1;
		if (side != pin.side || position != pin.position)
			pin.point = QPointF();
		pin.side = side;
		pin.position = position;
		pin.function = m_pins->item(row, PinFunction)->text().trimmed();
		if (!pin.name.isEmpty())
			pins << pin;
	}
	m_result.pins = pins;
}

void PictureComponentDialog::fillParts()
{
	const QSignalBlocker blocker(m_parts);
	m_parts->setRowCount(0);
	QStringList names;
	for (const PictureComponent::Pin &pin : std::as_const(m_result.pins))
		names << pin.name;
	for (const PictureComponent::Part &part : std::as_const(m_result.parts)) {
		const int row = m_parts->rowCount();
		m_parts->insertRow(row);
		auto *kind = new QComboBox;
		for (int k = 0; k < ModuleGenerator::SymbolKindCount; ++k)
			kind->addItem(PictureComponent::kindName(ModuleGenerator::SymbolKind(k)), k);
		kind->setCurrentIndex(kind->findData(int(part.kind)));
		m_parts->setCellWidget(row, PartKindColumn, kind);
		for (int column : {int(PartPinA), int(PartPinB)}) {
			auto *pin = new QComboBox;
			pin->addItems(names);
			pin->setCurrentText(column == PartPinA ? part.pin_a : part.pin_b);
			m_parts->setCellWidget(row, column, pin);
			connect(pin, &QComboBox::currentIndexChanged, this, [this]() { readParts(); updatePreview(); });
		}
		connect(kind, &QComboBox::currentIndexChanged, this, [this]() { readParts(); updatePreview(); });
		m_parts->setItem(row, PartLabel, new QTableWidgetItem(part.label));
		auto *drawn = new QTableWidgetItem;
		drawn->setFlags(Qt::ItemIsEnabled);
		m_parts->setItem(row, PartDrawn, drawn);
	}
	m_parts->resizeColumnsToContents();
}

void PictureComponentDialog::readParts()
{
	QList<PictureComponent::Part> parts;
	for (int row = 0; row < m_parts->rowCount(); ++row) {
		PictureComponent::Part part;
		part.kind = ModuleGenerator::SymbolKind(static_cast<QComboBox *>(m_parts->cellWidget(row, PartKindColumn))->currentData().toInt());
		part.pin_a = static_cast<QComboBox *>(m_parts->cellWidget(row, PartPinA))->currentText();
		part.pin_b = static_cast<QComboBox *>(m_parts->cellWidget(row, PartPinB))->currentText();
		part.label = m_parts->item(row, PartLabel) ? m_parts->item(row, PartLabel)->text().trimmed() : QString();
		parts << part;
	}
	m_result.parts = parts;
}

	///The information table: the AI's value, else the element's
void PictureComponentDialog::fillData()
{
	if (m_name->text().isEmpty())
		m_name->setText(m_result.name.isEmpty() ? m_data.m_names_list.name() : m_result.name);
	m_info->setRowCount(0);
	for (const QString &key : DatasheetReader::infoKeys()) {
		const int row = m_info->rowCount();
		m_info->insertRow(row);
		auto *field = new QTableWidgetItem(QETInformation::translatedInfoKey(key));
		field->setData(Qt::UserRole, key);
		field->setFlags(Qt::ItemIsEnabled);
		m_info->setItem(row, 0, field);
		m_info->setItem(row, 1, new QTableWidgetItem(
					m_result.info.value(key, m_data.m_informations.value(key).toString())));
	}
	m_info->resizeColumnToContents(0);
}

	///Numbered marks on the pins clicked
void PictureComponentDialog::updateMarkers()
{
	for (QGraphicsItem *item : m_picture_scene->items())
		if (item->data(0).toBool())
			delete item;
	const qreal size = std::max(8.0, std::max(m_picture.width(), m_picture.height()) / 80.0);
	for (const PictureComponent::Pin &pin : std::as_const(m_result.pins)) {
		if (pin.point.isNull())
			continue;
		auto *mark = m_picture_scene->addEllipse(pin.point.x() - size / 2, pin.point.y() - size / 2, size, size,
							 QPen(QColor(0, 74, 158), 2), QBrush(QColor(0, 74, 158, 60)));
		mark->setData(0, true);
		auto *text = m_picture_scene->addSimpleText(pin.name);
		text->setBrush(QColor(0, 74, 158));
		text->setPos(pin.point + QPointF(size / 2 + 1, -size));
		text->setData(0, true);
	}
	m_picture_view->fitInView(m_picture_scene->sceneRect(), Qt::KeepAspectRatio);
	static_cast<Page *>(page(PinsPage))->refresh();
}

	///The drawing as it will be made, and which parts can be drawn
void PictureComponentDialog::updatePreview()
{
	if (!m_preview_scene)
		return;
	const QSignalBlocker blocker(m_parts);
	int skipped = 0;
	for (int row = 0; row < m_parts->rowCount() && row < m_result.parts.size(); ++row) {
		const bool ok = PictureComponent::canDraw(m_result.parts.at(row), m_result.pins);
		if (QTableWidgetItem *item = m_parts->item(row, PartDrawn))
			item->setText(ok ? tr("sim") : tr("não: bornes não estão frente a frente"));
		if (!ok)
			++skipped;
	}
	m_parts_status->setText(skipped && m_with_parts->isChecked()
				? tr("%n contato(s) não serão desenhados: os dois bornes precisam estar em"
				     " lados opostos e na mesma posição.", nullptr, skipped)
				: QString());
	m_preview_scene->clear();
	drawDefinition(m_preview_scene, drawing());
	m_preview_scene->setSceneRect(m_preview_scene->itemsBoundingRect().adjusted(-10, -10, 10, 10));
	m_preview_view->fitInView(m_preview_scene->sceneRect(), Qt::KeepAspectRatio);
}

void PictureComponentDialog::setBusy(bool busy, const QString &status)
{
	m_read_ai->setEnabled(!busy);
	button(QWizard::NextButton)->setEnabled(!busy);
	if (busy)
		QApplication::setOverrideCursor(Qt::BusyCursor);
	else
		QApplication::restoreOverrideCursor();
	if (!status.isEmpty())
		m_pins_status->setText(status);
}
