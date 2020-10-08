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

#ifndef KONFYT_PATCH_LAYER_H
#define KONFYT_PATCH_LAYER_H

#include <QObject>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSlider>

#include "konfytStructs.h"
#include "konfytMidiFilter.h"



enum KonfytLayerType {
    KonfytLayerType_Uninitialized    = -1,
    KonfytLayerType_SoundfontProgram = 0,
    KonfytLayerType_Sfz              = 1,
    KonfytLayerType_MidiOut          = 2,
    KonfytLayerType_AudioIn          = 3
};

struct KonfytPortSpec
{
    QString name;
    QString midi_in_port;
    QString audio_out_port_left;
    QString audio_out_port_right;

};

// ----------------------------------------------------
// Structure for soundfont program layer
// ----------------------------------------------------
struct LayerSoundfontStruct {
    KonfytSoundfontProgram program;
    float gain = 1.0;
    KonfytMidiFilter filter;
    int indexInEngine = -1;
    bool solo = false;
    bool mute = false;
    int idInJackEngine = 0;
};

// ----------------------------------------------------
// Structure for SFZ layer
// ----------------------------------------------------
struct LayerSfzStruct {
    QString name;
    QString path;
    QString midi_in_port;
    QString audio_out_port_left;
    QString audio_out_port_right;
    int portsIdInJackEngine = -1;

    KonfytMidiFilter midiFilter;

    int indexInEngine = -1;

    float gain = 1.0;
    bool solo = false;
    bool mute = false;
};


// ----------------------------------------------------
// Structure for midi output port layer
// ----------------------------------------------------
struct LayerMidiOutStruct {
    int portIdInProject;
    KonfytMidiFilter filter;
    bool solo = false;
    bool mute = false;
    int jackRouteId = -1;
};

// ----------------------------------------------------
// Structure for audio input port layer
// ----------------------------------------------------
struct LayerAudioInStruct {
    QString name;
    int portIdInProject;   // Index of audio input port in project
    float gain = 1.0;
    bool solo = false;
    bool mute = false;
    int jackRouteIdLeft = -1;
    int jackRouteIdRight = -1;
};


// ----------------------------------------------------
// Class
// ----------------------------------------------------
class KonfytPatchLayer
{
public:
    int idInPatch = -1; // ID used in patch to uniquely identify layeritems within a patch.

    KonfytLayerType getLayerType() const;

    // Use to initialize layer
    void initLayer(int id, LayerSoundfontStruct newLayerData);
    void initLayer(int id, LayerSfzStruct newLayerData);
    void initLayer(int id, LayerMidiOutStruct newLayerData);
    void initLayer(int id, LayerAudioInStruct newLayerData);

    QString getName();
    float getGain() const;
    void setGain(float newGain);
    void setSolo(bool newSolo);
    void setMute(bool newMute);
    bool isSolo() const;
    bool isMute() const;

    int busIdInProject = 0;
    int midiInPortIdInProject = 0;

    KonfytMidiFilter getMidiFilter();
    void setMidiFilter(KonfytMidiFilter newFilter);

    void setErrorMessage(QString msg);
    bool hasError();
    QString getErrorMessage();

    // Depending on the layer type, one of the following is used:
    // TODO: MERGE BELOW INTO LAYER
    LayerSoundfontStruct    soundfontData;
    LayerSfzStruct          sfzData;
    LayerMidiOutStruct      midiOutputPortData;
    LayerAudioInStruct      audioInPortData;

    QList<MidiSendItem> midiSendList;
    QList<KonfytMidiEvent> getMidiSendListEvents();

private:
    KonfytLayerType layerType = KonfytLayerType_Uninitialized;

    QString errorMessage;
    
};

#endif // KONFYT_PATCH_LAYER_H
