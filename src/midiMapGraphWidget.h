#ifndef MIDIMAPGRAPHWIDGET_H
#define MIDIMAPGRAPHWIDGET_H

#include "konfytMidiFilter.h"

#include <QWidget>

class MidiMapGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MidiMapGraphWidget(QWidget *parent = nullptr);

    void setMapping(KonfytMidiMapping m);

private:
    KonfytMidiMapping mapping;

    void paintEvent(QPaintEvent *event);

    int mapx(int value);
    int mapy(int value);
    int inval(int index);
    int outval(int index);
};

#endif // MIDIMAPGRAPHWIDGET_H
