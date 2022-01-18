/******************************************************************************
 *
 * Copyright 2021 Gideon van der Kolf
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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include "konfytDefines.h"

#include <QWidget>

#define REPLACE_TXT_APP_VERSION "%APP_VERSION%"
#define REPLACE_TXT_APP_YEAR "%APP_YEAR%"
#define REPLACE_TXT_MORE_VERSION "%MORE_VERSION_TEXT%"

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QWidget
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = 0);
    ~AboutDialog();
    void setExtraVersionText(QString txt);

private slots:
    void on_pushButton_clicked();

private:
    Ui::AboutDialog *ui;
};

#endif // ABOUTDIALOG_H
