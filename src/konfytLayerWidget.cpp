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

#include "konfytLayerWidget.h"
#include "ui_konfytLayerWidget.h"

konfytLayerWidget::konfytLayerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::guiLayerItem)
{
    ui->setupUi(this);

    background_rectLeft = 0;
    background_rectRight = 0;
}

konfytLayerWidget::~konfytLayerWidget()
{
    delete ui;
}


void konfytLayerWidget::paintEvent(QPaintEvent* /*e*/)
{
    QColor colorFG = QColor(24, 87, 127, 255);
    QColor colorBG = QColor(0,0,0,0);

    QPainter p(this);

    QBrush b;
    b.setStyle(Qt::SolidPattern);

    QRect r;
    r.setHeight(this->height()-2);

    // Background rectangle
    r.setWidth(this->width()-1);
    b.setColor(colorBG);
    p.setBrush(b);
    p.drawRect(r);

    // Foreground rectangle
    r.setLeft( background_rectLeft*(float)(this->width()-2) );
    r.setRight( background_rectRight*(this->width()-2) );
    //r.setWidth( (float)(this->width()-2)*(background_rectRight-background_rectLeft) );
    b.setColor(colorFG);
    p.setBrush(b);
    p.drawRect(r);
}

void konfytLayerWidget::on_toolButton_clicked()
{
    emit toolbutton_clicked_signal(this);
}

// This function has to be called before the object can be used.
void konfytLayerWidget::initLayer(KonfytPatchLayer newg, QListWidgetItem *newItem)
{
    this->g = newg;
    this->listWidgetItem = newItem;

    // Set up GUI
    setUpGUI();
}

void konfytLayerWidget::setLayerItem(KonfytPatchLayer newg)
{
    this->g = newg;
    setUpGUI();
}

// Set up the widgets corresponding to the layer item.
void konfytLayerWidget::setUpGUI()
{
    filepath = "";

    if (g.getLayerType() == KonfytLayerType_SoundfontProgram) {

        filepath = g.soundfontData.program.parent_soundfont;
        // Display soundfont and program name
        ui->lineEdit->setText(g.soundfontData.program.parent_soundfont + "/" +
                              g.soundfontData.program.name);
        ui->lineEdit->setToolTip(ui->lineEdit->text());
        // Indicate note range of the midi filter
        this->updateBackgroundFromFilter();
        // Volume
        ui->gainSlider->setVisible(true);


    } else if (g.getLayerType() == KonfytLayerType_Sfz) {

        filepath = g.sfzData.path;

        // Display sfz name
        ui->lineEdit->setText(g.sfzData.path);
        ui->lineEdit->setToolTip( g.sfzData.name + ": " + g.sfzData.path );

        // Indicate note range of the midi filter
        this->updateBackgroundFromFilter();
        // Volume
        ui->gainSlider->setVisible(true);

    } else if (g.getLayerType() == KonfytLayerType_MidiOut) {

        // Display port id and name
        int portId = g.midiOutputPortData.portIdInProject;
        QString text = "MIDI Out " + n2s(portId);
        if (project != NULL) {
            // TODO: setErrorMessage should not be done here, it should be set
            //       in the engine when loading the patch
            if (project->midiOutPort_exists(portId)) {
                text = text + ": " + project->midiOutPort_getPort(portId).portName;
            } else {
                g.setErrorMessage("No MIDI out port " + n2s(portId) + " in project.");
            }
        }
        ui->lineEdit->setText( text );
        // Indicate note range of the midi filter
        this->updateBackgroundFromFilter();
        // Volume
        ui->gainSlider->setVisible(false);
        // Use bus button as MIDI output channel
        int outchan = g.getMidiFilter().outChan;
        if (outchan >= 0) {
            ui->toolButton_bus->setText( n2s(outchan+1));
        } else {
            ui->toolButton_bus->setText( "-" );
        }
        ui->toolButton_bus->setToolTip("MIDI Output Channel");

    } else if (g.getLayerType() == KonfytLayerType_AudioIn ) {

        // Display port id and name
        int portId = g.audioInPortData.portIdInProject;
        QString text = "Audio In " + n2s(portId);
        if (project != NULL) {
            // TODO: setErrorMessage should not be done here, it should be set
            //       in the engine when loading the patch
            if (project->audioInPort_exists(portId)) {
                text = text + ": " + project->audioInPort_getPort(portId).portName;
            } else {
                g.setErrorMessage("No audio in port " + n2s(portId) + " in project.");
            }
        }
        ui->lineEdit->setText( text );
        changeBackground(0,127);
        // Volume
        ui->gainSlider->setVisible(true);

    } else {

        ui->lineEdit->setText("ERROR: Layer not initialised.");
        ui->gainSlider->setVisible(false);

    }

    // Volume
    this->setSliderGain(g.getGain());
    // Solo and mute
    ui->toolButton_solo->setChecked(g.isSolo());
    ui->toolButton_mute->setChecked(g.isMute());

    // Bus button
    if ( (project != NULL) && (g.getLayerType() != KonfytLayerType_MidiOut) ) {
        if ( project->audioBus_exists(g.busIdInProject) ) {
            ui->toolButton_bus->setText( n2s(g.busIdInProject) );
            ui->toolButton_bus->setToolTip("Bus: " + project->audioBus_getBus(g.busIdInProject).busName);
            ui->toolButton_bus->setStyleSheet("");
        } else {
            ui->toolButton_bus->setText( n2s(g.busIdInProject) + "!" );
            ui->toolButton_bus->setToolTip("Bus does not exist in this project.");
            ui->toolButton_bus->setStyleSheet("background-color: red;");
        }
    }

    // Input-side tool button
    if ( (project != NULL) && (g.getLayerType() != KonfytLayerType_AudioIn) ) {
        if (project->midiInPort_exists(g.midiInPortIdInProject)) {
            QString btnTxt = QString("%1:").arg(g.midiInPortIdInProject);
            QString tooltip = "MIDI In Port: " + project->midiInPort_getPort(g.midiInPortIdInProject).portName;
            tooltip.append("\nMIDI Channel: ");
            int inChan = g.getMidiFilter().inChan;
            if (inChan < 0) {
                btnTxt.append("A");
                tooltip.append("All");
            } else {
                btnTxt.append(n2s(inChan+1));
                tooltip.append(n2s(inChan+1));
            }
            ui->toolButton->setText( btnTxt );
            ui->toolButton->setToolTip(tooltip);
            ui->toolButton->setStyleSheet("");
        } else {
            ui->toolButton->setText( n2s(g.midiInPortIdInProject) + "!" );
            ui->toolButton->setToolTip("MIDI In Port does not exist in this project.");
            ui->toolButton->setStyleSheet("background-color: red;");
        }
    }

    // MIDI Events Send button
    ui->toolButton_sendEvents->setVisible( g.midiSendList.count() > 0 );

    // If the layer has an error (marked when loading in patchEngine), indicate it.
    if (g.hasError()) {

        ui->lineEdit->setText("ERROR: " + g.getErrorMessage());

        ui->gainSlider->setVisible(false);
        ui->toolButton_solo->setVisible(false);
        ui->toolButton_mute->setVisible(false);
        ui->toolButton_bus->setVisible(false);

        ui->lineEdit->setProperty("konfytError", true);
        ui->lineEdit->style()->unpolish(ui->lineEdit);
        ui->lineEdit->style()->polish(ui->lineEdit);

    }
}

// Change the background to indicate midi filter zone.
void konfytLayerWidget::updateBackgroundFromFilter()
{
    KonfytMidiFilter filter = g.getMidiFilter();
    KonfytMidiFilterZone z = filter.zone;
    this->changeBackground(z.lowNote, z.highNote);
}

// Change the background depending on the min and max values (between 0 and 127),
// used to indicate the range/zone of a midi filter.
void konfytLayerWidget::changeBackground(int min, int max)
{
    max++; // inclusive!
    background_rectLeft = (float)min/127.0;
    if (max >= 127) { background_rectRight = 1; }
    else { background_rectRight = (float)max/127.0; }
    this->repaint();
}

KonfytPatchLayer konfytLayerWidget::getPatchLayerItem()
{
    return this->g;
}

QListWidgetItem* konfytLayerWidget::getListWidgetItem()
{
    return this->listWidgetItem;
}

QString konfytLayerWidget::getFilePath()
{
    return filepath;
}

/* Set slider gain from outside the class, e.g. if gain was changed by some
 * other event like a midi trigger. */
void konfytLayerWidget::setSliderGain(float newGain)
{
    ui->gainSlider->setValue(newGain*ui->gainSlider->maximum());
    g.setGain(newGain);
}

/* Set solo button from outside the class, e.g. if it was changed by some
 * other event like a midi trigger. */
void konfytLayerWidget::setSoloButton(bool solo)
{
    ui->toolButton_solo->setChecked(solo);
    g.setSolo(solo);
}

/* Set mute button from outside the class, e.g. if it was changed by some
 * other event like a midi trigger. */
void konfytLayerWidget::setMuteButton(bool mute)
{
    ui->toolButton_mute->setChecked(mute);
    g.setMute(mute);
}

void konfytLayerWidget::on_gainSlider_sliderMoved(int position)
{
    float gain = (float)position/(float)ui->gainSlider->maximum();
    g.setGain(gain);
    emit slider_moved_signal(this, gain);
}

void konfytLayerWidget::on_gainSlider_valueChanged(int /*value*/)
{
    // Update slider tooltip
    float tooltip_gain = (float)ui->gainSlider->value()/(float)ui->gainSlider->maximum();
    int tooltip_midi = tooltip_gain*127.0;
    ui->gainSlider->setToolTip("Gain: " +
                               QString::number( tooltip_gain, 'g', 2 ) +
                               ", MIDI Value: " +
                               n2s( tooltip_midi ) );
}

/* Solo button: Clicked. */
void konfytLayerWidget::on_toolButton_solo_clicked()
{
    bool solo = ui->toolButton_solo->isChecked();
    g.setSolo(solo);
    emit solo_clicked_signal(this, solo);
}

/* Mute button: Clicked. */
void konfytLayerWidget::on_toolButton_mute_clicked()
{
    bool mute = ui->toolButton_mute->isChecked();
    g.setMute(mute);
    emit mute_clicked_signal(this, mute);
}

void konfytLayerWidget::on_toolButton_bus_clicked()
{
    emit bus_clicked_signal(this);
}

void konfytLayerWidget::on_toolButton_sendEvents_clicked()
{
    emit sendMidiEvents_clicked_signal(this);
}
