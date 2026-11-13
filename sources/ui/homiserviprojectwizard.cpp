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
#include "homiserviprojectwizard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QSettings>
#include <QVBoxLayout>

namespace {
	QLineEdit *lineEdit(const QString &text, const QString &placeholder, QWidget *parent)
	{
		auto edit = new QLineEdit(text, parent);
		edit->setPlaceholderText(placeholder);
		return edit;
	}

		///The folios of the schematics a new project starts with
	QString defaultSchematics()
	{
		return HomiserviProjectWizard::tr("Distribuição de força\nComando\nEntradas e saídas do CLP\nRégua de bornes");
	}
}

/**
	@brief HomiserviProjectWizard::HomiserviProjectWizard
	@param mode : a new project, or the data of an existing one
	@param data : the starting values
	@param folio_count : the folios of an existing project
*/
HomiserviProjectWizard::HomiserviProjectWizard(Mode mode, const HomiserviTemplate::Data &data,
					       int folio_count, QWidget *parent) :
	QWizard(parent),
	m_mode(mode)
{
	setWindowTitle(mode == NewProject ? tr("Novo projeto Homiservi")
					  : tr("Dados do projeto Homiservi"));
	setWizardStyle(QWizard::ModernStyle);
	const QPixmap logo(QStringLiteral(":/ico/homiservi/homiservi-logo.png"));
	if (!logo.isNull())
		setPixmap(QWizard::LogoPixmap, logo.scaledToHeight(48, Qt::SmoothTransformation));
	setOption(QWizard::NoBackButtonOnStartPage);

	addPage(projectPage(data));
	addPage(peoplePage(data));
	addPage(mode == NewProject ? foliosPage() : applyPage(folio_count));
	setButtonText(QWizard::FinishButton, mode == NewProject ? tr("Criar o projeto")
								: tr("Aplicar em todas as folhas"));
	resize(620, 520);
}

QWizardPage *HomiserviProjectWizard::projectPage(const HomiserviTemplate::Data &data)
{
	auto page = new QWizardPage(this);
	page->setTitle(tr("Dados do projeto"));
	page->setSubTitle(tr("Aparecem na legenda de todas as folhas e na capa."));
	auto form = new QFormLayout(page);
	m_title = lineEdit(data.title, tr("ex.: Painel de comando da máquina 01"), page);
	m_client = lineEdit(data.client, tr("ex.: Homiservi"), page);
	m_plant = lineEdit(data.plant, tr("ex.: Painel Elétrico"), page);
	m_location = lineEdit(data.location, tr("ex.: Máquina 01 – Usinagem"), page);
	m_drawing_number = lineEdit(data.drawing_number, tr("ex.: HS-0001"), page);
	m_filename = lineEdit(data.filename, tr("referência do documento (opcional)"), page);
	m_layout = new QComboBox(page);
	m_layout->addItem(tr("Simples"), HomiserviTemplate::Simple);
	m_layout->addItem(tr("Com quadro de revisões (A3, grade 13 × 6, referências 15.B4)"),
			  HomiserviTemplate::Revisions);
	m_layout->setCurrentIndex(m_layout->findData(data.layout));
	m_layout->setToolTip(tr("As revisões são editadas em Projeto > Revisões do projeto"));
	form->addRow(tr("Título do projeto:"), m_title);
	form->addRow(tr("Cliente:"), m_client);
	form->addRow(tr("Instalação:"), m_plant);
	form->addRow(tr("Local / máquina:"), m_location);
	form->addRow(tr("Número do desenho:"), m_drawing_number);
	form->addRow(tr("Arquivo / referência:"), m_filename);
	form->addRow(tr("Legenda:"), m_layout);
	return page;
}

QWizardPage *HomiserviProjectWizard::peoplePage(const HomiserviTemplate::Data &data)
{
	auto page = new QWizardPage(this);
	page->setTitle(tr("Responsáveis e revisão"));
	page->setSubTitle(tr("Os nomes ficam guardados para os próximos projetos."));
	auto form = new QFormLayout(page);
	QSettings settings;
	m_designer = lineEdit(data.designer.isEmpty()
			      ? settings.value(QStringLiteral("homiservi/designer")).toString() : data.designer,
			      QString(), page);
	m_author = lineEdit(data.author.isEmpty()
			    ? settings.value(QStringLiteral("homiservi/author")).toString() : data.author,
			    tr("ex.: Eng. Responsável"), page);
	m_checker = lineEdit(data.checker.isEmpty()
			     ? settings.value(QStringLiteral("homiservi/checker")).toString() : data.checker,
			     QString(), page);
	m_date = new QDateEdit(data.date.isValid() ? data.date : QDate::currentDate(), page);
	m_date->setCalendarPopup(true);
	m_revision = lineEdit(data.revision.isEmpty() ? QStringLiteral("0") : data.revision,
			      tr("ex.: 0, A, 1.0"), page);
	form->addRow(tr("Projetado por:"), m_designer);
	form->addRow(tr("Desenhado por:"), m_author);
	form->addRow(tr("Verificado por:"), m_checker);
	form->addRow(tr("Data:"), m_date);
	form->addRow(tr("Revisão:"), m_revision);
	return page;
}

QWizardPage *HomiserviProjectWizard::foliosPage()
{
	auto page = new QWizardPage(this);
	page->setTitle(tr("Folhas"));
	page->setSubTitle(tr("O projeto já nasce com a legenda Homiservi, os estilos de texto e as regras"
			     " de cor e bitola dos fios."));
	auto layout = new QVBoxLayout(page);
	m_cover = new QCheckBox(tr("Capa, com o logotipo e os dados do projeto"), page);
	m_contents = new QCheckBox(tr("Índice (atualiza sozinho)"), page);
	m_materials = new QCheckBox(tr("Lista de materiais no fim (atualiza sozinha)"), page);
	for (QCheckBox *box : {m_cover, m_contents, m_materials})
		box->setChecked(true);
	layout->addWidget(m_cover);
	layout->addWidget(m_contents);
	layout->addWidget(new QLabel(tr("Seções de esquema, uma por linha (cada uma começa com uma folha"
					 " com esse título):"), page));
	m_schematics = new QPlainTextEdit(defaultSchematics(), page);
	layout->addWidget(m_schematics, 1);
	layout->addWidget(m_materials);
	m_numbering = new QCheckBox(tr("Numerar no padrão Homiservi: folhas por seção (02.01), tags por seção"
				       " (-KA21) e fios por folha (02.01-01), perto do componente"), page);
	m_numbering->setToolTip(tr("A capa e o índice ficam na seção 01 – Geral e as listas na última."
				   " Tudo pode ser mudado depois em Projeto > Numeração."));
	m_numbering->setChecked(true);
	layout->addWidget(m_numbering);
	return page;
}

QWizardPage *HomiserviProjectWizard::applyPage(int folio_count)
{
	auto page = new QWizardPage(this);
	page->setTitle(tr("Aplicar"));
	page->setSubTitle(tr("Os dados vão para a legenda das %n folha(s) do projeto e para a capa,"
			     " se houver. O título de cada folha não muda.", nullptr, folio_count));
	auto layout = new QVBoxLayout(page);
	m_use_title_block = new QCheckBox(tr("Usar a legenda Homiservi em todas as folhas"), page);
	m_use_title_block->setChecked(true);
	m_load_styles = new QCheckBox(tr("Carregar os estilos de texto e as regras de cor e bitola"
					 " Homiservi (só os que o projeto ainda não tem)"), page);
	m_load_styles->setToolTip(tr("Os estilos automáticos mudam a fonte das identificações, funções,"
				     " localizações e textos livres"));
	layout->addWidget(m_use_title_block);
	layout->addWidget(m_load_styles);
	layout->addStretch();
	return page;
}

HomiserviTemplate::Data HomiserviProjectWizard::data() const
{
	HomiserviTemplate::Data data;
	data.title = m_title->text().trimmed();
	data.client = m_client->text().trimmed();
	data.plant = m_plant->text().trimmed();
	data.location = m_location->text().trimmed();
	data.drawing_number = m_drawing_number->text().trimmed();
	data.filename = m_filename->text().trimmed();
	data.layout = HomiserviTemplate::Layout(m_layout->currentData().toInt());
	data.designer = m_designer->text().trimmed();
	data.author = m_author->text().trimmed();
	data.checker = m_checker->text().trimmed();
	data.date = m_date->date();
	data.revision = m_revision->text().trimmed();
	return data;
}

HomiserviTemplate::Folios HomiserviProjectWizard::folios() const
{
	HomiserviTemplate::Folios folios;
	if (m_mode != NewProject)
		return folios;
	folios.cover = m_cover->isChecked();
	folios.contents = m_contents->isChecked();
	folios.materials = m_materials->isChecked();
	folios.numbering = m_numbering->isChecked();
	for (const QString &line : m_schematics->toPlainText().split(QLatin1Char('\n')))
		if (!line.trimmed().isEmpty())
			folios.schematics << line.trimmed();
	return folios;
}

bool HomiserviProjectWizard::useTitleBlock() const
{
	return m_mode == NewProject || m_use_title_block->isChecked();
}

bool HomiserviProjectWizard::loadStyles() const
{
	return m_mode == NewProject || m_load_styles->isChecked();
}

	///Remember who draws and who checks, for the next projects
void HomiserviProjectWizard::accept()
{
	QSettings settings;
	settings.setValue(QStringLiteral("homiservi/designer"), m_designer->text().trimmed());
	settings.setValue(QStringLiteral("homiservi/author"), m_author->text().trimmed());
	settings.setValue(QStringLiteral("homiservi/checker"), m_checker->text().trimmed());
	QWizard::accept();
}
