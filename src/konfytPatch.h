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

#ifndef KONFYT_PATCH_H
#define KONFYT_PATCH_H

#include "konfytMidiFilter.h"
#include "konfytPatchLayer.h"

#include <QFile>
#include <QSharedPointer>
#include <QWeakPointer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>


#define XML_PATCH "sfpatch" // TODO: Change this to simply "patch"
#define XML_PATCH_NAME "name"
#define XML_PATCH_NOTE "patchNote"
#define XML_PATCH_ALWAYSACTIVE "alwaysActive"

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

#define XML_PATCH_MIDISENDLIST "midiSendList"

typedef QWeakPointer<KonfytPatchLayer> KfPatchLayerWeakPtr;
typedef QSharedPointer<KonfytPatchLayer> KfPatchLayerSharedPtr;

class KonfytPatch
{
public:

    // ----------------------------------------------------
    // Patch Info
    // ----------------------------------------------------
    QString name() const;
    void setName(QString newName);
    QString note() const;
    void setNote(QString newNote);
    bool alwaysActive = false;

    // ----------------------------------------------------
    // General layer related functions
    // ----------------------------------------------------
    QList<KfPatchLayerWeakPtr> layers();
    KfPatchLayerWeakPtr layer(int index);
    int layerCount() const;
    bool isValidLayerIndex(int layerIndex) const;
    void removeLayer(KfPatchLayerWeakPtr layer);
    void clearLayers();
    int layerIndex(KfPatchLayerWeakPtr layer) const;

    // ----------------------------------------------------
    // Soundfont layer related functions
    // ----------------------------------------------------

    KfPatchLayerWeakPtr addSfLayer(QString soundfontPath, KonfytSoundPreset preset);
    QList<KfPatchLayerWeakPtr> getSfLayerList() const;

    // ----------------------------------------------------
    // SFZ Plugin functions
    // ----------------------------------------------------
    KfPatchLayerWeakPtr addPlugin(LayerSfzData newPlugin, QString name);
    QList<KfPatchLayerWeakPtr> getPluginLayerList() const;

    // ----------------------------------------------------
    // Midi routing
    // ----------------------------------------------------

    QList<int> getMidiOutputPortListProjectIds() const;
    QList<KfPatchLayerWeakPtr> getMidiOutputLayerList() const;
    KfPatchLayerWeakPtr addMidiOutputPort(int newPort);
    KfPatchLayerWeakPtr addMidiOutputPort(LayerMidiOutData newPort);

    // ----------------------------------------------------
    // Audio input ports
    // ----------------------------------------------------

    QList<int> getAudioInPortListProjectIds() const;
    QList<KfPatchLayerWeakPtr> getAudioInLayerList() const;
    KfPatchLayerWeakPtr addAudioInPort(int newPort);
    KfPatchLayerWeakPtr addAudioInPort(LayerAudioInData newPort, QString name);

    // ----------------------------------------------------
    // Save/load functions
    // ----------------------------------------------------

    bool savePatchToFile(QString filename) const;
    bool loadPatchFromFile(QString filename, QString* errors = nullptr);

    QByteArray toByteArray();
    void fromByteArray(QByteArray data);

private:
    QString patchName;
    QString patchNote;  // Custom note for user instructions or to describe the patch

    // List of layers (all types, order is important for user in the patch)
    QList<KfPatchLayerSharedPtr> layerList;

    QList<KfPatchLayerWeakPtr> layersOfType(KonfytPatchLayer::LayerType layerType) const;

    void writeToXmlStream(QXmlStreamWriter* stream) const;
    void readFromXmlStream(QXmlStreamReader* r, QString* errors = nullptr);

    void xmlWriteSfontLayer(QXmlStreamWriter* stream, KfPatchLayerSharedPtr layer) const;
    void xmlReadSfontLayer(QXmlStreamReader* r, QString* errors = nullptr);
    void xmlWriteSfzLayer(QXmlStreamWriter* stream, KfPatchLayerSharedPtr layer) const;
    void xmlReadSfzLayer(QXmlStreamReader* r, QString* errors = nullptr);
    void xmlWriteMidiOutLayer(QXmlStreamWriter* stream, KfPatchLayerSharedPtr layer) const;
    void xmlReadMidiOutLayer(QXmlStreamReader* r, QString* errors = nullptr);
    void xmlWriteAudioInLayer(QXmlStreamWriter* stream, KfPatchLayerSharedPtr layer) const;
    void xmlReadAudioInLayer(QXmlStreamReader* r, QString* errors = nullptr);

    void appendError(QString *errorString, QString msg);
    void error_abort(QString msg);
};

#endif // KONFYT_PATCH_H
