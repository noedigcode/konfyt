/******************************************************************************
 *
 * Copyright 2023 Gideon van der Kolf
 *
 * This file is part of Konfyt.
 *
 *     Konfyt is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Konfyt is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with Konfyt.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include "menuEntryWidget.h"

MenuEntryWidget::MenuEntryWidget(QString text, bool hasRemoveButton, QWidget *parent) :
    QWidget(parent)
{
    setContentsMargins(5,0,5,0);
    setAutoFillBackground(true); // For un/highlighting with backgroundRole

    layout = new QHBoxLayout(this);
    layout->setMargin(0);
    setLayoutSpacing(2);

    label = new QLabel(text, this);
    label->setStyleSheet("min-height: 1.4em; min-width: 100px;");
    layout->addWidget(label);

    if (hasRemoveButton) {
        QToolButton* delbtn = new QToolButton(this);
        delbtn->setText("X");
        delbtn->setAutoRaise(true); // Hide border
        delbtn->setToolTip("Remove");
        connect(delbtn, &QToolButton::clicked, this, &MenuEntryWidget::removeButtonClicked);

        layout->addWidget(delbtn);
    }

    setLayout(layout);

    highlight(false);
}

void MenuEntryWidget::setLayoutSpacing(int spacing)
{
    layout->setSpacing(spacing);
}

void MenuEntryWidget::setText(QString text)
{
    label->setText(text);
}

QString MenuEntryWidget::text()
{
    return label->text();
}

void MenuEntryWidget::highlight(bool hilite)
{
    if (hilite) {
        setBackgroundRole(QPalette::Highlight);
    } else {
        setBackgroundRole(QPalette::Window);
    }
}

void MenuEntryWidget::enterEvent(QEvent *event)
{
    highlight(true);

    event->ignore();
}

void MenuEntryWidget::leaveEvent(QEvent *event)
{
    highlight(false);

    event->ignore();
}
