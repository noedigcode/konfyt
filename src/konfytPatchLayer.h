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
    KonfytLayerType_CarlaPlugin      = 1, // for sfz, lv2, internal, etc.
    KonfytLayerType_MidiOut          = 2,
    KonfytLayerType_AudioIn          = 3
};

enum KonfytCarlaPluginType {
    KonfytCarlaPluginType_SFZ = 1,
    KonfytCarlaPluginType_LV2 = 2,
    KonfytCarlaPluginType_Internal = 3
};

// ----------------------------------------------------
// Structure for soundfont program layer
// ----------------------------------------------------
struct LayerSoundfontStruct {
    konfytSoundfontProgram program;
    float gain;
    konfytMidiFilter filter;
    int indexInEngine;
    bool solo;
    bool mute;

    LayerSoundfontStruct() : gain(1),
                             indexInEngine(-1),
                             solo(true),
                             mute(true) {}

};

// ----------------------------------------------------
// Structure for carla plugin
// ----------------------------------------------------
struct LayerCarlaPluginStruct {
    QString name;
    QString path;
    KonfytCarlaPluginType pluginType;
    QString midi_in_port;
    QString audio_out_port_left;
    QString audio_out_port_right;

    konfytMidiFilter midiFilter;

    int indexInEngine;

    float gain;
    bool solo;
    bool mute;

    LayerCarlaPluginStruct() : indexInEngine(-1),
                               gain(1),
                               solo(false),
                               mute(false) {}

};


// ----------------------------------------------------
// Structure for midi output port layer
// ----------------------------------------------------
struct LayerMidiOutStruct {
    int portIdInProject;
    konfytMidiFilter filter;
    bool solo;
    bool mute;

    LayerMidiOutStruct() : solo(false),
                           mute(false) {}

};

// ----------------------------------------------------
// Structure for audio input port layer
// ----------------------------------------------------
struct LayerAudioInStruct {
    QString name;
    int portIdInProject;   // Index of audio input port in project
    float gain;
    bool solo;
    bool mute;

    LayerAudioInStruct() : gain(1),
                           solo(false),
                           mute(false) {}

};


// ----------------------------------------------------
// Class
// ----------------------------------------------------
class KonfytPatchLayer
{
public:
    explicit KonfytPatchLayer();

    int ID_in_patch; // ID used in patch to uniquely identify layeritems within a patch.

    KonfytLayerType getLayerType() const;

    // Use to initialize layer
    void initLayer(int id, LayerSoundfontStruct newLayerData);
    void initLayer(int id, LayerCarlaPluginStruct newLayerData);
    void initLayer(int id, LayerMidiOutStruct newLayerData);
    void initLayer(int id, LayerAudioInStruct newLayerData);

    QString getName();
    float getGain() const;
    void setGain(float newGain);
    void setSolo(bool newSolo);
    void setMute(bool newMute);
    bool isSolo() const;
    bool isMute() const;

    int busIdInProject;
    int midiInPortIdInProject;

    konfytMidiFilter getMidiFilter();
    void setMidiFilter(konfytMidiFilter newFilter);

    void setErrorMessage(QString msg);
    bool hasError();
    QString getErrorMessage();

    // Depending on the layer type, one of the following is used:
    LayerSoundfontStruct    sfData;
    LayerCarlaPluginStruct  carlaPluginData;
    LayerMidiOutStruct      midiOutputPortData;
    LayerAudioInStruct      audioInPortData;


private:
    KonfytLayerType layerType;

    QString errorMessage;
    
};

#endif // KONFYT_PATCH_LAYER_H
