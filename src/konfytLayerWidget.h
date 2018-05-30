/******************************************************************************
 *
 * Copyright 2017 Gideon van der Kolf
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
    void initLayer(konfytPatchLayer newg, QListWidgetItem* newItem);

    // This function is for updating the LayerItem
    void setLayerItem(konfytPatchLayer newg);

    void updateBackgroundFromFilter();
    void setSliderGain(float newGain);
    void setSoloButton(bool solo);
    void setMuteButton(bool mute);
    konfytPatchLayer getPatchLayerItem();
    QListWidgetItem* getListWidgetItem();
    
private:
    Ui::guiLayerItem *ui;

    konfytPatchLayer g;
    QListWidgetItem* listWidgetItem;
    QMenu popupMenu;
    QString filepath;

    void setUpGUI();
    float background_rectLeft;
    float background_rectRight;
    void changeBackground(int min, int max);

    void paintEvent(QPaintEvent *);


signals:
    void slider_moved_signal(konfytLayerWidget* layerItem, float gain);
    void remove_clicked_signal(konfytLayerWidget* layerItem);
    void filter_clicked_signal(konfytLayerWidget* layerItem);
    void solo_clicked_signal(konfytLayerWidget* layerItem, bool solo);
    void mute_clicked_signal(konfytLayerWidget* layerItem, bool mute);
    void bus_clicked_signal(konfytLayerWidget* layerItem);
    void reload_clicked_signal(konfytLayerWidget* layerItem);
    void openInFileManager_clicked_signal(konfytLayerWidget* layerItem, QString filepath);

private slots:
    void on_toolButton_clicked();
    void on_gainSlider_sliderMoved(int position);

    void on_actionEdit_Filter_triggered();
    void on_actionRemove_Layer_triggered();
    void on_gainSlider_valueChanged(int value);
    void on_toolButton_solo_clicked();
    void on_toolButton_mute_clicked();
    void on_toolButton_bus_clicked();
    void on_actionReload_Layer_triggered();
    void on_actionOpen_in_File_Manager_triggered();
};

#endif // KONFYT_LAYER_WIDGET_H
