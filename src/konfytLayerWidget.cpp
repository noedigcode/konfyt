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

PatchLayerWidget::PatchLayerWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PatchLayerWidget)
{
    ui->setupUi(this);

    background_rectLeft = 0;
    background_rectRight = 0;

    connect(&midiIndicateTimer, &QTimer::timeout,
            this, &PatchLayerWidget::midiIndicateTimerEvent);

    connect(&audioIndicateTimer, &QTimer::timeout,
            this, &PatchLayerWidget::audioIndicateTimerEvent);
    audioIndicateTimer.start(100);
}

PatchLayerWidget::~PatchLayerWidget()
{
    delete ui;
}

void PatchLayerWidget::setProject(PatchLayerWidget::ProjectPtr project)
{
    mProject = project;

    connect(mProject.data(), &Project::audioInPortNameChanged,
            this, &PatchLayerWidget::onProjectAudioInPortNameChanged);
    connect(mProject.data(), &Project::midiOutPortNameChanged,
            this, &PatchLayerWidget::onProjectMidiOutPortNameChanged);
}

void PatchLayerWidget::paintEvent(QPaintEvent* /*e*/)
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

void PatchLayerWidget::onProjectMidiOutPortNameChanged(int portId)
{
    if (!mPatchLayer) { return; }

    if (mPatchLayer->layerType() == PatchLayer::TypeMidiOut) {
        if (portId == mPatchLayer->midiOutputPortData.portIdInProject) {
            refresh();
        }
    }
}

void PatchLayerWidget::onProjectAudioInPortNameChanged(int portId)
{
    if (!mPatchLayer) { return; }

    if (mPatchLayer->layerType() == PatchLayer::TypeAudioIn) {
        if (portId == mPatchLayer->audioInPortData.portIdInProject) {
            refresh();
        }
    }
}

void PatchLayerWidget::on_toolButton_left_clicked()
{
    emit leftToolbutton_clicked_signal(this);
}

/* This function has to be called before the object can be used. */
void PatchLayerWidget::initLayer(PatchLayerPtr patchLayer, QListWidgetItem *listItem)
{
    mPatchLayer = patchLayer;
    mListWidgetItem = listItem;

    // Set up GUI
    setUpGUI();
}

void PatchLayerWidget::refresh()
{
    setUpGUI();
}

void PatchLayerWidget::setHighlighted(bool highlight)
{
    mHighlighted = highlight;
}

/* Set up the widgets corresponding to the layer item. */
void PatchLayerWidget::setUpGUI()
{
    mFilepath = "";
    bool gainSliderVisible = true;
    bool soloButtonVisible = true;
    bool muteButtonVisible = true;
    bool toolButtonRightVisible = true;
    QString text;
    QString tooltip;
    bool backgroundFilter = true;

    if (!mPatchLayer) {
        ui->lineEdit->setText("ERROR: Null layer.");
        return;
    }

    if (mPatchLayer->layerType() == PatchLayer::TypeSoundfontProgram) {

        mFilepath = mPatchLayer->soundfontData.parentSoundfont;
        // Display soundfont and program name
        text = (mPatchLayer->soundfontData.parentSoundfont + "/" +
                mPatchLayer->soundfontData.program.name);
        tooltip = text;

    } else if (mPatchLayer->layerType() == PatchLayer::TypeSfz) {

        mFilepath = mPatchLayer->sfzData.path;
        // Display sfz name
        text = mPatchLayer->sfzData.path;
        tooltip = mPatchLayer->name() + ": " + mPatchLayer->sfzData.path;

    } else if (mPatchLayer->layerType() == PatchLayer::TypeMidiOut) {

        // Display port id and name
        int portId = mPatchLayer->midiOutputPortData.portIdInProject;
        text = "MIDI Out " + n2s(portId);
        if (mProject) {
            // TODO: setErrorMessage should not be done here, it should be set
            //       in the engine when loading the patch
            if (mProject->midiOutPort_exists(portId)) {
                text = text + ": " + mProject->midiOutPort_getPort(portId)->portName;
            } else {
                mPatchLayer->setErrorMessage("No MIDI out port " + n2s(portId) + " in project.");
            }
        }
        gainSliderVisible = false;
        // Use right toolbutton as MIDI output channel
        int outchan = mPatchLayer->midiFilter().outChan;
        if (outchan >= 0) {
            ui->toolButton_right->setText( n2s(outchan+1));
        } else {
            ui->toolButton_right->setText( "-" );
        }
        ui->toolButton_right->setToolTip("MIDI Output Channel");

    } else if (mPatchLayer->layerType() == PatchLayer::TypeAudioIn ) {

        // Display port id and name
        int portId = mPatchLayer->audioInPortData.portIdInProject;
        text = "Audio In " + n2s(portId);
        if (mProject) {
            // TODO: setErrorMessage should not be done here, it should be set
            //       in the engine when loading the patch
            if (mProject->audioInPort_exists(portId)) {
                text = text + ": " + mProject->audioInPort_getPort(portId)->name;
            } else {
                mPatchLayer->setErrorMessage("No audio in port " + n2s(portId) + " in project.");
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
    setSliderGain(mPatchLayer->gain());
    // Solo and mute
    ui->toolButton_solo->setChecked(mPatchLayer->isSolo());
    ui->toolButton_mute->setChecked(mPatchLayer->isMute());

    // Update buttons
    updateRightToolButton();
    updateInputSideToolButton();

    // MIDI Events Send button
    ui->toolButton_sendEvents->setVisible( mPatchLayer->midiSendList.count() > 0 );

    // If the layer has an error (marked when loading in patchEngine), indicate it.
    if (mPatchLayer->hasError()) {

        text = "ERROR: " + mPatchLayer->errorMessage();

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

    ui->lineEdit->setProperty("konfytError", mPatchLayer->hasError());
    ui->lineEdit->style()->unpolish(ui->lineEdit);
    ui->lineEdit->style()->polish(ui->lineEdit);
}

/* Change the background to indicate MIDI filter zone. */
void PatchLayerWidget::updateBackgroundFromFilter()
{
    if (!mPatchLayer) { return; }
    MidiFilter filter = mPatchLayer->midiFilter();
    MidiFilterZone z = filter.zone;
    this->changeBackground(z.lowNote, z.highNote);
}

void PatchLayerWidget::updateRightToolButton()
{
    if (!mPatchLayer) { return; }

    if ( (mProject) && (mPatchLayer->layerType() != PatchLayer::TypeMidiOut) ) {
        int busId = mPatchLayer->busIdInProject();
        if ( mProject->audioBus_exists(busId) ) {
            ui->toolButton_right->setText( n2s(busId) );
            ui->toolButton_right->setToolTip("Bus: " + mProject->audioBus_getBus(busId)->name);
            ui->toolButton_right->setStyleSheet("");
        } else {
            ui->toolButton_right->setText( n2s(busId) + "!" );
            ui->toolButton_right->setToolTip("Bus does not exist in this project.");
            ui->toolButton_right->setStyleSheet("background-color: red;");
        }
    }
}

void PatchLayerWidget::updateInputSideToolButton()
{
    if (!mPatchLayer) { return; }

    if ( (mProject) && (mPatchLayer->layerType() != PatchLayer::TypeAudioIn) ) {
        int midiPortId = mPatchLayer->midiInPortIdInProject();
        if (mProject->midiInPort_exists(midiPortId)) {
            QString btnTxt = QString("%1:").arg(midiPortId);
            QString tooltip = "MIDI In Port: " + mProject->midiInPort_getPort(midiPortId)->name;
            tooltip.append("\nMIDI Channel: ");
            int inChan = mPatchLayer->midiFilter().inChan;
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
void PatchLayerWidget::changeBackground(int min, int max)
{
    max++; // inclusive!
    background_rectLeft = (float)min/127.0;
    if (max >= 127) { background_rectRight = 1; }
    else { background_rectRight = (float)max/127.0; }
    //this->repaint(); // Done in audioIndicateTimerEvent()
}

PatchLayerPtr PatchLayerWidget::getPatchLayer()
{
    return mPatchLayer;
}

QListWidgetItem* PatchLayerWidget::getListWidgetItem()
{
    return mListWidgetItem;
}

QString PatchLayerWidget::getFilePath()
{
    return mFilepath;
}

void PatchLayerWidget::indicateMidi()
{
    midiIndicateTimer.start(500);
    if (!midiIndicate) {
        midiIndicate = true;
        //this->repaint(); // Done in audioIndicateTimerEvent()
    }
}

void PatchLayerWidget::indicateSustain(bool sustain)
{
    if (sustain != midiIndicateSustain) {
        midiIndicateSustain = sustain;
        //this->repaint(); // Done in audioIndicateTimerEvent()
    }
}

void PatchLayerWidget::indicatePitchbend(bool pitchbend)
{
    if (pitchbend != midiIndicatePitchbend) {
        midiIndicatePitchbend = pitchbend;
        //this->repaint(); // Done in audioIndicateTimerEvent()
    }
}

void PatchLayerWidget::indicateAudioLeft(float value)
{
    if (value < audioZeroLimit) { return; }
    if (value > 0) { value += audioIndicateStep; }
    audioLeftValue = qMax(value, audioLeftValue);
}

void PatchLayerWidget::indicateAudioRight(float value)
{
    if (value < audioZeroLimit) { return; }
    if (value > 0) { value += audioIndicateStep; }
    audioRightValue = qMax(value, audioRightValue);
}

/* Set slider gain from outside the class, e.g. if gain was changed by some
 * other event like a MIDI trigger. */
void PatchLayerWidget::setSliderGain(float newGain)
{
    ui->gainSlider->setValue(newGain*ui->gainSlider->maximum());
}

/* Set solo button from outside the class, e.g. if it was changed by some
 * other event like a midi trigger. */
void PatchLayerWidget::setSoloButton(bool solo)
{
    ui->toolButton_solo->setChecked(solo);
}

/* Set mute button from outside the class, e.g. if it was changed by some
 * other event like a midi trigger. */
void PatchLayerWidget::setMuteButton(bool mute)
{
    ui->toolButton_mute->setChecked(mute);
}

void PatchLayerWidget::on_gainSlider_sliderMoved(int position)
{
    float gain = (float)position/(float)ui->gainSlider->maximum();
    emit slider_moved_signal(this, gain);
}

void PatchLayerWidget::on_gainSlider_valueChanged(int /*value*/)
{
    // Update slider tooltip
    float tooltip_gain = (float)ui->gainSlider->value()/(float)ui->gainSlider->maximum();
    int tooltip_midi = tooltip_gain*127.0;
    ui->gainSlider->setToolTip("Gain: " +
                               QString::number( tooltip_gain, 'g', 2 ) +
                               ", MIDI Value: " +
                               n2s( tooltip_midi ) );
}

void PatchLayerWidget::on_toolButton_solo_clicked()
{
    bool solo = ui->toolButton_solo->isChecked();
    emit solo_clicked_signal(this, solo);
}

void PatchLayerWidget::on_toolButton_mute_clicked()
{
    bool mute = ui->toolButton_mute->isChecked();
    emit mute_clicked_signal(this, mute);
}

void PatchLayerWidget::on_toolButton_right_clicked()
{
    emit rightToolbutton_clicked_signal(this);
}

void PatchLayerWidget::on_toolButton_sendEvents_clicked()
{
    emit sendMidiEvents_clicked_signal(this);
}

void PatchLayerWidget::midiIndicateTimerEvent()
{
    midiIndicate = false;
    midiIndicateTimer.stop();
    //this->repaint(); // Done in audioIndicateTimerEvent()
}

void PatchLayerWidget::audioIndicateTimerEvent()
{
    audioLeftValue = qMax((float)0.0, audioLeftValue - audioIndicateStep);
    audioRightValue = qMax((float)0.0, audioRightValue - audioIndicateStep);

    this->repaint();
}
