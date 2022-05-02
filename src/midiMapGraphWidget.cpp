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
