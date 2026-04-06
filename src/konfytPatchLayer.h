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


class PatchLayer
{
public:
    enum LayerType {
        TypeUninitialized    = -1,
        TypeSoundfontProgram = 0,
        TypeSfz              = 1,
        TypeMidiOut          = 2,
        TypeAudioIn          = 3
    };

    // -----------------------------------------------------------------------

    struct SoundfontData
    {
        QString soundfontFilePath;
        KonfytSoundPreset program;
        KfFluidSynth* synthInEngine = nullptr;
        KfJackPluginPorts* portsInJackEngine = nullptr;
    };

    // -----------------------------------------------------------------------

    struct SfzData
    {
        QString path;
        QString midiInPort;
        QString audioOutPortLeft;
        QString audioOutPortRight;
        KfJackPluginPorts* portsInJackEngine = nullptr;
        int indexInEngine = -1;
    };

    // -----------------------------------------------------------------------

    struct MidiOutData
    {
        int portIdInProject;
        KfJackMidiRoute* jackRoute = nullptr;
    };

    // -----------------------------------------------------------------------

    struct AudioInData
    {
        int portIdInProject;
        KfJackAudioRoute* jackRouteLeft = nullptr;
        KfJackAudioRoute* jackRouteRight = nullptr;
    };

    // -----------------------------------------------------------------------

    PatchLayer();

    // Use to initialize layer
    void initLayer(SoundfontData newLayerData);
    void initLayer(SfzData newLayerData);
    void initLayer(MidiOutData newLayerData);
    void initLayer(AudioInData newLayerData);

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

    MidiFilter midiFilter() const;
    void setMidiFilter(MidiFilter midiFilter);

    void setErrorMessage(QString msg);
    bool hasError() const;
    QString errorMessage() const;

    // Depending on the layer type, one of the following is used:
    // TODO: MERGE BELOW INTO LAYER
    SoundfontData soundfontData;
    SfzData       sfzData;
    MidiOutData   midiOutputPortData;
    AudioInData   audioInPortData;

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
    MidiFilter mMidiFilter;
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

public:
    static constexpr const char* XML_SF_LAYER = "sfLayer";
    static constexpr const char* XML_SF_FILENAME = "soundfont_filename";
    static constexpr const char* XML_SF_BANK = "bank";
    static constexpr const char* XML_SF_PROGRAM = "program";
    static constexpr const char* XML_SF_NAME = "name";
    static constexpr const char* XML_SF_GAIN = "gain";
    static constexpr const char* XML_SF_BUS = "bus";
    static constexpr const char* XML_SF_SOLO = "solo";
    static constexpr const char* XML_SF_MUTE = "mute";
    static constexpr const char* XML_SF_MIDI_IN = "midiIn";

    static constexpr const char* XML_SFZ_LAYER = "sfzLayer";
    static constexpr const char* XML_SFZ_NAME = "name";
    static constexpr const char* XML_SFZ_PATH = "path";
    static constexpr const char* XML_SFZ_GAIN = "gain";
    static constexpr const char* XML_SFZ_BUS = "bus";
    static constexpr const char* XML_SFZ_SOLO = "solo";
    static constexpr const char* XML_SFZ_MUTE = "mute";
    static constexpr const char* XML_SFZ_MIDI_IN = "midiIn";

    static constexpr const char* XML_MIDIOUT = "midiOutputPortLayer";
    static constexpr const char* XML_MIDIOUT_PORT = "port";
    static constexpr const char* XML_MIDIOUT_SOLO = "solo";
    static constexpr const char* XML_MIDIOUT_MUTE = "mute";
    static constexpr const char* XML_MIDIOUT_MIDI_IN = "midiIn";

    static constexpr const char* XML_AUDIOIN = "audioInPortLayer";
    static constexpr const char* XML_AUDIOIN_NAME = "name";
    static constexpr const char* XML_AUDIOIN_PORT = "port";
    static constexpr const char* XML_AUDIOIN_GAIN = "gain";
    static constexpr const char* XML_AUDIOIN_BUS = "bus";
    static constexpr const char* XML_AUDIOIN_SOLO = "solo";
    static constexpr const char* XML_AUDIOIN_MUTE = "mute";

    static constexpr const char* XML_SCRIPT = "script";
    static constexpr const char* XML_SCRIPT_CONTENT = "content";
    static constexpr const char* XML_SCRIPT_ENABLED = "enabled";
    static constexpr const char* XML_SCRIPT_PASS_MIDI_THROUGH = "passMidiThrough";

    static constexpr const char* XML_RESET_OPTION = "resetOption";

    static constexpr const char* XML_MIDISENDLIST = "midiSendList";
};

typedef QSharedPointer<PatchLayer> PatchLayerPtr;

#endif // KONFYT_PATCH_LAYER_H
