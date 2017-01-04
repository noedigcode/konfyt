#ifndef KONFYT_LAYER_WIDGET_H
#define KONFYT_LAYER_WIDGET_H

#include <QWidget>
#include "konfytPatchLayer.h"
#include <QListWidgetItem>
#include <QMenu>
#include "konfytProject.h"
#include <QPainter>
#include <QBrush>
#include <QRect>

namespace Ui {
class guiLayerItem;
}

class konfytLayerWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit konfytLayerWidget(QWidget *parent = 0);
    ~konfytLayerWidget();

    konfytProject* project; // Pointer to current project to get bus and port naming info

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
};

#endif // KONFYT_LAYER_WIDGET_H
