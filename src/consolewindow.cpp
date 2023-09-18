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


#include "consolewindow.h"
#include "ui_consolewindow.h"

#include <QScrollBar>


ConsoleWindow::ConsoleWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ConsoleWindow)
{
    ui->setupUi(this);
}

ConsoleWindow::~ConsoleWindow()
{
    delete ui;
}

void ConsoleWindow::setShowMidiEvents(bool show)
{
    ui->checkBox_ShowMidiEvents->setChecked(show);
}

void ConsoleWindow::print(QString message)
{
    ui->textBrowser->append(message);

    /* Ensure textBrowser scrolls to maximum when it is filled with text the
     * first time. Thereafter, it should keep doing this by itself. */
    QScrollBar* v = ui->textBrowser->verticalScrollBar();
    if (mFirstPrint) {
        if (v->value() != v->maximum()) {
            v->setValue(v->maximum());
            mFirstPrint = false;
        }
    }
}

void ConsoleWindow::on_pushButton_Clear_clicked()
{
    ui->textBrowser->clear();
}

void ConsoleWindow::on_checkBox_ShowMidiEvents_clicked()
{
    emit showMidiEventsChanged(ui->checkBox_ShowMidiEvents->isChecked());
}

