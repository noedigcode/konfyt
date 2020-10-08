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

#include "konfytPatch.h"
#include <iostream>

#define bool_to_10_string(x) x ? "1" : "0"

KonfytPatch::KonfytPatch()
{

}


void KonfytPatch::userMessage(QString /*msg*/)
{
    // dummy function at the moment to handle all userMessage calls.
}

void KonfytPatch::error_abort(QString msg)
{
    std::cout << "\n\n" << "Konfyt ERROR, ABORTING: konfytPatch:"
              << msg.toLocal8Bit().constData() << "\n\n";
    abort();
}

int KonfytPatch::getNumSfLayers()
{
    return this->getSfLayerList().count();
}


QList<KonfytPatchLayer> KonfytPatch::getSfLayerList() const
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
bool KonfytPatch::savePatchToFile(QString filename)
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
    stream.writeTextElement(XML_PATCH_ALWAYSACTIVE, QVariant(alwaysActive).toString());

    // Step through all the layers. It's important to save them in the correct order.
    for (int i=0; i<this->layerList.count(); i++) {

        KonfytPatchLayer g = this->layerList.at(i);

        if (g.getLayerType() == KonfytLayerType_SoundfontProgram) {

            // --------------------------------------------

            LayerSoundfontStruct sfLayer = g.soundfontData;

            stream.writeStartElement(XML_PATCH_SFLAYER);
            // Layer properties
            KonfytSoundfontProgram p = sfLayer.program;
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
            KonfytMidiFilter f = sfLayer.filter;
            f.writeToXMLStream(&stream);

            stream.writeEndElement(); // sfLayer

            // --------------------------------------------

        } else if (g.getLayerType() == KonfytLayerType_Sfz) {

            // ----------------------------------------------------------

            LayerSfzStruct p = g.sfzData;

            stream.writeStartElement(XML_PATCH_SFZLAYER);
            stream.writeTextElement(XML_PATCH_SFZLAYER_NAME, p.name);
            stream.writeTextElement(XML_PATCH_SFZLAYER_PATH, p.path);
            stream.writeTextElement(XML_PATCH_SFZLAYER_GAIN, n2s(p.gain));
            stream.writeTextElement(XML_PATCH_SFZLAYER_BUS, n2s(g.busIdInProject) );
            stream.writeTextElement(XML_PATCH_SFZLAYER_SOLO, bool_to_10_string(p.solo));
            stream.writeTextElement(XML_PATCH_SFZLAYER_MUTE, bool_to_10_string(p.mute));
            stream.writeTextElement(XML_PATCH_SFZLAYER_MIDI_IN, n2s(g.midiInPortIdInProject));

            // Midi filter
            KonfytMidiFilter f = p.midiFilter;
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
            KonfytMidiFilter f = m.filter;
            f.writeToXMLStream(&stream);

            // MIDI Send list
            if (g.midiSendList.count()) {
                stream.writeStartElement(XML_PATCH_MIDISENDLIST);

                foreach (MidiSendItem item, g.midiSendList) {
                    item.writeToXMLStream(&stream);
                }

                stream.writeEndElement();
            }

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
bool KonfytPatch::loadPatchFromFile(QString filename)
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

            } else if (r.name() == XML_PATCH_ALWAYSACTIVE) {

                this->alwaysActive = QVariant(r.readElementText()).toBool();

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

                LayerSfzStruct p = LayerSfzStruct();
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

                KonfytPatchLayer newLayer = this->addPlugin(p);
                this->setLayerBus(&newLayer, bus);
                this->setLayerMidiInPort(&newLayer, midiIn);


            } else if (r.name() == XML_PATCH_MIDIOUT) {

                LayerMidiOutStruct mp;
                int midiIn = 0;
                QList<MidiSendItem> midiSendItems;

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
                    } else if (r.name() == XML_PATCH_MIDISENDLIST) {
                        while (r.readNextStartElement()) {
                            if (r.name() == XML_MIDI_SEND_ITEM) {
                                MidiSendItem item;
                                QString error = item.readFromXmlStream(&r);
                                if (!error.isEmpty()) {
                                    userMessage("konfytPatch::loadPatchFromFile:");
                                    userMessage(error);
                                }
                                midiSendItems.append(item);
                            } else {
                                userMessage("konfytPatch::loadPatchFromFile: "
                                            "Unrecognized MIDI send list element: " + r.name().toString());
                                r.skipCurrentElement();
                            }
                        }
                    } else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized midiOutputPortLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

                // Add new midi port
                KonfytPatchLayer newLayer = this->addMidiOutputPort(mp);
                this->setLayerMidiInPort(&newLayer, midiIn);
                this->setLayerMidiSendEvents(&newLayer, midiSendItems);

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
bool KonfytPatch::isValidLayerNumber(int layer)
{
    if ( (layer>=0) && (layer < layerList.count()) ) {
        return true;
    } else {
        return false;
    }
}

bool KonfytPatch::isValid_Sf_LayerNumber(int SfLayer)
{
    if ( (SfLayer>=0) && (SfLayer < this->getNumSfLayers()) ) {
        return true;
    } else {
        return false;
    }
}

void KonfytPatch::removeLayer(KonfytPatchLayer* layer)
{
    // Identify layer using the unique ID in the layeritem.
    int id = layer->idInPatch;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).idInPatch == id) {
            layerList.removeAt(i);
            return;
        }
    }
}

// Removes all the layers in the patch.
void KonfytPatch::clearLayers()
{
    this->layerList.clear();
}

/* Find layer that matches ID_in_patch and return index in layerList.
 * Return -1 if not found. */
int KonfytPatch::layerListIndexFromPatchId(KonfytPatchLayer *layer)
{
    for (int i=0; i < layerList.count(); i++) {
        if (layerList.at(i).idInPatch == layer->idInPatch) {
            return i;
        }
    }
    // Not found
    return -1;
}

// Replace a layer. Layer is identified using unique patch_id in layer.
void KonfytPatch::replaceLayer(KonfytPatchLayer newLayer)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(&newLayer);

    if (index >= 0) {
        this->layerList.replace(index, newLayer);
    } else {
        // No layer was found and replaced. This is probably a logic error somewhere.
        error_abort("replaceLayer: Layer with id " + n2s(newLayer.idInPatch) + " is not in the patch's layerList.");
    }
}

// Set the midi filter for the layer for which the patch_id matches that of
// the specified layer item.
void KonfytPatch::setLayerFilter(KonfytPatchLayer *layer, KonfytMidiFilter newFilter)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = this->layerList.at(index);
        g.setMidiFilter(newFilter);
        this->layerList.replace(index, g);
    } else {
        // No layer was found. This is probably a logic error somewhere.
        error_abort("setLayerFilter: Layer with id " + n2s(layer->idInPatch) + " is not in the patch's layerList.");
    }
}

float KonfytPatch::getLayerGain(KonfytPatchLayer *layer)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        return this->layerList.at(index).getGain();
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("getLayerGain: Layer with id " + n2s(layer->idInPatch) + " is not in the patch's layerList.");
        return 0;
    }
}

void KonfytPatch::setLayerGain(KonfytPatchLayer *layer, float newGain)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.setGain(newGain);
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerGain: Layer with id " + n2s(layer->idInPatch) +  " is not in the patch's layerList.");
    }
}

void KonfytPatch::setLayerSolo(KonfytPatchLayer *layer, bool solo)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.setSolo(solo);
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerSolo: Layer with id " + n2s(layer->idInPatch) + " is not in the patch's layerList.");
    }
}

void KonfytPatch::setLayerMute(KonfytPatchLayer *layer, bool mute)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.setMute(mute);
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerMute: Layer with id " + n2s(layer->idInPatch) + " is not in the patch's layerList.");
    }
}

void KonfytPatch::setLayerBus(KonfytPatchLayer *layer, int bus)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);

        if ( (g.getLayerType() == KonfytLayerType_SoundfontProgram)
             || (g.getLayerType() == KonfytLayerType_AudioIn)
             || (g.getLayerType() == KonfytLayerType_Sfz) ) {

            g.busIdInProject = bus;

        } else {
            error_abort("setLayerBus: invalid LayerType");
        }

        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerBus: Layer with id " + n2s(layer->idInPatch) + " is not in the patch's layerList.");
    }
}

void KonfytPatch::setLayerMidiInPort(KonfytPatchLayer *layer, int portId)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.midiInPortIdInProject = portId;
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerMidiInPort: Layer with id " + n2s(layer->idInPatch) + " is not in the patch's layerList.");
    }
}

void KonfytPatch::setLayerMidiSendEvents(KonfytPatchLayer *layer, QList<MidiSendItem> events)
{
    // Find layer that matches ID_in_patch
    int index = layerListIndexFromPatchId(layer);

    if (index >= 0) {
        KonfytPatchLayer g = layerList.at(index);
        g.midiSendList = events;
        this->replaceLayer(g);
    } else {
        // No layer was found that matches the ID. This is probably a logic error somewhere.
        error_abort("setLayerMidiSendEvents: Layer with id " + n2s(layer->idInPatch) + " is not in the patch's layerList.");
    }
}

KonfytPatchLayer KonfytPatch::addProgram(KonfytSoundfontProgram p)
{
    // Set up new soundfont program layer structure
    LayerSoundfontStruct sfData;
    sfData.program = p;

    // Set up a new layer item
    // (a layer item is initialised with an unique id which will
    //  allow us to identify it later on.)
    KonfytPatchLayer layer;
    layer.initLayer(this->idCounter++, sfData);

    // Add to layer list
    layerList.append(layer);

    return layer;
}

KonfytPatchLayer KonfytPatch::addSfLayer(LayerSoundfontStruct newSfLayer)
{
    // Set up a new layer item
    // (a layer item is initialised with an unique id which will
    //  allow us to identify it later on.)
    KonfytPatchLayer g;
    g.initLayer(this->idCounter++, newSfLayer);

    // add to layer list
    layerList.append(g);

    return g;
}


// Get soundfont program of the soundfont program layer with the specified index.
KonfytSoundfontProgram KonfytPatch::getProgram(int id_in_engine)
{
    return this->getSfLayer(id_in_engine).program;
}

// Get sfLayerStruct of the soundfont program layer with the specified index.
LayerSoundfontStruct KonfytPatch::getSfLayer(int id_in_engine)
{
    return this->getSfLayer_LayerItem(id_in_engine).soundfontData;
}

KonfytPatchLayer KonfytPatch::getSfLayer_LayerItem(int id_in_engine)
{
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).getLayerType() == KonfytLayerType_SoundfontProgram) {
            if (this->layerList.at(i).soundfontData.indexInEngine == id_in_engine) {
                return this->layerList.at(i);
            }
        }
    }
    // If we did not return, the layer was not found. This is probably a logic error.
    error_abort("getSfLayer_LayerItem: invalid id_in_engine: " + n2s(id_in_engine) );

    KonfytPatchLayer g;
    return g;
}



float KonfytPatch::getSfLayerGain(int id_in_engine)
{
    return this->getSfLayer(id_in_engine).gain;
}

void KonfytPatch::setSfLayerGain(int id_in_engine, float newGain)
{
    KonfytPatchLayer g = this->getSfLayer_LayerItem(id_in_engine);
    g.setGain(newGain);
    this->replaceLayer(g);
}

int KonfytPatch::getNumLayers()
{
    return this->layerList.count();
}



void KonfytPatch::setName(QString newName)
{
    this->patchName = newName;
}

QString KonfytPatch::getName()
{
    return this->patchName;
}

void KonfytPatch::setNote(QString newNote)
{
    this->patchNote = newNote;
}

QString KonfytPatch::getNote()
{
    return this->patchNote;
}

QList<KonfytPatchLayer> KonfytPatch::getLayerItems()
{
    return this->layerList;
}

// Returns the layeritem in the patch that matches the unique ID
// of the layerItem specified.
KonfytPatchLayer KonfytPatch::getLayerItem(KonfytPatchLayer item)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).idInPatch == item.idInPatch) {
            return this->layerList.at(i);
        }
    }

    // No layer was found that matches the ID. This is probably a logic error somewhere.
    error_abort("getLayerItem: LayerItem with patch_id " + n2s(item.idInPatch) + " is not in the patch's layerList.");
    KonfytPatchLayer l;
    return l;
}

// Return list of port ids of the patch's midi output ports.
QList<int> KonfytPatch::getMidiOutputPortList_ids() const
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
QList<int> KonfytPatch::getAudioInPortList_ids() const
{
    QList<int> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_AudioIn) {
            l.append(layerList.at(i).audioInPortData.portIdInProject);
        }
    }
    return l;
}

QList<LayerAudioInStruct> KonfytPatch::getAudioInPortList_struct() const
{
    QList<LayerAudioInStruct> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_AudioIn) {
            l.append(layerList.at(i).audioInPortData);
        }
    }
    return l;
}

QList<KonfytPatchLayer> KonfytPatch::getAudioInLayerList() const
{
    QList<KonfytPatchLayer> l;
    for (int i=0; i < this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_AudioIn) {
            l.append(layerList.at(i));
        }
    }
    return l;
}

// Return list of midiOutputPortStruct's of the patch's midi output ports.
QList<LayerMidiOutStruct> KonfytPatch::getMidiOutputPortList_struct() const
{
    QList<LayerMidiOutStruct> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_MidiOut) {
            l.append(layerList.at(i).midiOutputPortData);
        }
    }
    return l;
}

QList<KonfytPatchLayer> KonfytPatch::getMidiOutputLayerList() const
{
    QList<KonfytPatchLayer> l;
    for (int i=0; i < this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_MidiOut) {
            l.append(layerList.at(i));
        }
    }
    return l;
}



KonfytPatchLayer KonfytPatch::addAudioInPort(int newPort)
{
    // Set up a default new audio input port with the specified port number
    LayerAudioInStruct a = LayerAudioInStruct();
    a.name = "Audio Input Port " + n2s(newPort);
    a.portIdInProject = newPort;

    return addAudioInPort(a);
}

KonfytPatchLayer KonfytPatch::addAudioInPort(LayerAudioInStruct newPort)
{
    KonfytPatchLayer g;

    // Check if port not already in patch
    QList<int> audioInPortList = this->getAudioInPortList_ids();
    if (audioInPortList.contains(newPort.portIdInProject) == false) {
        // Set up layer item
        // Layer item is initialised with a unique ID, which will later
        // be used to uniquely identify the item.
        g.initLayer(this->idCounter++, newPort);
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
KonfytPatchLayer KonfytPatch::addMidiOutputPort(int newPort)
{
    LayerMidiOutStruct m = LayerMidiOutStruct();
    m.portIdInProject = newPort;

    return addMidiOutputPort(m);
}

KonfytPatchLayer KonfytPatch::addMidiOutputPort(LayerMidiOutStruct newPort)
{
    KonfytPatchLayer g;

    QList<int> midiOutputPortList = this->getMidiOutputPortList_ids();
    if (midiOutputPortList.contains(newPort.portIdInProject) == false) {
        // Set up layer item
        // Layer item is initialised with a unique ID, which will later
        // be used to uniquely identify the item.
        g.initLayer(this->idCounter++, newPort);
        // Add to layer list
        layerList.append(g);
    } else {
        // We don't know how to handle duplicate ports yet. Let's just throw an error.
        error_abort("addMidiOutputPort: Midi output port " + n2s(newPort.portIdInProject)
                    + " already exists in patch.");
    }

    return g;
}


KonfytPatchLayer KonfytPatch::addPlugin(LayerSfzStruct newPlugin)
{
    // Create and initialise new layeritem.
    // Item is initialised with a unique ID, which will later be used to
    // uniquely identify it.
    KonfytPatchLayer g;
    g.initLayer(this->idCounter++, newPlugin);

    // Add to layer list
    layerList.append(g);

    return g;
}


LayerSfzStruct KonfytPatch::getPlugin(int index_in_engine)
{
    return this->getPlugin_LayerItem(index_in_engine).sfzData;
}

KonfytPatchLayer KonfytPatch::getPlugin_LayerItem(int index_in_engine)
{
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).getLayerType() == KonfytLayerType_Sfz) {
            if (this->layerList.at(i).sfzData.indexInEngine == index_in_engine) {
                return this->layerList.at(i);
            }
        }
    }
    // If we did not return, the layer was not found. This is probably a logic error.
    error_abort("getPlugin_LayerItem: invalid index_in_engine: " + n2s(index_in_engine) );

    KonfytPatchLayer g;
    return g;
}

int KonfytPatch::getPluginCount()
{
    return this->getPluginLayerList().count();
}


void KonfytPatch::setPluginGain(int index_in_engine, float newGain)
{
    KonfytPatchLayer g = this->getPlugin_LayerItem(index_in_engine);
    g.setGain(newGain);
    this->replaceLayer(g);
}

float KonfytPatch::getPluginGain(int index_in_engine)
{
    return this->getPlugin(index_in_engine).gain;
}

QList<KonfytPatchLayer> KonfytPatch::getPluginLayerList() const
{
    QList<KonfytPatchLayer> l;
    for (int i=0; i < this->layerList.count(); i++) {
        if ( this->layerList.at(i).getLayerType() == KonfytLayerType_Sfz ) {
            l.append(this->layerList.at(i));
        }
    }
    return l;
}




