/******************************************************************************
 *
 * Copyright 2021 Gideon van der Kolf
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

#include <math.h>

KonfytLayerWidget::KonfytLayerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::KonfytLayerWidget)
{
    ui->setupUi(this);

    background_rectLeft = 0;
    background_rectRight = 0;

    connect(&midiIndicateTimer, &QTimer::timeout,
            this, &KonfytLayerWidget::midiIndicateTimerEvent);

    connect(&audioIndicateTimer, &QTimer::timeout,
            this, &KonfytLayerWidget::audioIndicateTimerEvent);
    audioIndicateTimer.start(100);
}

KonfytLayerWidget::~KonfytLayerWidget()
{
    delete ui;
}

void KonfytLayerWidget::setProject(KonfytLayerWidget::ProjectPtr project)
{
    mProject = project;

    connect(mProject.data(), &KonfytProject::audioInPortNameChanged,
            this, &KonfytLayerWidget::onProjectAudioInPortNameChanged);
    connect(mProject.data(), &KonfytProject::midiOutPortNameChanged,
            this, &KonfytLayerWidget::onProjectMidiOutPortNameChanged);
}

void KonfytLayerWidget::paintEvent(QPaintEvent* /*e*/)
{
    static QColor colorFG = QColor(24, 87, 127, 255);
    static QColor colorBG = QColor(30, 30, 30, 255);
    static QColor colorHighlighted = QColor(255, 255, 255, 255);

    QPainter p(this);
    p.setPen(mHighlighted ? colorHighlighted : colorFG);

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

    // Audio indicator
    int alen = 30;
    if (audioLeftValue > 0) {
        int val = alen * pow(audioLeftValue, 0.333);

        QLinearGradient grad(this->width()-val, 0, this->width(), 0);
        grad.setColorAt(0, Qt::transparent);
        grad.setColorAt(1, Qt::green);
        QBrush b(grad);
        p.setBrush(b);
        p.setPen(Qt::NoPen);

        p.drawRect(this->width()-val, 0, this->width(), this->height()/2);
    }
    if (audioRightValue > 0) {
        int val = alen * pow(audioRightValue, 0.333);

        QLinearGradient grad(this->width()-val, 0, this->width(), 0);
        grad.setColorAt(0, Qt::transparent);
        grad.setColorAt(1, Qt::green);
        QBrush b(grad);
        p.setBrush(b);
        p.setPen(Qt::NoPen);

        p.drawRect(this->width()-val, this->height()/2, this->width(), this->height()-1);
    }
}

void KonfytLayerWidget::onProjectMidiOutPortNameChanged(int portId)
{
    KfPatchLayerSharedPtr layer = mPatchLayer.toStrongRef();
    if (layer.isNull()) { return; }

    if (layer->layerType() == KonfytPatchLayer::TypeMidiOut) {
        if (portId == layer->midiOutputPortData.portIdInProject) {
            refresh();
        }
    }
}

void KonfytLayerWidget::onProjectAudioInPortNameChanged(int portId)
{
    KfPatchLayerSharedPtr layer = mPatchLayer.toStrongRef();
    if (layer.isNull()) { return; }

    if (layer->layerType() == KonfytPatchLayer::TypeAudioIn) {
        if (portId == layer->audioInPortData.portIdInProject) {
            refresh();
        }
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

void KonfytLayerWidget::setHighlighted(bool highlight)
{
    mHighlighted = highlight;
}

/* Set up the widgets corresponding to the layer item. */
void KonfytLayerWidget::setUpGUI()
{
    mFilepath = "";
    bool gainSliderVisible = true;
    bool soloButtonVisible = true;
    bool muteButtonVisible = true;
    bool toolButtonRightVisible = true;
    QString text;
    QString tooltip;
    bool backgroundFilter = true;

    KfPatchLayerSharedPtr layer = mPatchLayer.toStrongRef();
    if (layer.isNull()) {
        ui->lineEdit->setText("ERROR: Null layer.");
        return;
    }

    if (layer->layerType() == KonfytPatchLayer::TypeSoundfontProgram) {

        mFilepath = layer->soundfontData.parentSoundfont;
        // Display soundfont and program name
        text = (layer->soundfontData.parentSoundfont + "/" +
                layer->soundfontData.program.name);
        tooltip = text;

    } else if (layer->layerType() == KonfytPatchLayer::TypeSfz) {

        mFilepath = layer->sfzData.path;
        // Display sfz name
        text = layer->sfzData.path;
        tooltip = layer->name() + ": " + layer->sfzData.path;

    } else if (layer->layerType() == KonfytPatchLayer::TypeMidiOut) {

        // Display port id and name
        int portId = layer->midiOutputPortData.portIdInProject;
        text = "MIDI Out " + n2s(portId);
        if (mProject) {
            // TODO: setErrorMessage should not be done here, it should be set
            //       in the engine when loading the patch
            if (mProject->midiOutPort_exists(portId)) {
                text = text + ": " + mProject->midiOutPort_getPort(portId)->portName;
            } else {
                layer->setErrorMessage("No MIDI out port " + n2s(portId) + " in project.");
            }
        }
        gainSliderVisible = false;
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
        text = "Audio In " + n2s(portId);
        if (mProject) {
            // TODO: setErrorMessage should not be done here, it should be set
            //       in the engine when loading the patch
            if (mProject->audioInPort_exists(portId)) {
                text = text + ": " + mProject->audioInPort_getPort(portId)->portName;
            } else {
                layer->setErrorMessage("No audio in port " + n2s(portId) + " in project.");
            }
        }
        backgroundFilter = false;

    } else {

        text = "ERROR: Layer not initialised.";
        gainSliderVisible = false;
        soloButtonVisible = false;
        muteButtonVisible = false;
        toolButtonRightVisible = false;
        backgroundFilter = false;

    }

    // Gain
    setSliderGain(layer->gain());
    // Solo and mute
    ui->toolButton_solo->setChecked(layer->isSolo());
    ui->toolButton_mute->setChecked(layer->isMute());

    // Update buttons
    updateRightToolButton();
    updateInputSideToolButton();

    // MIDI Events Send button
    ui->toolButton_sendEvents->setVisible( layer->midiSendList.count() > 0 );

    // If the layer has an error (marked when loading in patchEngine), indicate it.
    if (layer->hasError()) {

        text = "ERROR: " + layer->errorMessage();

        gainSliderVisible = false;
        soloButtonVisible = false;
        muteButtonVisible = false;
        toolButtonRightVisible = false;
        backgroundFilter = false;
    }

    // Apply visibility and text settings
    ui->gainSlider->setVisible(gainSliderVisible);
    ui->toolButton_solo->setVisible(soloButtonVisible);
    ui->toolButton_mute->setVisible(muteButtonVisible);
    ui->toolButton_right->setVisible(toolButtonRightVisible);
    ui->lineEdit->setText(text);
    ui->lineEdit->setToolTip(tooltip);
    // Background
    if (backgroundFilter) { updateBackgroundFromFilter(); }
    else { changeBackground(0,127); }

    ui->lineEdit->setProperty("konfytError", layer->hasError());
    ui->lineEdit->style()->unpolish(ui->lineEdit);
    ui->lineEdit->style()->polish(ui->lineEdit);
}

/* Change the background to indicate MIDI filter zone. */
void KonfytLayerWidget::updateBackgroundFromFilter()
{
    KonfytMidiFilter filter = mPatchLayer.toStrongRef()->midiFilter();
    KonfytMidiFilterZone z = filter.zone;
    this->changeBackground(z.lowNote, z.highNote);
}

void KonfytLayerWidget::updateRightToolButton()
{
    KfPatchLayerSharedPtr layer = mPatchLayer.toStrongRef();
    if (!layer) { return; }

    if ( (mProject) && (layer->layerType() != KonfytPatchLayer::TypeMidiOut) ) {
        int busId = layer->busIdInProject();
        if ( mProject->audioBus_exists(busId) ) {
            ui->toolButton_right->setText( n2s(busId) );
            ui->toolButton_right->setToolTip("Bus: " + mProject->audioBus_getBus(busId)->busName);
            ui->toolButton_right->setStyleSheet("");
        } else {
            ui->toolButton_right->setText( n2s(busId) + "!" );
            ui->toolButton_right->setToolTip("Bus does not exist in this project.");
            ui->toolButton_right->setStyleSheet("background-color: red;");
        }
    }
}

void KonfytLayerWidget::updateInputSideToolButton()
{
    KfPatchLayerSharedPtr layer = mPatchLayer.toStrongRef();
    if (!layer) { return; }

    if ( (mProject) && (layer->layerType() != KonfytPatchLayer::TypeAudioIn) ) {
        int midiPortId = layer->midiInPortIdInProject();
        if (mProject->midiInPort_exists(midiPortId)) {
            QString btnTxt = QString("%1:").arg(midiPortId);
            QString tooltip = "MIDI In Port: " + mProject->midiInPort_getPort(midiPortId)->portName;
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
}

/* Change the background depending on the min and max values (between 0 and 127),
 * used to indicate the range/zone of a MIDI filter. */
void KonfytLayerWidget::changeBackground(int min, int max)
{
    max++; // inclusive!
    background_rectLeft = (float)min/127.0;
    if (max >= 127) { background_rectRight = 1; }
    else { background_rectRight = (float)max/127.0; }
    //this->repaint(); // Done in audioIndicateTimerEvent()
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
        //this->repaint(); // Done in audioIndicateTimerEvent()
    }
}

void KonfytLayerWidget::indicateSustain(bool sustain)
{
    if (sustain != midiIndicateSustain) {
        midiIndicateSustain = sustain;
        //this->repaint(); // Done in audioIndicateTimerEvent()
    }
}

void KonfytLayerWidget::indicatePitchbend(bool pitchbend)
{
    if (pitchbend != midiIndicatePitchbend) {
        midiIndicatePitchbend = pitchbend;
        //this->repaint(); // Done in audioIndicateTimerEvent()
    }
}

void KonfytLayerWidget::indicateAudioLeft(float value)
{
    if (value < audioZeroLimit) { return; }
    if (value > 0) { value += audioIndicateStep; }
    audioLeftValue = qMax(value, audioLeftValue);
}

void KonfytLayerWidget::indicateAudioRight(float value)
{
    if (value < audioZeroLimit) { return; }
    if (value > 0) { value += audioIndicateStep; }
    audioRightValue = qMax(value, audioRightValue);
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
    //this->repaint(); // Done in audioIndicateTimerEvent()
}

void KonfytLayerWidget::audioIndicateTimerEvent()
{
    audioLeftValue = qMax((float)0.0, audioLeftValue - audioIndicateStep);
    audioRightValue = qMax((float)0.0, audioRightValue - audioIndicateStep);

    this->repaint();
}
