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
#include "partlibrarydialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
		///Draw the parts of an element definition on scene, for the preview
	void drawDefinition(QGraphicsScene *scene, const QDomDocument &document)
	{
		const QDomElement description = document.documentElement().firstChildElement(QStringLiteral("description"));
		for (QDomElement e = description.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
			auto number = [&e](const char *name) { return e.attribute(QString::fromLatin1(name)).toDouble(); };
			const QString style = e.attribute(QStringLiteral("style"));
			QPen pen(Qt::black, 1);
			pen.setCosmetic(true);
			if (style.contains(QLatin1String("dashed")))
				pen.setStyle(Qt::DashLine);
			const QBrush brush = style.contains(QLatin1String("filling:black")) ? QBrush(Qt::black) : QBrush();
			const QString tag = e.tagName();
			if (tag == QLatin1String("line")) {
				scene->addLine(number("x1"), number("y1"), number("x2"), number("y2"), pen);
			} else if (tag == QLatin1String("rect")) {
				QPainterPath path;
				path.addRoundedRect(QRectF(number("x"), number("y"), number("width"), number("height")),
						    number("rx"), number("ry"));
				scene->addPath(path, pen, brush);
			} else if (tag == QLatin1String("circle")) {
				scene->addEllipse(number("x"), number("y"), number("diameter"), number("diameter"), pen, brush);
			} else if (tag == QLatin1String("arc")) {
				QPainterPath path;
				const QRectF rect(number("x"), number("y"), number("width"), number("height"));
				path.arcMoveTo(rect, number("start"));
				path.arcTo(rect, number("start"), number("angle"));
				scene->addPath(path, pen);
			} else if (tag == QLatin1String("polygon")) {
				QPolygonF points;
				for (int i = 1; e.hasAttribute(QStringLiteral("x%1").arg(i)); ++i)
					points << QPointF(e.attribute(QStringLiteral("x%1").arg(i)).toDouble(),
							  e.attribute(QStringLiteral("y%1").arg(i)).toDouble());
				QPainterPath path;
				path.addPolygon(points);
				if (e.attribute(QStringLiteral("closed")) != QLatin1String("false"))
					path.closeSubpath();
				scene->addPath(path, pen, brush);
			} else if (tag == QLatin1String("terminal")) {
				scene->addEllipse(number("x") - 1.5, number("y") - 1.5, 3, 3, QPen(Qt::red, 0.5), QBrush(Qt::red));
				auto *name = scene->addSimpleText(e.attribute(QStringLiteral("name")), QFont(QStringLiteral("Sans Serif"), 4));
				name->setBrush(Qt::darkRed);
				name->setPos(number("x") + 2, number("y") - 7);
			} else if (tag == QLatin1String("text")) {
				QFont font;
				font.fromString(e.attribute(QStringLiteral("font")));
				auto *item = scene->addSimpleText(e.attribute(QStringLiteral("text")), font);
					//The part's position is the left end of its baseline
				item->setPos(number("x"), number("y") - QFontMetricsF(font).ascent());
			}
		}
	}
}

PartLibraryDialog::PartLibraryDialog(QWidget *parent) :
	QDialog(parent),
	m_entries(PartLibrary::entries())
{
	setWindowTitle(tr("Inserir peça"));
	resize(820, 480);
	auto layout = new QVBoxLayout(this);

	auto splitter = new QSplitter(this);
	m_tree = new QTreeWidget(splitter);
	m_tree->setHeaderHidden(true);
	QHash<QString, QTreeWidgetItem *> groups;
	for (const PartLibrary::Entry &entry : std::as_const(m_entries)) {
		QTreeWidgetItem *group = groups.value(entry.group);
		if (!group) {
			group = new QTreeWidgetItem(m_tree, {entry.group});
			group->setFlags(Qt::ItemIsEnabled);
			group->setExpanded(true);
			groups.insert(entry.group, group);
		}
		auto item = new QTreeWidgetItem(group, {entry.name});
		item->setData(0, Qt::UserRole, entry.id);
	}

	auto right = new QWidget(splitter);
	auto right_layout = new QVBoxLayout(right);
	right_layout->setContentsMargins(0, 0, 0, 0);
	m_params = new QWidget(right);
	m_form = new QFormLayout(m_params);
	right_layout->addWidget(m_params);
	m_preview_scene = new QGraphicsScene(this);
	m_preview = new QGraphicsView(m_preview_scene, right);
	m_preview->setRenderHint(QPainter::Antialiasing);
	m_preview->setBackgroundBrush(Qt::white);
	right_layout->addWidget(m_preview, 1);
	auto hint = new QLabel(tr("Depois de inserir, clique no desenho para colocar a peça. Os bornes ficam na grade dos esquemas."), right);
	hint->setWordWrap(true);
	right_layout->addWidget(hint);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({240, 580});
	layout->addWidget(splitter, 1);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Inserir"));
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_tree, &QTreeWidget::currentItemChanged, this, &PartLibraryDialog::showEntry);
	connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
		if (!item->data(0, Qt::UserRole).toString().isEmpty())
			accept();
	});

		//Open on the piece used last
	const QString last = QSettings().value(QStringLiteral("elementeditor/part_library_last"), QStringLiteral("no")).toString();
	for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
		if ((*it)->data(0, Qt::UserRole).toString() == last)
			m_tree->setCurrentItem(*it);
	connect(this, &QDialog::accepted, this, [this]() {
		QSettings().setValue(QStringLiteral("elementeditor/part_library_last"), m_id);
	});
}

QDomDocument PartLibraryDialog::drawing() const
{
	return PartLibrary::build(m_id, m_values);
}

/**
	@brief PartLibraryDialog::showEntry
	The fields of the piece chosen, with its default values
*/
void PartLibraryDialog::showEntry()
{
	const QString id = m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
	const PartLibrary::Entry *entry = PartLibrary::find(m_entries, id);
	if (!entry)
		return;
	m_id = id;
	m_values = PartLibrary::defaults(*entry);

	while (m_form->rowCount())
		m_form->removeRow(0);
	for (const PartLibrary::Param &param : entry->params) {
		const QString key = param.key;
		QWidget *field = nullptr;
		switch (param.kind) {
			case PartLibrary::Param::Number: {
				auto spin = new QDoubleSpinBox(m_params);
				spin->setRange(param.minimum, param.maximum);
				spin->setSingleStep(param.step);
				spin->setDecimals(param.step < 1 ? 1 : 0);
				spin->setValue(param.value.toDouble());
				connect(spin, &QDoubleSpinBox::valueChanged, this, [this, key](double value) {
					m_values.insert(key, value);
					updatePreview();
				});
				field = spin;
				break;
			}
			case PartLibrary::Param::Text: {
				auto edit = new QLineEdit(param.value.toString(), m_params);
				connect(edit, &QLineEdit::textChanged, this, [this, key](const QString &text) {
					m_values.insert(key, text);
					updatePreview();
				});
				field = edit;
				break;
			}
			case PartLibrary::Param::Choice: {
				auto combo = new QComboBox(m_params);
				combo->addItems(param.choices);
				combo->setCurrentIndex(param.value.toInt());
				connect(combo, &QComboBox::currentIndexChanged, this, [this, key](int index) {
					m_values.insert(key, index);
					updatePreview();
				});
				field = combo;
				break;
			}
			case PartLibrary::Param::Check: {
				auto box = new QCheckBox(m_params);
				box->setChecked(param.value.toBool());
				connect(box, &QCheckBox::toggled, this, [this, key](bool checked) {
					m_values.insert(key, checked);
					updatePreview();
				});
				field = box;
				break;
			}
		}
		m_form->addRow(param.label, field);
	}
	updatePreview();
}

void PartLibraryDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	m_preview->fitInView(m_preview_scene->sceneRect(), Qt::KeepAspectRatio);
}

void PartLibraryDialog::updatePreview()
{
	m_preview_scene->clear();
	drawDefinition(m_preview_scene, drawing());
	const QRectF rect = m_preview_scene->itemsBoundingRect().adjusted(-10, -10, 10, 10);
	m_preview_scene->setSceneRect(rect);
	m_preview->fitInView(rect, Qt::KeepAspectRatio);
}
