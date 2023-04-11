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

#include "midiMapGraphWidget.h"

#include <QPainter>

MidiMapGraphWidget::MidiMapGraphWidget(QWidget *parent) : QWidget(parent)
{

}

void MidiMapGraphWidget::setMapping(KonfytMidiMapping m)
{
    mapping = m;
    repaint();
}

void MidiMapGraphWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    QPen bgpen(Qt::NoPen);
    QBrush bg(QColor(40, 40, 40));
    p.setPen(bgpen);
    p.setBrush(bg);
    p.drawRect(0, 0, this->width(), this->height());

    // Foreground
    QPen fgpen(QColor(Qt::white));
    fgpen.setWidth(2);
    p.setPen(fgpen);

    QPoint points[128];
    for (int i=0; i < 128; i++) {
        points[i].setX(mapx(i));
        points[i].setY(mapy(mapping.map(i)));
    }
    p.drawPolyline(points, 128);
}

int MidiMapGraphWidget::mapx(int value)
{
    return (float)value / 127.0 * this->width();
}

int MidiMapGraphWidget::mapy(int value)
{
    return this->height() - (float)value / 127.0 * this->height();
}

int MidiMapGraphWidget::inval(int index)
{
    return mapping.inNodes.value(index);
}

int MidiMapGraphWidget::outval(int index)
{
    return mapping.outNodes.value(index);
}
