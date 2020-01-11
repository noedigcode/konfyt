/******************************************************************************
 *
 * Copyright 2020 Gideon van der Kolf
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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    QString txt = ui->textBrowser->document()->toHtml();
    txt.replace(REPLACE_TXT_APP_VERSION, APP_VERSION);
    ui->textBrowser->document()->setHtml(txt);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::setExtraVersionText(QString txt)
{
    QString extra = txt.replace("\n", "<br>");
    QString html = ui->textBrowser->document()->toHtml();
    html.replace(REPLACE_TXT_MORE_VERSION, extra);
    ui->textBrowser->document()->setHtml(html);
}

void AboutDialog::on_pushButton_clicked()
{
    this->hide();
}

