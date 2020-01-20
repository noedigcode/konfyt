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

#include "konfytLayerWidget.h"
#include "ui_konfytLayerWidget.h"

KonfytLayerWidget::KonfytLayerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::KonfytLayerWidget)
{
    ui->setupUi(this);

    background_rectLeft = 0;
    background_rectRight = 0;

    connect(&midiIndicateTimer, &QTimer::timeout,
            this, &KonfytLayerWidget::midiIndicateTimerEvent);
}

KonfytLayerWidget::~KonfytLayerWidget()
{
    delete ui;
}

void KonfytLayerWidget::paintEvent(QPaintEvent* /*e*/)
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
    b.setColor(colorFG);
    p.setBrush(b);
    p.drawRect(r);

    // MIDI indicator
    if (midiIndicate) {
        int len = 20;
        QLinearGradient grad(0,0,len,0);
        grad.setColorAt(0, Qt::green);
        grad.setColorAt(1, Qt::transparent);
        QBrush b(grad);
        p.setBrush(b);
        p.setPen(Qt::NoPen);

        p.drawRect(0,0,len,this->height()-1);
    }

    // Sustain and pitchbend indication
    if (midiIndicate) { p.setPen(Qt::black); }
    else { p.setPen(Qt::white); }
    if (midiIndicateSustain) {
        p.drawText(QRectF(1,1,this->height()/2, this->height()/2),"S");
    }
    if (midiIndicatePitchbend) {
        p.drawText(QRectF(1,this->height()/2,this->height()/2, this->height()/2),"P");
    }
}

void KonfytLayerWidget::on_toolButton_left_clicked()
{
    emit leftToolbutton_clicked_signal(this);
}

// This function has to be called before the object can be used.
void KonfytLayerWidget::initLayer(KonfytPatchLayer newg, QListWidgetItem *newItem)
{
    this->g = newg;
    this->listWidgetItem = newItem;

    // Set up GUI
    setUpGUI();
}

void KonfytLayerWidget::setLayerItem(KonfytPatchLayer newg)
{
    this->g = newg;
    setUpGUI();
}

// Set up the widgets corresponding to the layer item.
void KonfytLayerWidget::setUpGUI()
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
        // Use right toolbutton as MIDI output channel
        int outchan = g.getMidiFilter().outChan;
        if (outchan >= 0) {
            ui->toolButton_right->setText( n2s(outchan+1));
        } else {
            ui->toolButton_right->setText( "-" );
        }
        ui->toolButton_right->setToolTip("MIDI Output Channel");

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
            ui->toolButton_right->setText( n2s(g.busIdInProject) );
            ui->toolButton_right->setToolTip("Bus: " + project->audioBus_getBus(g.busIdInProject).busName);
            ui->toolButton_right->setStyleSheet("");
        } else {
            ui->toolButton_right->setText( n2s(g.busIdInProject) + "!" );
            ui->toolButton_right->setToolTip("Bus does not exist in this project.");
            ui->toolButton_right->setStyleSheet("background-color: red;");
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
            ui->toolButton_left->setText( btnTxt );
            ui->toolButton_left->setToolTip(tooltip);
            ui->toolButton_left->setStyleSheet("");
        } else {
            ui->toolButton_left->setText( n2s(g.midiInPortIdInProject) + "!" );
            ui->toolButton_left->setToolTip("MIDI In Port does not exist in this project.");
            ui->toolButton_left->setStyleSheet("background-color: red;");
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
        ui->toolButton_right->setVisible(false);

        ui->lineEdit->setProperty("konfytError", true);
        ui->lineEdit->style()->unpolish(ui->lineEdit);
        ui->lineEdit->style()->polish(ui->lineEdit);

    }
}

// Change the background to indicate midi filter zone.
void KonfytLayerWidget::updateBackgroundFromFilter()
{
    KonfytMidiFilter filter = g.getMidiFilter();
    KonfytMidiFilterZone z = filter.zone;
    this->changeBackground(z.lowNote, z.highNote);
}

// Change the background depending on the min and max values (between 0 and 127),
// used to indicate the range/zone of a midi filter.
void KonfytLayerWidget::changeBackground(int min, int max)
{
    max++; // inclusive!
    background_rectLeft = (float)min/127.0;
    if (max >= 127) { background_rectRight = 1; }
    else { background_rectRight = (float)max/127.0; }
    this->repaint();
}

KonfytPatchLayer KonfytLayerWidget::getPatchLayerItem()
{
    return this->g;
}

QListWidgetItem* KonfytLayerWidget::getListWidgetItem()
{
    return this->listWidgetItem;
}

QString KonfytLayerWidget::getFilePath()
{
    return filepath;
}

void KonfytLayerWidget::indicateMidi()
{
    midiIndicateTimer.start(500);
    if (!midiIndicate) {
        midiIndicate = true;
        this->repaint();
    }
}

void KonfytLayerWidget::indicateSustain(bool sustain)
{
    if (sustain != midiIndicateSustain) {
        midiIndicateSustain = sustain;
        this->repaint();
    }
}

void KonfytLayerWidget::indicatePitchbend(bool pitchbend)
{
    if (pitchbend != midiIndicatePitchbend) {
        midiIndicatePitchbend = pitchbend;
        this->repaint();
    }
}

/* Set slider gain from outside the class, e.g. if gain was changed by some
 * other event like a midi trigger. */
void KonfytLayerWidget::setSliderGain(float newGain)
{
    ui->gainSlider->setValue(newGain*ui->gainSlider->maximum());
    g.setGain(newGain);
}

/* Set solo button from outside the class, e.g. if it was changed by some
 * other event like a midi trigger. */
void KonfytLayerWidget::setSoloButton(bool solo)
{
    ui->toolButton_solo->setChecked(solo);
    g.setSolo(solo);
}

/* Set mute button from outside the class, e.g. if it was changed by some
 * other event like a midi trigger. */
void KonfytLayerWidget::setMuteButton(bool mute)
{
    ui->toolButton_mute->setChecked(mute);
    g.setMute(mute);
}

void KonfytLayerWidget::on_gainSlider_sliderMoved(int position)
{
    float gain = (float)position/(float)ui->gainSlider->maximum();
    g.setGain(gain);
    emit slider_moved_signal(this, gain);
}

void KonfytLayerWidget::on_gainSlider_valueChanged(int /*value*/)
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
void KonfytLayerWidget::on_toolButton_solo_clicked()
{
    bool solo = ui->toolButton_solo->isChecked();
    g.setSolo(solo);
    emit solo_clicked_signal(this, solo);
}

/* Mute button: Clicked. */
void KonfytLayerWidget::on_toolButton_mute_clicked()
{
    bool mute = ui->toolButton_mute->isChecked();
    g.setMute(mute);
    emit mute_clicked_signal(this, mute);
}

void KonfytLayerWidget::on_toolButton_right_clicked()
{
    emit rightToolbutton_clicked_signal(this);
}

void KonfytLayerWidget::on_toolButton_sendEvents_clicked()
{
    emit sendMidiEvents_clicked_signal(this);
}

void KonfytLayerWidget::midiIndicateTimerEvent()
{
    midiIndicate = false;
    midiIndicateTimer.stop();
    this->repaint();
}
