/******************************************************************************
 *
 * Copyright 2020 Gideon van der Kolf
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

#include "konfytPatchLayer.h"
#include "konfytProject.h"

#include <QWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QBrush>
#include <QRect>
#include <QTimer>


namespace Ui {
class KonfytLayerWidget;
}

class KonfytLayerWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit KonfytLayerWidget(QWidget *parent = 0);
    ~KonfytLayerWidget();

    KonfytProject* project; // Pointer to current project to get bus and port naming info

    // This function has to be called before using the object.
    void initLayer(KfPatchLayerWeakPtr patchLayer, QListWidgetItem* listItem);

    // Update the layer widget
    void refresh();

    void setSliderGain(float newGain);
    void setSoloButton(bool solo);
    void setMuteButton(bool mute);
    KfPatchLayerWeakPtr getPatchLayer();
    QListWidgetItem* getListWidgetItem();
    QString getFilePath();

    void indicateMidi();
    void indicateSustain(bool sustain);
    void indicatePitchbend(bool pitchbend);
    void indicateAudioLeft(float value);
    void indicateAudioRight(float value);
    
private:
    Ui::KonfytLayerWidget *ui;

    KfPatchLayerWeakPtr mPatchLayer;
    QListWidgetItem* mListWidgetItem;
    QString mFilepath;

    void setUpGUI();
    float background_rectLeft;
    float background_rectRight;
    void changeBackground(int min, int max);
    void updateBackgroundFromFilter();

    void paintEvent(QPaintEvent *);

    QTimer midiIndicateTimer;
    bool midiIndicate = false;
    bool midiIndicateSustain = false;
    bool midiIndicatePitchbend = false;

    QTimer audioIndicateTimer;
    float audioIndicateStep = 0.1;
    float audioLeftValue = 0;
    float audioRightValue = 0;
    const float audioZeroLimit = 1E-3;

signals:
    void slider_moved_signal(KonfytLayerWidget* layerItem, float gain);
    void solo_clicked_signal(KonfytLayerWidget* layerItem, bool solo);
    void mute_clicked_signal(KonfytLayerWidget* layerItem, bool mute);
    void rightToolbutton_clicked_signal(KonfytLayerWidget* layerItem);
    void leftToolbutton_clicked_signal(KonfytLayerWidget* layerItem);
    void sendMidiEvents_clicked_signal(KonfytLayerWidget* layerItem);

private slots:
    void on_toolButton_left_clicked();
    void on_gainSlider_sliderMoved(int position);
    void on_gainSlider_valueChanged(int value);
    void on_toolButton_solo_clicked();
    void on_toolButton_mute_clicked();
    void on_toolButton_right_clicked();
    void on_toolButton_sendEvents_clicked();

    void midiIndicateTimerEvent();
    void audioIndicateTimerEvent();
};

#endif // KONFYT_LAYER_WIDGET_H
