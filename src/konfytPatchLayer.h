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

#define XML_PATCH_SFLAYER "sfLayer"
#define XML_PATCH_SFLAYER_FILENAME "soundfont_filename"
#define XML_PATCH_SFLAYER_BANK "bank"
#define XML_PATCH_SFLAYER_PROGRAM "program"
#define XML_PATCH_SFLAYER_NAME "name"
#define XML_PATCH_SFLAYER_GAIN "gain"
#define XML_PATCH_SFLAYER_BUS "bus"
#define XML_PATCH_SFLAYER_SOLO "solo"
#define XML_PATCH_SFLAYER_MUTE "mute"
#define XML_PATCH_SFLAYER_MIDI_IN "midiIn"

#define XML_PATCH_SFZLAYER "sfzLayer"
#define XML_PATCH_SFZLAYER_NAME "name"
#define XML_PATCH_SFZLAYER_PATH "path"
#define XML_PATCH_SFZLAYER_GAIN "gain"
#define XML_PATCH_SFZLAYER_BUS "bus"
#define XML_PATCH_SFZLAYER_SOLO "solo"
#define XML_PATCH_SFZLAYER_MUTE "mute"
#define XML_PATCH_SFZLAYER_MIDI_IN "midiIn"

#define XML_PATCH_MIDIOUT "midiOutputPortLayer"
#define XML_PATCH_MIDIOUT_PORT "port"
#define XML_PATCH_MIDIOUT_SOLO "solo"
#define XML_PATCH_MIDIOUT_MUTE "mute"
#define XML_PATCH_MIDIOUT_MIDI_IN "midiIn"

#define XML_PATCH_AUDIOIN "audioInPortLayer"
#define XML_PATCH_AUDIOIN_NAME "name"
#define XML_PATCH_AUDIOIN_PORT "port"
#define XML_PATCH_AUDIOIN_GAIN "gain"
#define XML_PATCH_AUDIOIN_BUS "bus"
#define XML_PATCH_AUDIOIN_SOLO "solo"
#define XML_PATCH_AUDIOIN_MUTE "mute"

#define XML_PATCH_LAYER_SCRIPT "script"
#define XML_PATCH_LAYER_SCRIPT_CONTENT "content"
#define XML_PATCH_LAYER_SCRIPT_ENABLED "enabled"
#define XML_PATCH_LAYER_SCRIPT_PASS_MIDI_THROUGH "passMidiThrough"

#define XML_PATCH_LAYER_RESET_OPTION "resetOption"

#define XML_PATCH_MIDISENDLIST "midiSendList"

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
    void addGainRelativeMidiValue(int value);
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

    void setResetOption(KonfytReset option);
    KonfytReset getResetOption();
    void createResetSnapshot();
    void restoreResetSnapshotIfAllowed(KonfytReset inheritedOption);

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

    void writeToXmlStream(QXmlStreamWriter* stream) const;
    void readFromXmlStream(QXmlStreamReader* r, QString* errors = nullptr);

    QByteArray toByteArray();
    void fromByteArray(QByteArray data);

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

    KonfytReset mResetOption = KonfytReset::Inherit;
    struct ResetSnapshot
    {
        bool valid = false;
        float gain = 0;
        bool solo = false;
        bool mute = false;
    } mResetSnapshot;

    QString mScript;
    bool mScriptEnabled = false;
    // True to pass original events to output in addition to script processing.
    bool mPassMidiThrough = true;

    MidiValueController gainMidiCtrl;

    void writeSoundfontDataToXmlStream(QXmlStreamWriter* stream) const;
    void readSoundfontDataFromXmlStream(QXmlStreamReader* r, QString* errors);
    void writeSfzDataToXmlStream(QXmlStreamWriter* stream) const;
    void readSfzDataFromXmlStream(QXmlStreamReader* r, QString* errors);
    void writeMidiOutDataToXmlStream(QXmlStreamWriter* stream) const;
    void readMidiOutDataFromXmlStream(QXmlStreamReader* r, QString* errors);
    void writeAudioInDataToXmlStream(QXmlStreamWriter* stream) const;
    void readAudioInDataFromXmlStream(QXmlStreamReader* r, QString* errors);
    void writeScriptToXmlStream(QXmlStreamWriter* stream) const;

    struct LayerScriptData {
        QString content;
        bool enabled = false;
        bool passMidiThrough = true;
    };
    LayerScriptData readScriptFromXmlStream(QXmlStreamReader* r, QString* errors);

    void appendError(QString *errorString, QString msg);
};

typedef QSharedPointer<KonfytPatchLayer> KonfytPatchLayerPtr;

#endif // KONFYT_PATCH_LAYER_H
