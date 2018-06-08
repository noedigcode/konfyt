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

#ifndef KONFYT_PATCH_H
#define KONFYT_PATCH_H

#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

#include "konfytMidiFilter.h"
#include "konfytPatchLayer.h"

#define DEFAULT_GAIN_FOR_NEW_LAYER 0.8

#define XML_PATCH "sfpatch"
#define XML_PATCH_NAME "name"
#define XML_PATCH_NOTE "patchNote"

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



class konfytPatch
{
public:
    konfytPatch();

    // ----------------------------------------------------
    // Patch Info
    // ----------------------------------------------------
    QString getName();
    void setName(QString newName);
    QString getNote();
    void setNote(QString newNote);
    int id_in_project; // Unique ID in project to identify the patch in runtime.

    // ----------------------------------------------------
    // General layer related functions
    // ----------------------------------------------------
    QList<KonfytPatchLayer> getLayerItems();
    KonfytPatchLayer getLayerItem(KonfytPatchLayer item);
    int getNumLayers();
    bool isValidLayerNumber(int layer);
    void removeLayer(KonfytPatchLayer *layer);
    void clearLayers();
    int layerListIndexFromPatchId(KonfytPatchLayer* layer);
    void replaceLayer(KonfytPatchLayer newLayer);
    void setLayerFilter(KonfytPatchLayer* layer, konfytMidiFilter newFilter);
    float getLayerGain(KonfytPatchLayer* layer);
    void setLayerGain(KonfytPatchLayer* layer, float newGain);
    void setLayerSolo(KonfytPatchLayer* layer, bool solo);
    void setLayerMute(KonfytPatchLayer* layer, bool mute);
    void setLayerBus(KonfytPatchLayer* layer, int bus);
    void setLayerMidiInPort(KonfytPatchLayer* layer, int portId);

    // ----------------------------------------------------
    // Soundfont layer related functions
    // ----------------------------------------------------

    KonfytPatchLayer addProgram(konfytSoundfontProgram p);
    KonfytPatchLayer addSfLayer(LayerSoundfontStruct newSfLayer);
    LayerSoundfontStruct getSfLayer(int id_in_engine);
    KonfytPatchLayer getSfLayer_LayerItem(int id_in_engine);
    konfytSoundfontProgram getProgram(int id_in_engine);
    int getNumSfLayers();
    bool isValid_Sf_LayerNumber(int SfLayer);
    QList<KonfytPatchLayer> getSfLayerList();
    float getSfLayerGain(int id_in_engine);
    void setSfLayerGain(int id_in_engine, float newGain);

    // ----------------------------------------------------
    // Carla Plugin functions
    // ----------------------------------------------------
    KonfytPatchLayer addPlugin(LayerCarlaPluginStruct newPlugin);
    LayerCarlaPluginStruct getPlugin(int index_in_engine);
    KonfytPatchLayer getPlugin_LayerItem(int index_in_engine);
    int getPluginCount();
    void setPluginGain(int index_in_engine, float newGain);
    float getPluginGain(int index_in_engine);
    QList<KonfytPatchLayer> getPluginLayerList();

    // ----------------------------------------------------
    // Midi routing
    // ----------------------------------------------------

    QList<int> getMidiOutputPortList_ids();
    QList<LayerMidiOutStruct> getMidiOutputPortList_struct();
    KonfytPatchLayer addMidiOutputPort(int newPort);
    KonfytPatchLayer addMidiOutputPort(LayerMidiOutStruct newPort);

    // ----------------------------------------------------
    // Audio input ports
    // ----------------------------------------------------

    QList<int> getAudioInPortList_ids();
    QList<LayerAudioInStruct> getAudioInPortList_struct();
    KonfytPatchLayer addAudioInPort(int newPort);
    KonfytPatchLayer addAudioInPort(LayerAudioInStruct newPort);

    // ----------------------------------------------------
    // Save/load functions
    // ----------------------------------------------------

    bool savePatchToFile(QString filename);
    bool loadPatchFromFile(QString filename);


    void userMessage(QString msg);
    void error_abort(QString msg);

private:
    QString patchName;
    QString patchNote;  // Custom note for user instructions or to describe the patch

    // List of layers (all types, order is important for user in the patch)
    QList<KonfytPatchLayer> layerList;

    int id_counter; // Counter for unique ID given to layeritem to uniquely identify them.

};

#endif // KONFYT_PATCH_H
