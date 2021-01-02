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

#ifndef KONFYT_PATCH_LAYER_H
#define KONFYT_PATCH_LAYER_H

#include "konfytFluidsynthEngine.h"
#include "konfytJackStructs.h"
#include "konfytMidiFilter.h"
#include "konfytStructs.h"

// ----------------------------------------------------
// Structure for soundfont program layer
// ----------------------------------------------------
struct LayerSoundfontData
{
    QString parentSoundfont;
    KonfytSoundPreset program;
    KfFluidSynth* synthInEngine = nullptr;
    KfJackPluginPorts* portsInJackEngine = nullptr;
};

// ----------------------------------------------------
// Structure for SFZ layer
// ----------------------------------------------------
struct LayerSfzData
{
    QString path;
    QString midiInPort;
    QString audioOutPortLeft;
    QString audioOutPortRight;
    KfJackPluginPorts* portsInJackEngine = nullptr;
    int indexInEngine = -1;
};

// ----------------------------------------------------
// Structure for midi output port layer
// ----------------------------------------------------
struct LayerMidiOutData
{
    int portIdInProject;
    KfJackMidiRoute* jackRoute = nullptr;
};

// ----------------------------------------------------
// Structure for audio input port layer
// ----------------------------------------------------
struct LayerAudioInData
{
    int portIdInProject;
    KfJackAudioRoute* jackRouteLeft = nullptr;
    KfJackAudioRoute* jackRouteRight = nullptr;
};


// ----------------------------------------------------
// Class
// ----------------------------------------------------
class KonfytPatchLayer
{
public:
    enum LayerType {
        TypeUninitialized    = -1,
        TypeSoundfontProgram = 0,
        TypeSfz              = 1,
        TypeMidiOut          = 2,
        TypeAudioIn          = 3
    };

    // Use to initialize layer
    void initLayer(LayerSoundfontData newLayerData);
    void initLayer(LayerSfzData newLayerData);
    void initLayer(LayerMidiOutData newLayerData);
    void initLayer(LayerAudioInData newLayerData);

    LayerType layerType() const;
    bool hasMidiInput() const;
    QString name() const;
    void setName(QString name);
    float gain() const;
    void setGain(float gain);
    void setSolo(bool isSolo);
    void setMute(bool isMute);
    bool isSolo() const;
    bool isMute() const;
    int busIdInProject() const;
    void setBusIdInProject(int bus);
    int midiInPortIdInProject() const;
    void setMidiInPortIdInProject(int port);

    KonfytMidiFilter midiFilter() const;
    void setMidiFilter(KonfytMidiFilter midiFilter);

    void setErrorMessage(QString msg);
    bool hasError() const;
    QString errorMessage() const;

    // Depending on the layer type, one of the following is used:
    // TODO: MERGE BELOW INTO LAYER
    LayerSoundfontData    soundfontData;
    LayerSfzData          sfzData;
    LayerMidiOutData      midiOutputPortData;
    LayerAudioInData      audioInPortData;

    QList<MidiSendItem> midiSendList;
    QList<KonfytMidiEvent> getMidiSendListEvents();

private:
    LayerType mLayerType = TypeUninitialized;
    QString mErrorMessage;
    float mGain = 1.0;
    bool mSolo = false;
    bool mMute = false;
    KonfytMidiFilter mMidiFilter;
    QString mName = "UNINITIALIZED LAYER";
    int mBusIdInProject = 0;
    int mMidiInPortIdInProject = 0;
};

#endif // KONFYT_PATCH_LAYER_H
