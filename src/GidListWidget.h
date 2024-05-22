/******************************************************************************
 *
 * Copyright 2024 Gideon van der Kolf
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

/* GidListWidget
 *
 * GVDK May 2024
 *
 * A QListWidget that attempts to implement an easier to use internal move
 * drag and drop solution with an itemMoved() signal.
 *
 * This was created in response to the internal move drag and drop of the stock
 * QListWidget with the model's rowsMoved() signal not behaving as expected,
 * especially when an item is dropped on itself.
 */

#ifndef GIDLISTWIDGET_H
#define GIDLISTWIDGET_H

#include <QDrag>
#include <QListWidget>
#include <QMouseEvent>
#include <QWidget>


class GidListWidget : public QListWidget
{
    Q_OBJECT

public:
    GidListWidget(QWidget *parent = nullptr);

signals:
    void itemMoved(QListWidgetItem* item, int from, int to);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QPoint mDragStartPos;
    QLine mDropLine;
    QListWidgetItem* mDragItem = nullptr;

    void startDrag();
    void updateDropLineLocation(QPoint pos);
};


#endif // GIDLISTWIDGET_H
