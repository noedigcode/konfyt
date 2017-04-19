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



enum konfytLayerType {
    KonfytLayerType_Uninitialized    = -1,
    KonfytLayerType_SoundfontProgram = 0,
    KonfytLayerType_CarlaPlugin      = 1, // for sfz, lv2, internal, etc.
    KonfytLayerType_MidiOut          = 2,
    KonfytLayerType_AudioIn          = 3
};

enum konfytCarlaPluginType {
    KonfytCarlaPluginType_SFZ = 1,
    KonfytCarlaPluginType_LV2 = 2,
    KonfytCarlaPluginType_Internal = 3
};

// ----------------------------------------------------
// Structure for soundfont program layer
// ----------------------------------------------------
struct layerSoundfontStruct {
    konfytSoundfontProgram program;
    float gain;
    konfytMidiFilter filter;
    int indexInEngine;
    bool solo;
    bool mute;

    layerSoundfontStruct() : gain(1),
                             indexInEngine(-1),
                             solo(true),
                             mute(true) {}

};

// ----------------------------------------------------
// Structure for carla plugin
// ----------------------------------------------------
struct layerCarlaPluginStruct {
    QString name;
    QString path;
    konfytCarlaPluginType pluginType;
    QString midi_in_port;
    QString audio_out_port_left;
    QString audio_out_port_right;

    konfytMidiFilter midiFilter;

    int indexInEngine;

    float gain;
    bool solo;
    bool mute;

    layerCarlaPluginStruct() : indexInEngine(-1),
                               gain(1),
                               solo(false),
                               mute(false) {}

};


// ----------------------------------------------------
// Structure for midi output port layer
// ----------------------------------------------------
struct layerMidiOutStruct {
    int portIdInProject;
    konfytMidiFilter filter;
    bool solo;
    bool mute;

    layerMidiOutStruct() : solo(false),
                           mute(false) {}

};

// ----------------------------------------------------
// Structure for audio input port layer
// ----------------------------------------------------
struct layerAudioInStruct {
    QString name;
    int portIdInProject;   // Index of audio input port in project
    float gain;
    bool solo;
    bool mute;

    layerAudioInStruct() : gain(1),
                           solo(false),
                           mute(false) {}

};


// ----------------------------------------------------
// Class
// ----------------------------------------------------
class konfytPatchLayer
{
public:
    explicit konfytPatchLayer();

    int ID_in_patch; // ID used in patch to uniquely identify layeritems within a patch.

    konfytLayerType getLayerType();

    // Use to initialize layer
    void initLayer(int id, layerSoundfontStruct newLayerData);
    void initLayer(int id, layerCarlaPluginStruct newLayerData);
    void initLayer(int id, layerMidiOutStruct newLayerData);
    void initLayer(int id, layerAudioInStruct newLayerData);

    float getGain();
    void setGain(float newGain);
    void setSolo(bool newSolo);
    void setMute(bool newMute);
    bool isSolo();
    bool isMute();

    int busIdInProject;

    konfytMidiFilter getMidiFilter();
    void setMidiFilter(konfytMidiFilter newFilter);

    void setErrorMessage(QString msg);
    bool hasError();
    QString getErrorMessage();

    // Depending on the layer type, one of the following is used:
    layerSoundfontStruct    sfData;
    layerCarlaPluginStruct  carlaPluginData;
    layerMidiOutStruct      midiOutputPortData;
    layerAudioInStruct      audioInPortData;


private:
    konfytLayerType layerType;

    QString errorMessage;
    
};

#endif // KONFYT_PATCH_LAYER_H
