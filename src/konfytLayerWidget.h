/******************************************************************************
 *
 * Copyright 2019 Gideon van der Kolf
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

#ifndef KONFYT_LAYER_WIDGET_H
#define KONFYT_LAYER_WIDGET_H

#include <QWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QBrush>
#include <QRect>

#include "konfytPatchLayer.h"
#include "konfytProject.h"


namespace Ui {
class guiLayerItem;
}

class konfytLayerWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit konfytLayerWidget(QWidget *parent = 0);
    ~konfytLayerWidget();

    KonfytProject* project; // Pointer to current project to get bus and port naming info

    // This function has to be called before using the object.
    void initLayer(KonfytPatchLayer newg, QListWidgetItem* newItem);

    // This function is for updating the LayerItem
    void setLayerItem(KonfytPatchLayer newg);

    void updateBackgroundFromFilter();
    void setSliderGain(float newGain);
    void setSoloButton(bool solo);
    void setMuteButton(bool mute);
    KonfytPatchLayer getPatchLayerItem();
    QListWidgetItem* getListWidgetItem();
    QString getFilePath();
    
private:
    Ui::guiLayerItem *ui;

    KonfytPatchLayer g;
    QListWidgetItem* listWidgetItem;
    QString filepath;

    void setUpGUI();
    float background_rectLeft;
    float background_rectRight;
    void changeBackground(int min, int max);

    void paintEvent(QPaintEvent *);


signals:
    void slider_moved_signal(konfytLayerWidget* layerItem, float gain);
    void solo_clicked_signal(konfytLayerWidget* layerItem, bool solo);
    void mute_clicked_signal(konfytLayerWidget* layerItem, bool mute);
    void bus_clicked_signal(konfytLayerWidget* layerItem);
    void toolbutton_clicked_signal(konfytLayerWidget* layerItem);

private slots:
    void on_toolButton_clicked();
    void on_gainSlider_sliderMoved(int position);

    void on_gainSlider_valueChanged(int value);
    void on_toolButton_solo_clicked();
    void on_toolButton_mute_clicked();
    void on_toolButton_bus_clicked();
};

#endif // KONFYT_LAYER_WIDGET_H
