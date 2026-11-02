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
#include "selectionpanel.h"

#include "../../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../elementalign.h"
#include "../elementscene.h"
#include "../graphicspart/partarc.h"
#include "../graphicspart/partdynamictextfield.h"
#include "../graphicspart/partellipse.h"
#include "../graphicspart/partline.h"
#include "../graphicspart/partpolygon.h"
#include "../graphicspart/partrectangle.h"
#include "../graphicspart/partterminal.h"
#include "../graphicspart/parttext.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

namespace {
	struct Kind
	{
		int type;
		QString (*name)(int count);
	};

		///The kinds of parts, in the order they are listed
	QList<Kind> kinds()
	{
		return {
			{PartLine::Type,      [](int n) { return SelectionPanel::tr("%n linha(s)", "", n); }},
			{PartRectangle::Type, [](int n) { return SelectionPanel::tr("%n retângulo(s)", "", n); }},
			{PartEllipse::Type,   [](int n) { return SelectionPanel::tr("%n elipse(s)", "", n); }},
			{PartArc::Type,       [](int n) { return SelectionPanel::tr("%n arco(s)", "", n); }},
			{PartPolygon::Type,   [](int n) { return SelectionPanel::tr("%n polígono(s)", "", n); }},
			{PartTerminal::Type,  [](int n) { return SelectionPanel::tr("%n borne(s)", "", n); }},
			{PartText::Type,      [](int n) { return SelectionPanel::tr("%n texto(s)", "", n); }},
			{PartDynamicTextField::Type, [](int n) { return SelectionPanel::tr("%n campo(s) de texto", "", n); }},
		};
	}

	const QList<Qet::Orientation> orientations {Qet::North, Qet::South, Qet::West, Qet::East};

	bool isText(const QGraphicsItem *item)
	{
		return item->type() == PartText::Type || item->type() == PartDynamicTextField::Type;
	}
}

SelectionPanel::SelectionPanel(ElementScene *scene, QWidget *parent) :
	QWidget(parent),
	m_scene(scene)
{
	auto layout = new QVBoxLayout(this);
	m_summary = new QLabel(this);
	m_summary->setWordWrap(true);
	layout->addWidget(m_summary);

	auto form = new QFormLayout();
	m_x = new QDoubleSpinBox(this);
	m_y = new QDoubleSpinBox(this);
	for (QDoubleSpinBox *spin : {m_x, m_y}) {
		spin->setRange(-10000, 10000);
		spin->setDecimals(1);
		spin->setKeyboardTracking(false);
		connect(spin, &QDoubleSpinBox::valueChanged, this, &SelectionPanel::moveTo);
	}
	m_x->setToolTip(tr("Lado esquerdo da seleção; mudar move tudo junto"));
	m_y->setToolTip(tr("Topo da seleção; mudar move tudo junto"));
	form->addRow(tr("X :"), m_x);
	form->addRow(tr("Y :"), m_y);
	m_size = new QLabel(this);
	form->addRow(tr("Tamanho :"), m_size);

	m_orientation_label = new QLabel(tr("Saída dos bornes :"), this);
	m_orientation = new QComboBox(this);
	m_orientation->addItems({tr("(várias)"), tr("Para cima"), tr("Para baixo"), tr("Para a esquerda"), tr("Para a direita")});
	connect(m_orientation, QOverload<int>::of(&QComboBox::activated), this, &SelectionPanel::setOrientation);
	form->addRow(m_orientation_label, m_orientation);

	m_text_size_label = new QLabel(tr("Tamanho dos textos :"), this);
	m_text_size = new QSpinBox(this);
	m_text_size->setRange(0, 200);
	m_text_size->setSpecialValueText(tr("(vários)"));
	m_text_size->setKeyboardTracking(false);
	connect(m_text_size, &QSpinBox::valueChanged, this, &SelectionPanel::setTextSize);
	form->addRow(m_text_size_label, m_text_size);
	layout->addLayout(form);

	auto filters_title = new QLabel(tr("Manter selecionados só:"), this);
	layout->addWidget(filters_title);
	m_filters = new QWidget(this);
	m_filters_layout = new QGridLayout(m_filters);
	m_filters_layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_filters);
	layout->addStretch();

	connect(&scene->undoStack(), &QUndoStack::indexChanged, this, [this]() {
		if (isVisible())
			refresh();
	});
}

/**
	@brief SelectionPanel::refresh
	Show the selection as it is now
*/
void SelectionPanel::refresh()
{
	if (!m_scene)
		return;
	const QList<QGraphicsItem *> selected = m_scene->selectedItems();

	QStringList counts;
	while (QLayoutItem *item = m_filters_layout->takeAt(0)) {
			//Later: refresh() may run while a button of the list is clicked
		item->widget()->hide();
		item->widget()->deleteLater();
		delete item;
	}
	int column = 0, row = 0;
	for (const Kind &kind : kinds()) {
		int n = 0;
		for (QGraphicsItem *item : selected)
			n += item->type() == kind.type;
		if (!n)
			continue;
		counts << kind.name(n);
		auto button = new QPushButton(kind.name(n), m_filters);
		const int type = kind.type;
		connect(button, &QPushButton::clicked, this, [this, type]() { keepOnly(type); });
		m_filters_layout->addWidget(button, row, column);
		if (++column == 2) {
			column = 0;
			++row;
		}
	}
	m_summary->setText(tr("<b>Seleção</b>: %1").arg(counts.join(QStringLiteral(", "))));

	const QRectF rect = ElementAlign::geometry(m_scene, selected);
	for (QDoubleSpinBox *spin : {m_x, m_y}) {
		const QSignalBlocker blocker(spin);
		spin->setValue(spin == m_x ? rect.left() : rect.top());
	}
	m_size->setText(tr("%1 × %2").arg(qRound(rect.width() * 10) / 10.0).arg(qRound(rect.height() * 10) / 10.0));

		//What the terminals share
	int orientation = -1;
	bool terminals = false;
	for (QGraphicsItem *item : selected) {
		if (auto terminal = qgraphicsitem_cast<PartTerminal *>(item)) {
			const int index = orientations.indexOf(terminal->orientation()) + 1;
			orientation = !terminals || orientation == index ? index : 0;
			terminals = true;
		}
	}
	m_orientation_label->setVisible(terminals);
	m_orientation->setVisible(terminals);
	m_orientation->setCurrentIndex(qMax(0, orientation));

		//What the texts share
	int size = -1;
	bool texts = false;
	for (QGraphicsItem *item : selected) {
		if (isText(item)) {
			const int point_size = item->toGraphicsObject()->property("font").value<QFont>().pointSize();
			size = !texts || size == point_size ? point_size : 0;
			texts = true;
		}
	}
	m_text_size_label->setVisible(texts);
	m_text_size->setVisible(texts);
	const QSignalBlocker blocker(m_text_size);
	m_text_size->setValue(qMax(0, size));
}

/**
	@brief SelectionPanel::moveTo
	Move the selection so its top left corner is the one typed
*/
void SelectionPanel::moveTo()
{
	if (!m_scene)
		return;
	const QList<QGraphicsItem *> selected = m_scene->selectedItems();
	const QRectF rect = ElementAlign::geometry(m_scene, selected);
	if (QUndoCommand *undo = ElementAlign::translate(selected, QPointF(m_x->value(), m_y->value()) - rect.topLeft(),
							 tr("Mover a seleção")))
		m_scene->undoStack().push(undo);
}

void SelectionPanel::keepOnly(int type)
{
	if (!m_scene)
		return;
	for (QGraphicsItem *item : m_scene->selectedItems())
		if (item->type() != type)
			item->setSelected(false);
}

void SelectionPanel::setOrientation(int index)
{
	if (!m_scene || index <= 0)
		return;
	auto undo = new QUndoCommand(tr("Saída dos bornes"));
	for (QGraphicsItem *item : m_scene->selectedItems())
		if (auto terminal = qgraphicsitem_cast<PartTerminal *>(item))
			if (terminal->orientation() != orientations.at(index - 1))
				new QPropertyUndoCommand(terminal, "orientation", QVariant::fromValue(terminal->orientation()),
							 QVariant::fromValue(orientations.at(index - 1)), undo);
	if (undo->childCount())
		m_scene->undoStack().push(undo);
	else
		delete undo;
}

void SelectionPanel::setTextSize(int size)
{
	if (!m_scene || size <= 0)
		return;
	auto undo = new QUndoCommand(tr("Tamanho dos textos"));
	for (QGraphicsItem *item : m_scene->selectedItems()) {
		if (!isText(item))
			continue;
		QGraphicsObject *text = item->toGraphicsObject();
		const QFont font = text->property("font").value<QFont>();
		if (font.pointSize() == size)
			continue;
		QFont sized = font;
		sized.setPointSize(size);
		new QPropertyUndoCommand(text, "font", font, sized, undo);
	}
	if (undo->childCount())
		m_scene->undoStack().push(undo);
	else
		delete undo;
}
