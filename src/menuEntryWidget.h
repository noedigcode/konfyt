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

#ifndef MENUITEM_H
#define MENUITEM_H

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QWidget>

class MenuEntryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MenuEntryWidget(QString text = "", bool hasRemoveButton = false,
                      QWidget *parent = nullptr);

    void setLayoutSpacing(int spacing);
    void setText(QString text);
    QString text();

signals:
    void removeButtonClicked();

private:
    QHBoxLayout* layout;
    QLabel* label;

    void highlight(bool hilite);

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
};

#endif // MENUITEM_H
