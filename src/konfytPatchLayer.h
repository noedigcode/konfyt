/******************************************************************************
 *
 * Copyright 2023 Gideon van der Kolf
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
#include "konfytMidi.h"
#include "konfytMidiFilter.h"
#include "konfytStructs.h"

// ============================================================================

struct LayerSoundfontData
{
    QString parentSoundfont;
    KonfytSoundPreset program;
    KfFluidSynth* synthInEngine = nullptr;
    KfJackPluginPorts* portsInJackEngine = nullptr;
};

// ============================================================================

struct LayerSfzData
{
    QString path;
    QString midiInPort;
    QString audioOutPortLeft;
    QString audioOutPortRight;
    KfJackPluginPorts* portsInJackEngine = nullptr;
    int indexInEngine = -1;
};

// ============================================================================

struct LayerMidiOutData
{
    int portIdInProject;
    KfJackMidiRoute* jackRoute = nullptr;
};

// ============================================================================

struct LayerAudioInData
{
    int portIdInProject;
    KfJackAudioRoute* jackRouteLeft = nullptr;
    KfJackAudioRoute* jackRouteRight = nullptr;
};

// ============================================================================

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

    KonfytPatchLayer();

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
    void setGainByMidi(int value);
    void setGainMidiPickupRange(int range);
    int gainMidiPickupRange();
    void setSolo(bool isSolo);
    void setMute(bool isMute);
    bool isSolo() const;
    bool isMute() const;
    int busIdInProject() const;
    void setBusIdInProject(int bus);
    int midiInPortIdInProject() const;
    void setMidiInPortIdInProject(int port);

    QString script() const;
    void setScript(QString script);
    bool isScriptEnabled() const;
    void setScriptEnabled(bool enable);
    bool isPassMidiThrough() const;
    void setPassMidiThrough(bool enable);

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

    QString uri;

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

    QString mScript;
    bool mScriptEnabled = false;
    // True to pass original events through to output in addition to script
    // processing.
    bool mPassMidiThrough = true;

    MidiValueController gainMidiCtrl;
};

#endif // KONFYT_PATCH_LAYER_H
