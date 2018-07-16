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

#include "konfytPatch.h"
#include <iostream>

#define bool_to_10_string(x) x ? "1" : "0"

konfytPatch::konfytPatch()
{
    this->id_counter = 0; // Initialise ID counter for layeritem unique ids.
}


void konfytPatch::userMessage(QString msg)
{
    // dummy function at the moment to handle all userMessage calls.
}

void konfytPatch::error_abort(QString msg)
{
    std::cout << "\n\n" << "Konfyt ERROR, ABORTING: konfytPatch:"
              << msg.toLocal8Bit().constData() << "\n\n";
    abort();
}

int konfytPatch::getNumSfLayers()
{
    return this->getSfLayerList().count();
}


QList<KonfytPatchLayer> konfytPatch::getSfLayerList() const
{
    QList<KonfytPatchLayer> l;
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).getLayerType() == KonfytLayerType_SoundfontProgram) {
            l.append(this->layerList.at(i));
        }
    }
    return l;
}

// Saves the patch to a xml patch file.
bool konfytPatch::savePatchToFile(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // error message.
        return false;
    }

    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeComment("This is a Konfyt patch file.");

    stream.writeStartElement(XML_PATCH); // change this to just patch, here and when loading
    stream.writeAttribute(XML_PATCH_NAME,this->patchName);

    stream.writeTextElement(XML_PATCH_NOTE, this->getNote());

    // Step through all the layers. It's important to save them in the correct order.
    for (int i=0; i<this->layerList.count(); i++) {

        KonfytPatchLayer g = this->layerList.at(i);
        if (g.getLayerType() == KonfytLayerType_SoundfontProgram) {

            // --------------------------------------------

            LayerSoundfontStruct sfLayer = g.sfData;

            stream.writeStartElement(XML_PATCH_SFLAYER);
            // Layer properties
            konfytSoundfontProgram p = sfLayer.program;
            stream.writeTextElement(XML_PATCH_SFLAYER_FILENAME, p.parent_soundfont);
            stream.writeTextElement(XML_PATCH_SFLAYER_BANK, n2s(p.bank));
            stream.writeTextElement(XML_PATCH_SFLAYER_PROGRAM, n2s(p.program));
            stream.writeTextElement(XML_PATCH_SFLAYER_NAME, p.name);
            stream.writeTextElement(XML_PATCH_SFLAYER_GAIN, n2s( sfLayer.gain ));
            stream.writeTextElement(XML_PATCH_SFLAYER_BUS, n2s(g.busIdInProject));
            stream.writeTextElement(XML_PATCH_SFLAYER_SOLO, bool_to_10_string(sfLayer.solo));
            stream.writeTextElement(XML_PATCH_SFLAYER_MUTE, bool_to_10_string(sfLayer.mute));
            stream.writeTextElement(XML_PATCH_SFLAYER_MIDI_IN, n2s(g.midiInPortIdInProject));

            // Midi filter
            konfytMidiFilter f = sfLayer.filter;
            f.writeToXMLStream(&stream);

            stream.writeEndElement(); // sfLayer

            // --------------------------------------------

        } else if (g.getLayerType() == KonfytLayerType_CarlaPlugin) {

            // ----------------------------------------------------------

            LayerCarlaPluginStruct p = g.carlaPluginData;

            stream.writeStartElement(XML_PATCH_SFZLAYER);
            stream.writeTextElement(XML_PATCH_SFZLAYER_NAME, p.name);
            stream.writeTextElement(XML_PATCH_SFZLAYER_PATH, p.path);
            stream.writeTextElement(XML_PATCH_SFZLAYER_GAIN, n2s(p.gain));
            stream.writeTextElement(XML_PATCH_SFZLAYER_BUS, n2s(g.busIdInProject) );
            stream.writeTextElement(XML_PATCH_SFZLAYER_SOLO, bool_to_10_string(p.solo));
            stream.writeTextElement(XML_PATCH_SFZLAYER_MUTE, bool_to_10_string(p.mute));
            stream.writeTextElement(XML_PATCH_SFZLAYER_MIDI_IN, n2s(g.midiInPortIdInProject));

            // Midi filter
            konfytMidiFilter f = p.midiFilter;
            f.writeToXMLStream(&stream);

            stream.writeEndElement();

            // ----------------------------------------------------------

        } else if (g.getLayerType() == KonfytLayerType_MidiOut) {

            // --------------------------------------------------

            LayerMidiOutStruct m = g.midiOutputPortData;

            stream.writeStartElement(XML_PATCH_MIDIOUT);
            stream.writeTextElement(XML_PATCH_MIDIOUT_PORT, n2s( m.portIdInProject ));
            stream.writeTextElement(XML_PATCH_MIDIOUT_SOLO, bool_to_10_string(m.solo));
            stream.writeTextElement(XML_PATCH_MIDIOUT_MUTE, bool_to_10_string(m.mute));
            stream.writeTextElement(XML_PATCH_MIDIOUT_MIDI_IN, n2s(g.midiInPortIdInProject));

            // Midi filter
            konfytMidiFilter f = m.filter;
            f.writeToXMLStream(&stream);

            stream.writeEndElement();

            // --------------------------------------------------

        } else if (g.getLayerType() == KonfytLayerType_AudioIn) {

            LayerAudioInStruct a = g.audioInPortData;

            stream.writeStartElement(XML_PATCH_AUDIOIN);

            stream.writeTextElement(XML_PATCH_AUDIOIN_NAME, a.name);
            stream.writeTextElement(XML_PATCH_AUDIOIN_PORT, n2s(a.portIdInProject));
            stream.writeTextElement(XML_PATCH_AUDIOIN_GAIN, n2s(a.gain));
            stream.writeTextElement(XML_PATCH_AUDIOIN_BUS, n2s(g.busIdInProject));
            stream.writeTextElement(XML_PATCH_AUDIOIN_SOLO, bool_to_10_string(a.solo));
            stream.writeTextElement(XML_PATCH_AUDIOIN_MUTE, bool_to_10_string(a.mute));

            stream.writeEndElement();
        }

    }

    stream.writeEndElement(); // patch

    stream.writeEndDocument();

    file.close();
    return true;
}

// Loads a pach from an patch xml file.
bool konfytPatch::loadPatchFromFile(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // error message!
        return false;
    }


    this->clearLayers();

    QXmlStreamReader r(&file);
    r.setNamespaceProcessing(false);

    while (r.readNextStartElement()) { // patch

        // Get the patch name attribute
        QXmlStreamAttributes ats =  r.attributes();
        if (ats.count()) {
            this->patchName = ats.at(0).value().toString();
        }

        while (r.readNextStartElement()) {

            if (r.name() == XML_PATCH_NOTE) {

                this->setNote(r.readElementText());

            } else if (r.name() == XML_PATCH_SFLAYER) { // soundfont layer

                LayerSoundfontStruct sfLayer = LayerSoundfontStruct();
                int bus = 0;
                int midiIn = 0;

                while (r.readNextStartElement()) { // layer properties

                    if (r.name() == XML_PATCH_SFLAYER_FILENAME) {
                        sfLayer.program.parent_soundfont = r.readElementText();
                    } else if (r.name() == XML_PATCH_SFLAYER_BANK) {
                        sfLayer.program.bank = r.readElementText().toInt();
                    } else if (r.name() == XML_PATCH_SFLAYER_PROGRAM) {
                        sfLayer.program.program = r.readElementText().toInt();
                    } else if (r.name() == XML_PATCH_SFLAYER_NAME) {
                        sfLayer.program.name = r.readElementText();
                    } else if (r.name() == XML_PATCH_SFLAYER_GAIN) {
                        sfLayer.gain = r.readElementText().toFloat();
                    } else if (r.name() == XML_PATCH_SFLAYER_SOLO) {
                        sfLayer.solo = (r.readElementText() == "1");
                    } else if (r.name() == XML_PATCH_SFLAYER_MUTE) {
                        sfLayer.mute = (r.readElementText() == "1");
                    } else if (r.name() == XML_MIDIFILTER) {
                        sfLayer.filter.readFromXMLStream(&r);
                    } else if (r.name() == XML_PATCH_SFLAYER_BUS) {
                        bus = r.readElementText().toInt();
                    } else if (r.name() == XML_PATCH_SFLAYER_MIDI_IN) {
                        midiIn = r.readElementText().toInt();
                    } else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized sfLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }

                }

                KonfytPatchLayer newLayer = this->addSfLayer(sfLayer);
                this->setLayerBus(&newLayer, bus);
                this->setLayerMidiInPort(&newLayer, midiIn);


            } else if (r.name() == XML_PATCH_SFZLAYER) { // SFZ Layer

                LayerCarlaPluginStruct p = LayerCarlaPluginStruct();
                int bus = 0;
                int midiIn = 0;

                while (r.readNextStartElement()) {

                    if (r.name() == XML_PATCH_SFZLAYER_NAME) {
                        p.name = r.readElementText();
                    } else if (r.name() == XML_PATCH_SFZLAYER_PATH) {
                        p.path = r.readElementText();
                    } else if (r.name() == XML_PATCH_SFZLAYER_GAIN) {
                        p.gain = r.readElementText().toFloat();
                    } else if (r.name() == XML_PATCH_SFZLAYER_BUS) {
                        bus = r.readElementText().toInt();
                    } else if (r.name() == XML_PATCH_SFZLAYER_MIDI_IN) {
                        midiIn = r.readElementText().toInt();
                    } else if (r.name() == XML_PATCH_SFZLAYER_SOLO) {
                        p.solo = (r.readElementText() == "1");
                    } else if (r.name() == XML_PATCH_SFZLAYER_MUTE) {
                        p.mute = (r.readElementText() == "1");
                    } else if (r.name() == XML_MIDIFILTER) {
                        p.midiFilter.readFromXMLStream(&r);
                    } else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized sfzLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }

                }

                p.pluginType = KonfytCarlaPluginType_SFZ;

                KonfytPatchLayer newLayer = this->addPlugin(p);
                this->setLayerBus(&newLayer, bus);
                this->setLayerMidiInPort(&newLayer, midiIn);


            } else if (r.name() == XML_PATCH_MIDIOUT) {

                LayerMidiOutStruct mp;
                int midiIn = 0;

                while (r.readNextStartElement()) { // port
                    if (r.name() == XML_PATCH_MIDIOUT_PORT) {
                        mp.portIdInProject = r.readElementText().toInt();
                    } else if (r.name() == XML_PATCH_MIDIOUT_SOLO) {
                        mp.solo = (r.readElementText() == "1");
                    } else if (r.name() == XML_PATCH_MIDIOUT_MUTE) {
                        mp.mute = (r.readElementText() == "1");
                    } else if (r.name() == XML_MIDIFILTER) {
                        mp.filter.readFromXMLStream(&r);
                    } else if (r.name() == XML_PATCH_MIDIOUT_MIDI_IN) {
                        midiIn = r.readElementText().toInt();
                    } else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized midiOutputPortLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

                // Add new midi port
                KonfytPatchLayer newLayer = this->addMidiOutputPort(mp);
                this->setLayerMidiInPort(&newLayer, midiIn);

            } else if (r.name() == XML_PATCH_AUDIOIN) {

                LayerAudioInStruct a;
                int bus = 0;

                while (r.readNextStartElement()) {
                    if (r.name() == XML_PATCH_AUDIOIN_NAME) { a.name = r.readElementText(); }
                    else if (r.name() == XML_PATCH_AUDIOIN_PORT) { a.portIdInProject = r.readElementText().toInt(); }
                    else if (r.name() == XML_PATCH_AUDIOIN_GAIN) { a.gain = r.readElementText().toFloat(); }
                    else if (r.name() == XML_PATCH_AUDIOIN_BUS) { bus = r.readElementText().toInt(); }
                    else if (r.name() == XML_PATCH_AUDIOIN_SOLO) { a.solo = (r.readElementText() == "1"); }
                    else if (r.name() == XML_PATCH_AUDIOIN_MUTE) { a.mute = (r.readElementText() == "1"); }
                    else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized audioInPortLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

                KonfytPatchLayer newLayer = this->addAudioInPort(a);
                this->setLayerBus(&newLayer, bus);

            } else {
                userMessage("konfytPatch::loadPatchFromFile: "
                            "Unrecognized layer type: " + r.name().toString() );
                r.skipCurrentElement(); // not any of the layer types
            }
        }
    }



    file.close();
    return true;
}

// Returns true if the layer number represents a valid layer.
// This includes all types of layers.
bool konfytPatch::isValidLayerNumber(int layer)
{
    if ( (layer>=0) && (layer < layerList.count()) ) {
        return true;
    } else {
        return false;
    }
}

bool konfytPatch::isValid_Sf_LayerNumber(int SfLayer)
{
    if ( (SfLayer>=0) && (SfLayer < this->getNumSfLayers()) ) {
        return true;
    } else {
        return false;
    }
}

void konfytPatch::removeLayer(KonfytPatchLayer* layer)
{
    // Identify layer using the unique ID in the layeritem.
    int id = layer->ID_in_patch;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).ID_in_patch == id) {
            layerList.removeAt(i);
            return;
        }
    }
}

// Removes all the layers in the patch.
void konfytPatch::clearLayers()
{
    this->layerList.clear();
}

/* Find layer that matches ID_in_patch and return index in layerList.
 * Return -1 if not found. */
int konfytPatch::layerListIndexFromPatchId(KonfytPatchLayer *layer)
{
    for (int i=0; i < layerList.count(); i++) {
        if (layerList.at(i).ID_in_patch == layer->ID_in_patch) {
            return i;
        }
    }
    // Not found
    return -1;
}

// Replace a layer. Layer is identified using unique patch_id in layer.
void konfytPatch::replaceLayer(KonfytPatchLayer newLayer)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(&newLayer);

    if (index >= 0) {
        this->layerList.replace(index, newLayer);
    } else {
        // No layer was found and replaced. This is probably a logic error somewhere.
        error_abort("replaceLayer: Layer with id " + n2s(newLayer.ID_in_patch) + " is not in the patch's layerList.");
    }
}

// Set the midi filter for the layer for which the patch_id matches that of
// the specified layer item.
void konfytPatch::setLayerFilter(KonfytPatchLayer *layer, konfytMidiFilter newFilter)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = this->layerList.at(index);
        g.setMidiFilter(newFilter);
        this->layerList.replace(index, g);
    } else {
        // No layer was found. This is probably a logic error somewhere.
        error_abort("setLayerFilter: Layer with id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
    }
}

float konfytPatch::getLayerGain(KonfytPatchLayer *layer)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        return this->layerList.at(index).getGain();
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("getLayerGain: Layer with id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
        return 0;
    }
}

void konfytPatch::setLayerGain(KonfytPatchLayer *layer, float newGain)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.setGain(newGain);
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerGain: Layer with id " + n2s(layer->ID_in_patch) +  " is not in the patch's layerList.");
    }
}

void konfytPatch::setLayerSolo(KonfytPatchLayer *layer, bool solo)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.setSolo(solo);
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerSolo: Layer with id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
    }
}

void konfytPatch::setLayerMute(KonfytPatchLayer *layer, bool mute)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.setMute(mute);
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerMute: Layer with id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
    }
}

void konfytPatch::setLayerBus(KonfytPatchLayer *layer, int bus)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);

        if ( (g.getLayerType() == KonfytLayerType_SoundfontProgram)
             || (g.getLayerType() == KonfytLayerType_AudioIn)
             || (g.getLayerType() == KonfytLayerType_CarlaPlugin) ) {

            g.busIdInProject = bus;

        } else {
            error_abort("setLayerBus: invalid LayerType");
        }

        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerBus: Layer with id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
    }
}

void konfytPatch::setLayerMidiInPort(KonfytPatchLayer *layer, int portId)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.midiInPortIdInProject = portId;
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerMidiInPort: Layer with id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
    }
}

KonfytPatchLayer konfytPatch::addProgram(konfytSoundfontProgram p)
{
    // Set up new soundfont program layer structure
    LayerSoundfontStruct l = LayerSoundfontStruct();
    l.program = p;
    l.gain = DEFAULT_GAIN_FOR_NEW_LAYER; // default gain
    l.solo = false;
    l.mute = false;

    // Set up a new layer item
    // (a layer item is initialised with an unique id which will
    //  allow us to identify it later on.)
    KonfytPatchLayer g;
    g.initLayer(this->id_counter++, l);

    // Add to layer list
    layerList.append(g);

    return g;
}

KonfytPatchLayer konfytPatch::addSfLayer(LayerSoundfontStruct newSfLayer)
{
    // Set up a new layer item
    // (a layer item is initialised with an unique id which will
    //  allow us to identify it later on.)
    KonfytPatchLayer g;
    g.initLayer(this->id_counter++, newSfLayer);

    // add to layer list
    layerList.append(g);

    return g;
}


// Get soundfont program of the soundfont program layer with the specified index.
konfytSoundfontProgram konfytPatch::getProgram(int id_in_engine)
{
    return this->getSfLayer(id_in_engine).program;
}

// Get sfLayerStruct of the soundfont program layer with the specified index.
LayerSoundfontStruct konfytPatch::getSfLayer(int id_in_engine)
{
    return this->getSfLayer_LayerItem(id_in_engine).sfData;
}

KonfytPatchLayer konfytPatch::getSfLayer_LayerItem(int id_in_engine)
{
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).getLayerType() == KonfytLayerType_SoundfontProgram) {
            if (this->layerList.at(i).sfData.indexInEngine == id_in_engine) {
                return this->layerList.at(i);
            }
        }
    }
    // If we did not return, the layer was not found. This is probably a logic error.
    error_abort("getSfLayer_LayerItem: invalid id_in_engine: " + n2s(id_in_engine) );

    KonfytPatchLayer g;
    return g;
}



float konfytPatch::getSfLayerGain(int id_in_engine)
{
    return this->getSfLayer(id_in_engine).gain;
}

void konfytPatch::setSfLayerGain(int id_in_engine, float newGain)
{
    KonfytPatchLayer g = this->getSfLayer_LayerItem(id_in_engine);
    g.setGain(newGain);
    this->replaceLayer(g);
}

int konfytPatch::getNumLayers()
{
    return this->layerList.count();
}



void konfytPatch::setName(QString newName)
{
    this->patchName = newName;
}

QString konfytPatch::getName()
{
    return this->patchName;
}

void konfytPatch::setNote(QString newNote)
{
    this->patchNote = newNote;
}

QString konfytPatch::getNote()
{
    return this->patchNote;
}

QList<KonfytPatchLayer> konfytPatch::getLayerItems()
{
    return this->layerList;
}

// Returns the layeritem in the patch that matches the unique ID
// of the layerItem specified.
KonfytPatchLayer konfytPatch::getLayerItem(KonfytPatchLayer item)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == item.ID_in_patch) {
            return this->layerList.at(i);
        }
    }

    // No layer was found that matches the ID. This is probably a logic error somewhere.
    error_abort("getLayerItem: LayerItem with patch_id " + n2s(item.ID_in_patch) + " is not in the patch's layerList.");
    KonfytPatchLayer l;
    return l;
}

// Return list of port ids of the patch's midi output ports.
QList<int> konfytPatch::getMidiOutputPortList_ids() const
{
    QList<int> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_MidiOut) {
            l.append(layerList.at(i).midiOutputPortData.portIdInProject);
        }
    }
    return l;
}

// Return list of port ids of patch's audio input ports.
QList<int> konfytPatch::getAudioInPortList_ids() const
{
    QList<int> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_AudioIn) {
            l.append(layerList.at(i).audioInPortData.portIdInProject);
        }
    }
    return l;
}

QList<LayerAudioInStruct> konfytPatch::getAudioInPortList_struct()
{
    QList<LayerAudioInStruct> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_AudioIn) {
            l.append(layerList.at(i).audioInPortData);
        }
    }
    return l;
}

// Return list of midiOutputPortStruct's of the patch's midi output ports.
QList<LayerMidiOutStruct> konfytPatch::getMidiOutputPortList_struct()
{
    QList<LayerMidiOutStruct> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_MidiOut) {
            l.append(layerList.at(i).midiOutputPortData);
        }
    }
    return l;
}



KonfytPatchLayer konfytPatch::addAudioInPort(int newPort)
{
    // Set up a default new audio input port with the specified port number
    LayerAudioInStruct a = LayerAudioInStruct();
    a.name = "Audio Input Port " + n2s(newPort);
    a.portIdInProject = newPort;
    a.gain = DEFAULT_GAIN_FOR_NEW_LAYER;
    a.mute = false;
    a.solo = false;

    return addAudioInPort(a);
}

KonfytPatchLayer konfytPatch::addAudioInPort(LayerAudioInStruct newPort)
{
    KonfytPatchLayer g;

    // Check if port not already in patch
    QList<int> audioInPortList = this->getAudioInPortList_ids();
    if (audioInPortList.contains(newPort.portIdInProject) == false) {
        // Set up layer item
        // Layer item is initialised with a unique ID, which will later
        // be used to uniquely identify the item.
        g.initLayer(this->id_counter++, newPort);
        // Add to layer list
        layerList.append(g);
    } else {
        // We do not handle duplicate ports well yet. Throw an error.
        error_abort("addAudioInPort: Audio input port " + n2s(newPort.portIdInProject)
                    + " already exists in patch.");
    }

    return g;
}

// TODO: Change so that function returns true or false for success, and takes
// a pointer, of which the location will be set to the LayerItem.
KonfytPatchLayer konfytPatch::addMidiOutputPort(int newPort)
{
    LayerMidiOutStruct m = LayerMidiOutStruct();
    m.portIdInProject = newPort;

    return addMidiOutputPort(m);
}

KonfytPatchLayer konfytPatch::addMidiOutputPort(LayerMidiOutStruct newPort)
{
    KonfytPatchLayer g;

    QList<int> midiOutputPortList = this->getMidiOutputPortList_ids();
    if (midiOutputPortList.contains(newPort.portIdInProject) == false) {
        // Set up layer item
        // Layer item is initialised with a unique ID, which will later
        // be used to uniquely identify the item.
        g.initLayer(this->id_counter++, newPort);
        // Add to layer list
        layerList.append(g);
    } else {
        // We don't know how to handle duplicate ports yet. Let's just throw an error.
        error_abort("addMidiOutputPort: Midi output port " + n2s(newPort.portIdInProject)
                    + " already exists in patch.");
    }

    return g;
}


KonfytPatchLayer konfytPatch::addPlugin(LayerCarlaPluginStruct newPlugin)
{
    // Create and initialise new layeritem.
    // Item is initialised with a unique ID, which will later be used to
    // uniquely identify it.
    KonfytPatchLayer g;
    g.initLayer(this->id_counter++, newPlugin);

    // Add to layer list
    layerList.append(g);

    return g;
}


LayerCarlaPluginStruct konfytPatch::getPlugin(int index_in_engine)
{
    return this->getPlugin_LayerItem(index_in_engine).carlaPluginData;
}

KonfytPatchLayer konfytPatch::getPlugin_LayerItem(int index_in_engine)
{
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).getLayerType() == KonfytLayerType_CarlaPlugin) {
            if (this->layerList.at(i).carlaPluginData.indexInEngine == index_in_engine) {
                return this->layerList.at(i);
            }
        }
    }
    // If we did not return, the layer was not found. This is probably a logic error.
    error_abort("getPlugin_LayerItem: invalid index_in_engine: " + n2s(index_in_engine) );

    KonfytPatchLayer g;
    return g;
}

int konfytPatch::getPluginCount()
{
    return this->getPluginLayerList().count();
}


void konfytPatch::setPluginGain(int index_in_engine, float newGain)
{
    KonfytPatchLayer g = this->getPlugin_LayerItem(index_in_engine);
    g.setGain(newGain);
    this->replaceLayer(g);
}

float konfytPatch::getPluginGain(int index_in_engine)
{
    return this->getPlugin(index_in_engine).gain;
}

QList<KonfytPatchLayer> konfytPatch::getPluginLayerList() const
{
    QList<KonfytPatchLayer> l;
    for (int i=0; i < this->layerList.count(); i++) {
        if ( this->layerList.at(i).getLayerType() == KonfytLayerType_CarlaPlugin ) {
            l.append(this->layerList.at(i));
        }
    }
    return l;
}



