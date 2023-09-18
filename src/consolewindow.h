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


#ifndef CONSOLEWINDOW_H
#define CONSOLEWINDOW_H

#include <QMainWindow>

namespace Ui {
class ConsoleWindow;
}

class ConsoleWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ConsoleWindow(QWidget *parent = nullptr);
    ~ConsoleWindow();

    void setShowMidiEvents(bool show);

signals:
    void showMidiEventsChanged(bool show);

public slots:
    void print(QString message);

private slots:
    void on_pushButton_Clear_clicked();
    void on_checkBox_ShowMidiEvents_clicked();

private:
    Ui::ConsoleWindow *ui;
    bool mFirstPrint = true;
};

#endif // CONSOLEWINDOW_H
