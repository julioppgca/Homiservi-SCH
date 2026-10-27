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
#include "projecthistorydialog.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
		/// "há 5 min", "há 3 h", "há 2 dias"
	QString age(const QDateTime &when)
	{
		const qint64 seconds = when.secsTo(QDateTime::currentDateTime());
		if (seconds < 60)
			return QObject::tr("agora há pouco");
		if (seconds < 3600)
			return QObject::tr("há %1 min").arg(seconds / 60);
		if (seconds < 86400)
			return QObject::tr("há %1 h").arg(seconds / 3600);
		return QObject::tr("há %n dia(s)", nullptr, int(seconds / 86400));
	}
}

/**
	@brief ProjectHistoryDialog::ProjectHistoryDialog
	@param project_path : path of the project whose versions are listed
	@param parent : parent widget
*/
ProjectHistoryDialog::ProjectHistoryDialog(const QString &project_path,
					   QWidget *parent) :
	QDialog(parent),
	m_project_path(project_path),
	m_entries(ProjectHistory::entries(project_path))
{
	setWindowTitle(tr("Histórico de versões - %1")
		       .arg(QFileInfo(project_path).fileName()));
	resize(560, 420);

	const QLocale locale;
	auto layout = new QVBoxLayout(this);

	QString text = tr("A cada salvamento, a versão anterior do arquivo é guardada"
			  " aqui. São mantidas as %n mais recentes (Configurações >"
			  " Projetos).", nullptr, ProjectHistory::maxVersions());
	if (ProjectHistory::maxVersions() == 0) {
		text = tr("O histórico de versões está desativado (Configurações >"
			  " Projetos). As versões abaixo foram guardadas antes.");
	}
	const QFileInfo current(project_path);
	text += QStringLiteral("\n\n") + tr("Versão atual: salva em %1, %2.")
			.arg(locale.toString(current.lastModified(), QLocale::ShortFormat),
			     locale.formattedDataSize(current.size()));
	auto label = new QLabel(text, this);
	label->setWordWrap(true);
	layout->addWidget(label);

	m_list = new QTreeWidget(this);
	m_list->setRootIsDecorated(false);
	m_list->setAlternatingRowColors(true);
	m_list->setHeaderLabels({tr("Salvo em"), tr("Quando"), tr("Tamanho")});
	for (const ProjectHistory::Entry &entry : std::as_const(m_entries)) {
		auto item = new QTreeWidgetItem(m_list, {
			locale.toString(entry.saved_at, QStringLiteral("dd/MM/yyyy HH:mm:ss")),
			age(entry.saved_at),
			locale.formattedDataSize(entry.size)});
		item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
	}
	m_list->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	layout->addWidget(m_list);

	if (m_entries.isEmpty()) {
		auto empty = new QLabel(tr("Nenhuma versão guardada ainda: a primeira"
					   " entra no histórico no próximo salvamento."), this);
		empty->setWordWrap(true);
		layout->addWidget(empty);
	}

	auto buttons = new QDialogButtonBox(this);
	m_open_copy = buttons->addButton(tr("Abrir cópia"), QDialogButtonBox::ActionRole);
	m_open_copy->setToolTip(tr("Abre esta versão como outro projeto, somente"
				   " leitura, para comparar ou copiar partes"));
	m_restore = buttons->addButton(tr("Restaurar..."), QDialogButtonBox::ActionRole);
	m_restore->setToolTip(tr("Põe esta versão no lugar do arquivo do projeto."
				 " A versão atual entra no histórico antes."));
	auto open_folder = buttons->addButton(tr("Abrir pasta"), QDialogButtonBox::HelpRole);
	buttons->addButton(QDialogButtonBox::Close);
	layout->addWidget(buttons);

	connect(m_open_copy, &QPushButton::clicked, this, [this]{ choose(OpenCopy); });
	connect(m_restore, &QPushButton::clicked, this, [this]{ choose(Restore); });
	connect(open_folder, &QPushButton::clicked, this, [this]{
		const QString dir = ProjectHistory::historyDir(m_project_path);
		QDir().mkpath(dir);
		QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_list, &QTreeWidget::itemSelectionChanged,
		this, &ProjectHistoryDialog::updateButtons);
	connect(m_list, &QTreeWidget::itemDoubleClicked,
		this, [this]{ choose(OpenCopy); });

	if (!m_entries.isEmpty()) {
		m_list->setCurrentItem(m_list->topLevelItem(0));
	}
	updateButtons();
}

/**
	@brief ProjectHistoryDialog::selectedEntry
	@return the version chosen in the list, an empty entry if none
*/
ProjectHistory::Entry ProjectHistoryDialog::selectedEntry() const
{
	const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
	if (row < 0 || row >= m_entries.size()) {
		return ProjectHistory::Entry();
	}
	return m_entries.at(row);
}

void ProjectHistoryDialog::updateButtons()
{
	const bool selected = m_list->currentItem() != nullptr;
	m_open_copy->setEnabled(selected);
	m_restore->setEnabled(selected);
}

void ProjectHistoryDialog::choose(Action action)
{
	if (selectedEntry().file_path.isEmpty()) {
		return;
	}
	m_action = action;
	accept();
}
