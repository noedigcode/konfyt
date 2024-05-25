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

#include "GidListWidget.h"

#include <QMimeData>
#include <QPainter>


GidListWidget::GidListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setDragEnabled(true);
    setAcceptDrops(true);
}

void GidListWidget::setDropLineColor(QColor color)
{
    mDropLineColor = color;
}

void GidListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Record start position of potential drag/drop operation
        mDragStartPos = event->pos();
    }
    QListWidget::mousePressEvent(event);
}

void GidListWidget::mouseMoveEvent(QMouseEvent* event)
{
    // Start drag/drop operation if left button has dragged further than the startDragDistance
    if (event->buttons() & Qt::LeftButton) {
        int dist = (event->pos() - mDragStartPos).manhattanLength();
        if (dist >= QApplication::startDragDistance()) {
            startDrag();
        }
    }
}

void GidListWidget::paintEvent(QPaintEvent* event)
{
    // Normal drawing of widget
    QListWidget::paintEvent(event);

    // Draw drag/drop line
    if (mDragItem) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(mDropLineColor);
        painter.drawLine(mDropLine);
    }
}

void GidListWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (mDragItem) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}

void GidListWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (mDragItem) {

        updateDropLineLocation(event->pos());
        // Repaint the widget to draw the line
        viewport()->update();

        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}

void GidListWidget::dropEvent(QDropEvent* event)
{
    if (mDragItem) {

        // Calculate index to move item to
        int to = this->count();
        QListWidgetItem* itemUnder = itemAt(event->pos());
        if (itemUnder) {
            to = row(itemUnder);
            if (event->pos().y() > visualItemRect(itemUnder).center().y()) {
                to += 1;
            }
        }
        int from = this->row(mDragItem);
        if (to > from) { to -= 1; }

        if (from != to) {
            this->blockSignals(true); // To block current item changed signals
            takeItem(from);
            insertItem(to, mDragItem);
            setCurrentItem(mDragItem);
            this->blockSignals(false);
            emit itemMoved(mDragItem, from, to);
            emit currentRowChanged(to);
        }

        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}

void GidListWidget::startDrag()
{
    mDragItem = itemAt(mDragStartPos);
    if (!mDragItem) { return; }

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    // Mime data left empty
    drag->setMimeData(mimeData);

    drag->exec(Qt::MoveAction);
    // End of drag/drop operation
    mDragItem = nullptr;
    viewport()->update();
}

void GidListWidget::updateDropLineLocation(QPoint pos)
{
    bool lineBelow = true;
    QListWidgetItem* itemUnder = itemAt(pos);
    int row = this->row(itemUnder);

    if (itemUnder) {
        QRect rect = visualItemRect(itemUnder);
        if (pos.y() > rect.center().y()) {
            // Mouse below center, draw line below item.
            lineBelow = true;
        } else if (row > 0) {
            // Line should be above item, but if possible, draw it below the
            // previous item, to prevent the line jumping up and down while moving.
            itemUnder = this->item(row-1);
            lineBelow = true;
        } else {
            // First item, have to draw line above the item.
            lineBelow = false;
        }
    } else {
        // No item under. Mouse is probably below all items in empty space.
        // Draw line below the last item.
        itemUnder = this->item(this->count() - 1);
        lineBelow = true;
    }

    if (itemUnder) {
        QRect rect = visualItemRect(itemUnder);
        if (lineBelow) {
            mDropLine.setP1(rect.bottomLeft());
            mDropLine.setP2(rect.bottomRight());
        } else {
            mDropLine.setP1(rect.topLeft());
            mDropLine.setP2(rect.topRight());
        }
    } else {
        // No item. Just draw line at top.
        mDropLine.setLine(0, 0, this->width(), 0);
    }
}
