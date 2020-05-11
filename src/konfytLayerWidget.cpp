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

/* This function has to be called before the object can be used. */
void KonfytLayerWidget::initLayer(KfPatchLayerWeakPtr patchLayer, QListWidgetItem *listItem)
{
    mPatchLayer = patchLayer;
    mListWidgetItem = listItem;

    // Set up GUI
    setUpGUI();
}

void KonfytLayerWidget::refresh()
{
    setUpGUI();
}

/* Set up the widgets corresponding to the layer item. */
void KonfytLayerWidget::setUpGUI()
{
    mFilepath = "";

    KfPatchLayerSharedPtr layer = mPatchLayer.toStrongRef();
    if (layer.isNull()) {
        ui->lineEdit->setText("ERROR: Null layer.");
        return;
    }

    if (layer->layerType() == KonfytPatchLayer::TypeSoundfontProgram) {

        mFilepath = layer->soundfontData.program.parent_soundfont;
        // Display soundfont and program name
        ui->lineEdit->setText(layer->soundfontData.program.parent_soundfont + "/" +
                              layer->soundfontData.program.name);
        ui->lineEdit->setToolTip(ui->lineEdit->text());
        // Indicate note range of the midi filter
        this->updateBackgroundFromFilter();
        // Volume
        ui->gainSlider->setVisible(true);


    } else if (layer->layerType() == KonfytPatchLayer::TypeSfz) {

        mFilepath = layer->sfzData.path;

        // Display sfz name
        ui->lineEdit->setText(layer->sfzData.path);
        ui->lineEdit->setToolTip( layer->name() + ": " + layer->sfzData.path );

        // Indicate note range of the midi filter
        this->updateBackgroundFromFilter();
        // Volume
        ui->gainSlider->setVisible(true);

    } else if (layer->layerType() == KonfytPatchLayer::TypeMidiOut) {

        // Display port id and name
        int portId = layer->midiOutputPortData.portIdInProject;
        QString text = "MIDI Out " + n2s(portId);
        if (project != NULL) {
            // TODO: setErrorMessage should not be done here, it should be set
            //       in the engine when loading the patch
            if (project->midiOutPort_exists(portId)) {
                text = text + ": " + project->midiOutPort_getPort(portId).portName;
            } else {
                layer->setErrorMessage("No MIDI out port " + n2s(portId) + " in project.");
            }
        }
        ui->lineEdit->setText( text );
        // Indicate note range of the midi filter
        this->updateBackgroundFromFilter();
        // Volume
        ui->gainSlider->setVisible(false);
        // Use right toolbutton as MIDI output channel
        int outchan = layer->midiFilter().outChan;
        if (outchan >= 0) {
            ui->toolButton_right->setText( n2s(outchan+1));
        } else {
            ui->toolButton_right->setText( "-" );
        }
        ui->toolButton_right->setToolTip("MIDI Output Channel");

    } else if (layer->layerType() == KonfytPatchLayer::TypeAudioIn ) {

        // Display port id and name
        int portId = layer->audioInPortData.portIdInProject;
        QString text = "Audio In " + n2s(portId);
        if (project != NULL) {
            // TODO: setErrorMessage should not be done here, it should be set
            //       in the engine when loading the patch
            if (project->audioInPort_exists(portId)) {
                text = text + ": " + project->audioInPort_getPort(portId).portName;
            } else {
                layer->setErrorMessage("No audio in port " + n2s(portId) + " in project.");
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
    this->setSliderGain(layer->gain());
    // Solo and mute
    ui->toolButton_solo->setChecked(layer->isSolo());
    ui->toolButton_mute->setChecked(layer->isMute());

    // Bus button
    if ( (project != NULL) && (layer->layerType() != KonfytPatchLayer::TypeMidiOut) ) {
        int busId = layer->busIdInProject();
        if ( project->audioBus_exists(busId) ) {
            ui->toolButton_right->setText( n2s(busId) );
            ui->toolButton_right->setToolTip("Bus: " + project->audioBus_getBus(busId).busName);
            ui->toolButton_right->setStyleSheet("");
        } else {
            ui->toolButton_right->setText( n2s(busId) + "!" );
            ui->toolButton_right->setToolTip("Bus does not exist in this project.");
            ui->toolButton_right->setStyleSheet("background-color: red;");
        }
    }

    // Input-side tool button
    if ( (project != NULL) && (layer->layerType() != KonfytPatchLayer::TypeAudioIn) ) {
        int midiPortId = layer->midiInPortIdInProject();
        if (project->midiInPort_exists(midiPortId)) {
            QString btnTxt = QString("%1:").arg(midiPortId);
            QString tooltip = "MIDI In Port: " + project->midiInPort_getPort(midiPortId).portName;
            tooltip.append("\nMIDI Channel: ");
            int inChan = layer->midiFilter().inChan;
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
            ui->toolButton_left->setText( n2s(midiPortId) + "!" );
            ui->toolButton_left->setToolTip("MIDI In Port does not exist in this project.");
            ui->toolButton_left->setStyleSheet("background-color: red;");
        }
    }

    // MIDI Events Send button
    ui->toolButton_sendEvents->setVisible( layer->midiSendList.count() > 0 );

    // If the layer has an error (marked when loading in patchEngine), indicate it.
    if (layer->hasError()) {

        ui->lineEdit->setText("ERROR: " + layer->errorMessage());

        ui->gainSlider->setVisible(false);
        ui->toolButton_solo->setVisible(false);
        ui->toolButton_mute->setVisible(false);
        ui->toolButton_right->setVisible(false);

        ui->lineEdit->setProperty("konfytError", true);
        ui->lineEdit->style()->unpolish(ui->lineEdit);
        ui->lineEdit->style()->polish(ui->lineEdit);

    }
}

/* Change the background to indicate MIDI filter zone. */
void KonfytLayerWidget::updateBackgroundFromFilter()
{
    KonfytMidiFilter filter = mPatchLayer.toStrongRef()->midiFilter();
    KonfytMidiFilterZone z = filter.zone;
    this->changeBackground(z.lowNote, z.highNote);
}

/* Change the background depending on the min and max values (between 0 and 127),
 * used to indicate the range/zone of a MIDI filter. */
void KonfytLayerWidget::changeBackground(int min, int max)
{
    max++; // inclusive!
    background_rectLeft = (float)min/127.0;
    if (max >= 127) { background_rectRight = 1; }
    else { background_rectRight = (float)max/127.0; }
    this->repaint();
}

KfPatchLayerWeakPtr KonfytLayerWidget::getPatchLayer()
{
    return mPatchLayer;
}

QListWidgetItem* KonfytLayerWidget::getListWidgetItem()
{
    return mListWidgetItem;
}

QString KonfytLayerWidget::getFilePath()
{
    return mFilepath;
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
 * other event like a MIDI trigger. */
void KonfytLayerWidget::setSliderGain(float newGain)
{
    ui->gainSlider->setValue(newGain*ui->gainSlider->maximum());
}

/* Set solo button from outside the class, e.g. if it was changed by some
 * other event like a midi trigger. */
void KonfytLayerWidget::setSoloButton(bool solo)
{
    ui->toolButton_solo->setChecked(solo);
}

/* Set mute button from outside the class, e.g. if it was changed by some
 * other event like a midi trigger. */
void KonfytLayerWidget::setMuteButton(bool mute)
{
    ui->toolButton_mute->setChecked(mute);
}

void KonfytLayerWidget::on_gainSlider_sliderMoved(int position)
{
    float gain = (float)position/(float)ui->gainSlider->maximum();
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
    emit solo_clicked_signal(this, solo);
}

/* Mute button: Clicked. */
void KonfytLayerWidget::on_toolButton_mute_clicked()
{
    bool mute = ui->toolButton_mute->isChecked();
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
