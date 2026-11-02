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
#include "datasheetdialog.h"

#include "../../qetapp.h"
#include "../../qetinformation.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QVBoxLayout>

namespace {
		///The row of the component name in the information table
	const QString name_key = QStringLiteral("__name");
		///The Claude API takes PDFs up to 32 MB per request
	constexpr qint64 max_ai_pdf = 30 * 1024 * 1024;

	enum InfoColumn { FieldColumn, CurrentColumn, NewColumn };
	enum PinColumn { NameColumn, FunctionColumn, SideColumn };
}

DatasheetDialog::DatasheetDialog(const ElementData &data, bool has_drawing, QWidget *parent) :
	QDialog(parent),
	m_data(data),
	m_network(new QNetworkAccessManager(this))
{
	setWindowTitle(tr("Datasheet do componente"));

	m_datasheet = new QLineEdit(data.m_informations.value(QETInformation::ELMT_DATASHEET).toString(), this);
	m_datasheet->setPlaceholderText(tr("arquivo PDF ou link https://..."));
	auto browse_button = new QPushButton(tr("Escolher..."), this);
	auto open_button = new QPushButton(tr("Abrir"), this);
	connect(browse_button, &QPushButton::clicked, this, &DatasheetDialog::browse);
	connect(open_button, &QPushButton::clicked, this, &DatasheetDialog::openDatasheet);
	auto file_row = new QHBoxLayout();
	file_row->addWidget(new QLabel(tr("Datasheet:"), this));
	file_row->addWidget(m_datasheet, 1);
	file_row->addWidget(browse_button);
	file_row->addWidget(open_button);

	m_method = new QComboBox(this);
	m_method->addItem(tr("Automático: IA quando houver chave, senão regras"), Automatic);
	m_method->addItem(tr("IA (API Claude)"), Ai);
	m_method->addItem(tr("Regras (offline)"), Rules);
	m_method->setCurrentIndex(QSettings().value(QStringLiteral("homiservi/datasheet_method"), 0).toInt());
	m_method->setToolTip(tr("A IA lê o PDF inteiro, inclusive tabelas e desenhos, e acerta muito mais;"
				" tem custo por uso e envia o PDF à Anthropic. As regras leem só o texto,"
				" sem internet."));
	m_api_key = new QPushButton(this);
	connect(m_api_key, &QPushButton::clicked, this, &DatasheetDialog::editApiKey);
	m_read = new QPushButton(tr("Ler datasheet"), this);
	m_read->setDefault(true);
	connect(m_read, &QPushButton::clicked, this, &DatasheetDialog::read);
	auto read_row = new QHBoxLayout();
	read_row->addWidget(new QLabel(tr("Ler com:"), this));
	read_row->addWidget(m_method, 1);
	read_row->addWidget(m_api_key);
	read_row->addWidget(m_read);

	m_status = new QLabel(tr("Escolha o datasheet e clique em Ler datasheet. Nada muda no componente"
				 " antes de você conferir e clicar em OK."), this);
	m_status->setWordWrap(true);

	m_info = new QTableWidget(0, 3, this);
	m_info->setHorizontalHeaderLabels({tr("Preencher"), tr("Atual"), tr("Do datasheet")});
	m_info->horizontalHeader()->setSectionResizeMode(FieldColumn, QHeaderView::ResizeToContents);
	m_info->horizontalHeader()->setSectionResizeMode(CurrentColumn, QHeaderView::Stretch);
	m_info->horizontalHeader()->setSectionResizeMode(NewColumn, QHeaderView::Stretch);
	m_info->verticalHeader()->hide();
	QStringList keys = {name_key};
	keys << DatasheetReader::infoKeys();
	for (const QString &key : keys) {
		const int row = m_info->rowCount();
		m_info->insertRow(row);
		auto field = new QTableWidgetItem(key == name_key ? tr("Nome do componente")
								  : QETInformation::translatedInfoKey(key));
		field->setData(Qt::UserRole, key);
		field->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
		field->setCheckState(Qt::Unchecked);
		m_info->setItem(row, FieldColumn, field);
		auto current = new QTableWidgetItem(key == name_key
						    ? data.m_names_list.name()
						    : data.m_informations.value(key).toString());
		current->setFlags(Qt::ItemIsEnabled);
		m_info->setItem(row, CurrentColumn, current);
		m_info->setItem(row, NewColumn, new QTableWidgetItem());
	}
		//Writing a value ticks its row
	connect(m_info, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
		if (item->column() == NewColumn && m_info->item(item->row(), FieldColumn))
			m_info->item(item->row(), FieldColumn)->setCheckState(
						item->text().trimmed().isEmpty() ? Qt::Unchecked : Qt::Checked);
	});
	auto info_box = new QGroupBox(tr("Dados"), this);
	auto info_layout = new QVBoxLayout(info_box);
	info_layout->addWidget(m_info);

	m_pins = new QTableWidget(0, 3, this);
	m_pins->setHorizontalHeaderLabels({tr("Pino"), tr("Função"), tr("Lado")});
	m_pins->horizontalHeader()->setSectionResizeMode(FunctionColumn, QHeaderView::Stretch);
	m_pins->verticalHeader()->hide();
	auto add_pin = new QPushButton(tr("Adicionar pino"), this);
	auto remove_pin = new QPushButton(tr("Remover pino"), this);
	connect(add_pin, &QPushButton::clicked, this, [this]() { addPinRow(DatasheetReader::Pin()); });
	connect(remove_pin, &QPushButton::clicked, this, [this]() {
		const int row = m_pins->currentRow();
		if (row >= 0)
			m_pins->removeRow(row);
	});
	m_draw = new QCheckBox(tr("Gerar o desenho básico com estes pinos (caixa com os bornes)"), this);
	m_replace = new QCheckBox(tr("Substituir o desenho atual"), this);
	m_replace->setEnabled(has_drawing);
	m_replace->setVisible(has_drawing);
	connect(m_draw, &QCheckBox::toggled, m_replace, [this, has_drawing](bool on) {
		m_replace->setEnabled(on && has_drawing);
	});
	m_draw->setChecked(false);
	m_replace->setEnabled(false);
	auto pin_buttons = new QHBoxLayout();
	pin_buttons->addWidget(add_pin);
	pin_buttons->addWidget(remove_pin);
	pin_buttons->addStretch();
	auto pins_box = new QGroupBox(tr("Pinos"), this);
	auto pins_layout = new QVBoxLayout(pins_box);
	pins_layout->addWidget(m_pins);
	pins_layout->addLayout(pin_buttons);
	pins_layout->addWidget(m_draw);
	pins_layout->addWidget(m_replace);

	auto tables = new QHBoxLayout();
	tables->addWidget(info_box, 3);
	tables->addWidget(pins_box, 2);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	m_ok = buttons->button(QDialogButtonBox::Ok);
	connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
		QSettings().setValue(QStringLiteral("homiservi/datasheet_method"), m_method->currentIndex());
		accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto layout = new QVBoxLayout(this);
	layout->addLayout(file_row);
	layout->addLayout(read_row);
	layout->addWidget(m_status);
	layout->addLayout(tables, 1);
	layout->addWidget(buttons);
	updateApiKeyButton();
	resize(980, 620);
}

	///The file or the link the user gave
QString DatasheetDialog::datasheet() const
{
	return m_datasheet->text().trimmed();
}

	///The name to give the component, empty to keep its own
QString DatasheetDialog::name() const
{
	for (int row = 0; row < m_info->rowCount(); ++row)
		if (m_info->item(row, FieldColumn)->data(Qt::UserRole).toString() == name_key
				&& m_info->item(row, FieldColumn)->checkState() == Qt::Checked)
			return m_info->item(row, NewColumn)->text().trimmed();
	return QString();
}

	///The information of the ticked rows, by key
QMap<QString, QString> DatasheetDialog::information() const
{
	QMap<QString, QString> map;
	for (int row = 0; row < m_info->rowCount(); ++row) {
		const QString key = m_info->item(row, FieldColumn)->data(Qt::UserRole).toString();
		if (key != name_key && m_info->item(row, FieldColumn)->checkState() == Qt::Checked)
			map.insert(key, m_info->item(row, NewColumn)->text().trimmed());
	}
	return map;
}

bool DatasheetDialog::drawPins() const
{
	return m_draw->isChecked() && m_pins->rowCount() > 0;
}

bool DatasheetDialog::replaceDrawing() const
{
	return m_replace->isEnabled() && m_replace->isChecked();
}

QList<DatasheetReader::Pin> DatasheetDialog::pins() const
{
	QList<DatasheetReader::Pin> list;
	for (int row = 0; row < m_pins->rowCount(); ++row) {
		DatasheetReader::Pin pin;
		pin.name = m_pins->item(row, NameColumn) ? m_pins->item(row, NameColumn)->text().trimmed() : QString();
		pin.function = m_pins->item(row, FunctionColumn) ? m_pins->item(row, FunctionColumn)->text().trimmed()
								  : QString();
		if (auto side = qobject_cast<QComboBox *>(m_pins->cellWidget(row, SideColumn)))
			pin.side = ModuleGenerator::Side(side->currentData().toInt());
		if (!pin.name.isEmpty())
			list << pin;
	}
	return list;
}

	///The text in the box: the reference, else the model, of the component
QString DatasheetDialog::bodyText() const
{
	for (const QString &key : {QETInformation::ELMT_MANUFACTURER_REF, QETInformation::ELMT_MODEL}) {
		for (int row = 0; row < m_info->rowCount(); ++row)
			if (m_info->item(row, FieldColumn)->data(Qt::UserRole).toString() == key) {
				const QString value = m_info->item(row, NewColumn)->text().trimmed();
				if (!value.isEmpty())
					return value;
				const QString current = m_info->item(row, CurrentColumn)->text().trimmed();
				if (!current.isEmpty())
					return current;
			}
	}
	return QString();
}

void DatasheetDialog::browse()
{
	const QString start = DatasheetReader::isLink(datasheet()) || datasheet().isEmpty()
			? QSettings().value(QStringLiteral("homiservi/datasheet_dir")).toString()
			: QFileInfo(datasheet()).absolutePath();
	const QString file = QFileDialog::getOpenFileName(this, tr("Datasheet do componente"), start,
							  tr("Documentos PDF (*.pdf)"));
	if (file.isEmpty())
		return;
	QSettings().setValue(QStringLiteral("homiservi/datasheet_dir"), QFileInfo(file).absolutePath());
	m_datasheet->setText(QDir::toNativeSeparators(file));
}

void DatasheetDialog::openDatasheet()
{
	if (datasheet().isEmpty())
		return;
	if (!QDesktopServices::openUrl(DatasheetReader::url(datasheet())))
		QMessageBox::warning(this, windowTitle(), tr("Não foi possível abrir %1.").arg(datasheet()));
}

void DatasheetDialog::editApiKey()
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

void DatasheetDialog::updateApiKeyButton()
{
	m_api_key->setText(DatasheetReader::apiKey().isEmpty() ? tr("Configurar chave da IA...")
							       : tr("Chave da IA ✓"));
}

/**
	@brief DatasheetDialog::read
	Get the PDF, from the disk or the link, then read it.
*/
void DatasheetDialog::read()
{
	if (datasheet().isEmpty()) {
		browse();
		if (datasheet().isEmpty())
			return;
	}
	if (!DatasheetReader::isLink(datasheet())) {
		QFile file(datasheet());
		if (!file.open(QIODevice::ReadOnly)) {
			m_status->setText(tr("Não foi possível abrir o arquivo %1.").arg(datasheet()));
			return;
		}
		readBytes(file.readAll());
		return;
	}

	setBusy(true, tr("Baixando o datasheet..."));
	QNetworkRequest request(DatasheetReader::url(datasheet()));
	request.setTransferTimeout(60 * 1000);
	QNetworkReply *reply = m_network->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		setBusy(false);
		const QByteArray bytes = reply->readAll();
		if (reply->error() != QNetworkReply::NoError) {
			m_status->setText(tr("Não foi possível baixar o datasheet: %1").arg(reply->errorString()));
			return;
		}
		if (!bytes.startsWith("%PDF")) {
			m_status->setText(tr("O link não leva direto a um PDF (é uma página da web?). Baixe o PDF"
					     " e escolha o arquivo, ou use o link direto do PDF."));
			return;
		}
		readBytes(bytes);
	});
}

void DatasheetDialog::readBytes(const QByteArray &pdf)
{
	const Method method = Method(m_method->currentData().toInt());
	const bool has_key = !DatasheetReader::apiKey().isEmpty();
	if (method == Rules) {
		readWithRules(pdf, QString());
	} else if (!has_key) {
		if (method == Ai) {
			m_status->setText(tr("Configure a chave da API Claude para ler com a IA."));
			editApiKey();
			return;
		}
		readWithRules(pdf, tr("Sem chave da IA: lido pelas regras."));
	} else if (pdf.size() > max_ai_pdf) {
		readWithRules(pdf, tr("O PDF passa de 30 MB, o limite da IA: lido pelas regras."));
	} else {
		readWithAi(pdf);
	}
}

void DatasheetDialog::readWithAi(const QByteArray &pdf)
{
	setBusy(true, tr("A IA está lendo o datasheet; pode levar um minuto..."));
	QNetworkReply *reply = m_network->post(DatasheetReader::aiRequest(),
					       DatasheetReader::aiRequestBody(pdf));
	connect(reply, &QNetworkReply::finished, this, [this, reply, pdf]() {
		reply->deleteLater();
		setBusy(false);
		const QByteArray body = reply->readAll();
		QString error;
		const DatasheetReader::Result result = DatasheetReader::fromAiReply(body, &error);
		if (error.isEmpty() && reply->error() != QNetworkReply::NoError)
			error = reply->errorString();
		if (!error.isEmpty() || result.isEmpty()) {
			const QString why = tr("A IA não conseguiu: %1").arg(error.isEmpty() ? tr("resposta vazia") : error);
			if (Method(m_method->currentData().toInt()) == Automatic)
				readWithRules(pdf, why + QLatin1Char(' ') + tr("Lido pelas regras."));
			else
				m_status->setText(why);
			return;
		}
		showResult(result, tr("Lido pela IA. Confira os valores antes de aplicar: a IA pode errar."));
	});
}

void DatasheetDialog::readWithRules(const QByteArray &pdf, const QString &why)
{
	QTemporaryFile file(QDir::tempPath() + QStringLiteral("/datasheet-XXXXXX.pdf"));
	if (!file.open()) {
		m_status->setText(tr("Não foi possível criar um arquivo temporário."));
		return;
	}
	file.write(pdf);
	file.close();
	QString error;
	const QString text = DatasheetReader::pdfText(file.fileName(), &error);
	if (!error.isEmpty()) {
		m_status->setText((why.isEmpty() ? QString() : why + QLatin1Char(' ')) + error);
		return;
	}
	const DatasheetReader::Result result = DatasheetReader::readByRules(text);
	QString status = why.isEmpty() ? tr("Lido pelas regras.") : why;
	status += QLatin1Char(' ') + tr("As regras só acertam em datasheets bem organizados: confira e"
				       " complete os valores.");
	if (result.isEmpty())
		status = (why.isEmpty() ? QString() : why + QLatin1Char(' '))
			 + tr("As regras não acharam nada neste datasheet; preencha à mão ou use a IA.");
	showResult(result, status);
}

	///Put result in the tables, ticking the rows it fills
void DatasheetDialog::showResult(const DatasheetReader::Result &result, const QString &status)
{
	for (int row = 0; row < m_info->rowCount(); ++row) {
		const QString key = m_info->item(row, FieldColumn)->data(Qt::UserRole).toString();
		const QString value = key == name_key ? result.name : result.info.value(key);
		const QString current = m_info->item(row, CurrentColumn)->text().trimmed();
		m_info->item(row, NewColumn)->setText(value);
			//itemChanged ticked it: only the rows that change something stay ticked
		m_info->item(row, FieldColumn)->setCheckState(!value.isEmpty() && value != current
							      ? Qt::Checked : Qt::Unchecked);
	}
	if (!result.pins.isEmpty()) {
		m_pins->setRowCount(0);
		for (const DatasheetReader::Pin &pin : result.pins)
			addPinRow(pin);
		m_pins->resizeColumnToContents(NameColumn);
		m_draw->setChecked(true);
	}
	m_status->setText(status);
}

void DatasheetDialog::addPinRow(const DatasheetReader::Pin &pin)
{
	const int row = m_pins->rowCount();
	m_pins->insertRow(row);
	m_pins->setItem(row, NameColumn, new QTableWidgetItem(pin.name));
	m_pins->setItem(row, FunctionColumn, new QTableWidgetItem(pin.function));
	auto side = new QComboBox(m_pins);
	for (ModuleGenerator::Side s : {ModuleGenerator::Left, ModuleGenerator::Right,
					ModuleGenerator::Top, ModuleGenerator::Bottom})
		side->addItem(DatasheetReader::sideName(s), int(s));
	side->setCurrentIndex(side->findData(int(pin.side)));
	m_pins->setCellWidget(row, SideColumn, side);
	if (pin.name.isEmpty())
		m_pins->editItem(m_pins->item(row, NameColumn));
}

void DatasheetDialog::setBusy(bool busy, const QString &status)
{
	m_read->setEnabled(!busy);
	m_method->setEnabled(!busy);
	m_ok->setEnabled(!busy);
	if (busy)
		QApplication::setOverrideCursor(Qt::BusyCursor);
	else
		QApplication::restoreOverrideCursor();
	if (!status.isEmpty())
		m_status->setText(status);
}
