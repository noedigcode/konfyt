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


QList<konfytPatchLayer> konfytPatch::getSfLayerList()
{
    QList<konfytPatchLayer> l;
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

    stream.writeComment("This is a patch file.");

    stream.writeStartElement("sfpatch"); // change this to just patch, here and when loading
    stream.writeAttribute("name",this->patchName);

    stream.writeTextElement("patchNote", this->getNote());

    // Step through all the layers. It's important to save them in the correct order.
    for (int i=0; i<this->layerList.count(); i++) {

        konfytPatchLayer g = this->layerList.at(i);
        if (g.getLayerType() == KonfytLayerType_SoundfontProgram) {

            // --------------------------------------------

            layerSoundfontStruct sfLayer = g.sfData;

            stream.writeStartElement("sfLayer");
            // Layer properties
            konfytSoundfontProgram p = sfLayer.program;
            stream.writeTextElement("soundfont_filename", p.parent_soundfont);
            stream.writeTextElement("bank", n2s(p.bank));
            stream.writeTextElement("program", n2s(p.program));
            stream.writeTextElement("name", p.name);
            stream.writeTextElement("gain", n2s( sfLayer.gain ));
            stream.writeTextElement("bus", n2s(g.busIdInProject));
            stream.writeTextElement("solo", bool_to_10_string(sfLayer.solo));
            stream.writeTextElement("mute", bool_to_10_string(sfLayer.mute));

            // Midi filter
            konfytMidiFilter f = sfLayer.filter;
            writeMidiFilterToXMLStream(&stream, f);

            stream.writeEndElement(); // sfLayer

            // --------------------------------------------

        } else if (g.getLayerType() == KonfytLayerType_CarlaPlugin) {

            // ----------------------------------------------------------

            layerCarlaPluginStruct p = g.carlaPluginData;

            stream.writeStartElement("sfzLayer");
            stream.writeTextElement("name", p.name);
            stream.writeTextElement("path", p.path);
            stream.writeTextElement("gain", n2s(p.gain));
            stream.writeTextElement("bus", n2s(g.busIdInProject) );
            stream.writeTextElement("solo", bool_to_10_string(p.solo));
            stream.writeTextElement("mute", bool_to_10_string(p.mute));

            // Midi filter
            konfytMidiFilter f = p.midiFilter;
            writeMidiFilterToXMLStream(&stream, f);

            stream.writeEndElement();

            // ----------------------------------------------------------

        } else if (g.getLayerType() == KonfytLayerType_MidiOut) {

            // --------------------------------------------------

            layerMidiOutStruct m = g.midiOutputPortData;

            stream.writeStartElement("midiOutputPortLayer");
            stream.writeTextElement("port", n2s( m.portIdInProject ));
            stream.writeTextElement("solo", bool_to_10_string(m.solo));
            stream.writeTextElement("mute", bool_to_10_string(m.mute));

            // Midi filter
            konfytMidiFilter f = m.filter;
            writeMidiFilterToXMLStream(&stream, f);

            stream.writeEndElement();

            // --------------------------------------------------

        } else if (g.getLayerType() == KonfytLayerType_AudioIn) {

            layerAudioInStruct a = g.audioInPortData;

            stream.writeStartElement("audioInPortLayer");

            stream.writeTextElement("name", a.name);
            stream.writeTextElement("port", n2s(a.portIdInProject));
            stream.writeTextElement("gain", n2s(a.gain));
            stream.writeTextElement("bus", n2s(g.busIdInProject));
            stream.writeTextElement("solo", bool_to_10_string(a.solo));
            stream.writeTextElement("mute", bool_to_10_string(a.mute));

            stream.writeEndElement();
        }

    }

    stream.writeEndElement(); // patch

    stream.writeEndDocument();

    file.close();
    return true;
}

void konfytPatch::writeMidiFilterToXMLStream(QXmlStreamWriter *stream, konfytMidiFilter f)
{
    stream->writeStartElement("midiFilter");

    // Note / velocity zone
    konfytMidiFilterZone z = f.zone;
    stream->writeStartElement("zone");
    stream->writeTextElement("lowNote", n2s(z.lowNote));
    stream->writeTextElement("highNote", n2s(z.highNote));
    stream->writeTextElement("multiply", n2s(z.multiply));
    stream->writeTextElement("add", n2s(z.add));
    stream->writeTextElement("lowVel", n2s(z.lowVel));
    stream->writeTextElement("highVel", n2s(z.highVel));
    stream->writeEndElement();

    // passAllCC
    QString tempBool;
    if (f.passAllCC) { tempBool = "1"; } else { tempBool = "0"; }
    stream->writeTextElement("passAllCC", tempBool);

    // passPitchbend
    if (f.passPitchbend) { tempBool = "1"; } else { tempBool = "0"; }
    stream->writeTextElement("passPitchbend", tempBool);

    // passProg
    if (f.passProg) { tempBool = "1"; } else { tempBool = "0"; }
    stream->writeTextElement("passProg", tempBool);

    // CC list
    for (int i=0; i<f.passCC.count(); i++) {
        stream->writeTextElement("cc", n2s(f.passCC.at(i)));
    }

    // Input/output channels
    stream->writeTextElement("inChan", n2s(f.inChan));
    stream->writeTextElement("outChan", n2s(f.outChan));

    stream->writeEndElement(); // midiFilter
}

konfytMidiFilter konfytPatch::readMidiFilterFromXMLStream(QXmlStreamReader* r)
{
    konfytMidiFilter f;
    f.passCC.clear();

    while (r->readNextStartElement()) { // Filter properties
        if (r->name() == "zone") {
            konfytMidiFilterZone z;
            while (r->readNextStartElement()) { // zone properties
                if (r->name() == "lowNote") {
                    z.lowNote = r->readElementText().toInt();
                } else if (r->name() == "highNote") {
                    z.highNote = r->readElementText().toInt();
                } else if (r->name() == "multiply") {
                    z.multiply = r->readElementText().toInt();
                } else if (r->name() == "add") {
                    z.add = r->readElementText().toInt();
                } else if (r->name() ==  "lowVel") {
                    z.lowVel = r->readElementText().toInt();
                } else if (r->name() == "highVel") {
                    z.highVel = r->readElementText().toInt();
                } else {
                    r->skipCurrentElement();
                }
            } // end zone while
            f.setZone(z);
        } else if (r->name() == "passAllCC") {
            f.passAllCC = (r->readElementText() == "1");
        } else if (r->name() == "passPitchbend") {
            f.passPitchbend = (r->readElementText() == "1");
        } else if (r->name() == "passProg") {
            f.passProg = (r->readElementText() == "1");
        } else if (r->name() == "cc") {
            f.passCC.append(r->readElementText().toInt());
        } else if (r->name() == "inChan") {
            f.inChan = r->readElementText().toInt();
        } else if (r->name() == "outChan") {
            f.outChan = r->readElementText().toInt();
        } else {
            r->skipCurrentElement();
        }
    } // end filter while

    return f;
}

// Loads a pach from an patch xml file.
bool konfytPatch::loadPatchFromFile(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // error message!
        return false;
    }

    // Midi filter variables
    int outport_port;
    bool port_solo;
    bool port_mute;

    // SFZ Layer variables
    QString sfz_name;
    QString sfz_path;
    float sfz_gain;
    bool sfz_solo;
    bool sfz_mute;


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

            if (r.name() == "patchNote") {

                this->setNote(r.readElementText());

            } else if (r.name() == "sfLayer") { // soundfont layer

                layerSoundfontStruct sfLayer = layerSoundfontStruct();
                int bus = 0;

                while (r.readNextStartElement()) { // layer properties

                    if (r.name() == "soundfont_filename") {
                        sfLayer.program.parent_soundfont = r.readElementText();
                    } else if (r.name() == "bank") {
                        sfLayer.program.bank = r.readElementText().toInt();
                    } else if (r.name() == "program") {
                        sfLayer.program.program = r.readElementText().toInt();
                    } else if (r.name() == "name") {
                        sfLayer.program.name = r.readElementText();
                    } else if (r.name() == "gain") {
                        sfLayer.gain = r.readElementText().toFloat();
                    } else if (r.name() == "solo") {
                        sfLayer.solo = (r.readElementText() == "1");
                    } else if (r.name() == "mute") {
                        sfLayer.mute = (r.readElementText() == "1");
                    } else if (r.name() == "midiFilter") {
                        sfLayer.filter = readMidiFilterFromXMLStream(&r);
                    } else if (r.name() == "bus") {
                        bus = r.readElementText().toInt();
                    } else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized sfLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }

                }

                konfytPatchLayer newLayer = this->addSfLayer(sfLayer);
                this->setLayerBus(&newLayer, bus);


            } else if (r.name() == "sfzLayer") { // SFZ Layer

                layerCarlaPluginStruct p = layerCarlaPluginStruct();
                int bus = 0;

                while (r.readNextStartElement()) {

                    if (r.name() == "name") {
                        p.name = r.readElementText();
                    } else if (r.name() == "path") {
                        p.path = r.readElementText();
                    } else if (r.name() == "gain") {
                        p.gain = r.readElementText().toFloat();
                    } else if (r.name() == "bus") {
                        bus = r.readElementText().toInt();
                    } else if (r.name() == "solo") {
                        p.solo = (r.readElementText() == "1");
                    } else if (r.name() == "mute") {
                        p.mute = (r.readElementText() == "1");
                    } else if (r.name() == "midiFilter") {
                        p.midiFilter = readMidiFilterFromXMLStream(&r);
                    } else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized sfzLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }

                }

                p.pluginType = KonfytCarlaPluginType_SFZ;

                konfytPatchLayer newLayer = this->addPlugin(p);
                this->setLayerBus(&newLayer, bus);


            } else if (r.name() == "midiOutputPortLayer") {

                konfytMidiFilter f;
                layerMidiOutStruct mp;

                while (r.readNextStartElement()) { // port
                    if (r.name() == "port") {
                        mp.portIdInProject = r.readElementText().toInt();
                    } else if (r.name() == "solo") {
                        mp.solo = (r.readElementText() == "1");
                    } else if (r.name() == "mute") {
                        mp.mute = (r.readElementText() == "1");
                    } else if (r.name() == "midiFilter") {
                        mp.filter = readMidiFilterFromXMLStream(&r);
                    } else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized midiOutputPortLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

                // Add new midi port
                this->addMidiOutputPort(mp);

            } else if (r.name() == "audioInPortLayer") {

                layerAudioInStruct a;
                int bus = 0;

                while (r.readNextStartElement()) {
                    if (r.name() == "name") { a.name = r.readElementText(); }
                    else if (r.name() == "port") { a.portIdInProject = r.readElementText().toInt(); }
                    else if (r.name() == "gain") { a.gain = r.readElementText().toFloat(); }
                    else if (r.name() == "bus") { bus = r.readElementText().toInt(); }
                    else if (r.name() == "solo") { a.solo = (r.readElementText() == "1"); }
                    else if (r.name() == "mute") { a.mute = (r.readElementText() == "1"); }
                    else {
                        userMessage("konfytPatch::loadPatchFromFile: "
                                    "Unrecognized audioInPortLayer element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

                konfytPatchLayer newLayer = this->addAudioInPort(a);
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

void konfytPatch::removeLayer(konfytPatchLayer* layer)
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

// Replace a layer. Layer is identified using unique patch_id in layer.
void konfytPatch::replaceLayer(konfytPatchLayer newLayer)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == newLayer.ID_in_patch) {
            this->layerList.replace(i, newLayer);
            return;
        }
    }

    // No layer was found and replaced. This is probably a logic error somewhere.
    error_abort("replaceLayer: Layer with patch_id " + n2s(newLayer.ID_in_patch) + " is not in the patch's layerList.");
}

// Set the midi filter for the layer for which the patch_id matches that of
// the specified layer item.
void konfytPatch::setLayerFilter(konfytPatchLayer *layer, konfytMidiFilter newFilter)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == layer->ID_in_patch) {
            konfytPatchLayer g = this->layerList.at(i);
            g.setMidiFilter(newFilter);
            this->layerList.replace(i, g);
            return;
        }
    }

    // No layer was found. This is probably a logic error somewhere.
    error_abort("setLayerFilter: Layer with patch_id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
}

float konfytPatch::getLayerGain(konfytPatchLayer *layer)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == layer->ID_in_patch) {
            return this->layerList.at(i).getGain();
        }
    }

    // No layer was found that matches the ID. This is probably a logic error somewhere.
    error_abort("getLayerGain: Layer with patch_id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
}

void konfytPatch::setLayerGain(konfytPatchLayer *layer, float newGain)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == layer->ID_in_patch) {
            konfytPatchLayer g = layerList.at(i);
            g.setGain(newGain);
            this->replaceLayer(g);
            return;
        }
    }

    // No layer was found that matches the ID. This is probably a logic error somewhere.
    error_abort("setLayerGain: Layer with patch_id " + n2s(layer->ID_in_patch) +  " is not in the patch's layerList.");
}

void konfytPatch::setLayerSolo(konfytPatchLayer *layer, bool solo)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == layer->ID_in_patch) {
            konfytPatchLayer g = layerList.at(i);

            g.setSolo(solo);

            this->replaceLayer(g);
            return;
        }
    }

    // No layer was found that matches the ID. This is probably a logic error somewhere.
    error_abort("setLayerSolo: Layer with patch_id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
}

void konfytPatch::setLayerMute(konfytPatchLayer *layer, bool mute)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == layer->ID_in_patch) {
            konfytPatchLayer g = layerList.at(i);

            g.setMute(mute);

            this->replaceLayer(g);
            return;
        }
    }

    // No layer was found that matches the ID. This is probably a logic error somewhere.
    error_abort("setLayerMute: Layer with patch_id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
}

void konfytPatch::setLayerBus(konfytPatchLayer *layer, int bus)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == layer->ID_in_patch) {
            konfytPatchLayer g = layerList.at(i);

            if ( (g.getLayerType() == KonfytLayerType_SoundfontProgram)
                 || (g.getLayerType() == KonfytLayerType_AudioIn)
                 || (g.getLayerType() == KonfytLayerType_CarlaPlugin) ) {

                g.busIdInProject = bus;

            } else {
                error_abort("setLayerBus: invalid LayerType");
            }

            this->replaceLayer(g);
            return;
        }
    }

    // No layer was found that matches the ID. This is probably a logic error somewhere.
    error_abort("setLayerMute: Layer with patch_id " + n2s(layer->ID_in_patch) + " is not in the patch's layerList.");
}

konfytPatchLayer konfytPatch::addProgram(konfytSoundfontProgram p)
{
    // Set up new soundfont program layer structure
    layerSoundfontStruct l = layerSoundfontStruct();
    l.program = p;
    l.gain = DEFAULT_GAIN_FOR_NEW_LAYER; // default gain
    l.solo = false;
    l.mute = false;

    // Set up a new layer item
    // (a layer item is initialised with an unique id which will
    //  allow us to identify it later on.)
    konfytPatchLayer g;
    g.initLayer(this->id_counter++, l);

    // Add to layer list
    layerList.append(g);

    return g;
}

konfytPatchLayer konfytPatch::addSfLayer(layerSoundfontStruct newSfLayer)
{
    // Set up a new layer item
    // (a layer item is initialised with an unique id which will
    //  allow us to identify it later on.)
    konfytPatchLayer g;
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
layerSoundfontStruct konfytPatch::getSfLayer(int id_in_engine)
{
    return this->getSfLayer_LayerItem(id_in_engine).sfData;
}

konfytPatchLayer konfytPatch::getSfLayer_LayerItem(int id_in_engine)
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

    konfytPatchLayer g;
    return g;
}



float konfytPatch::getSfLayerGain(int id_in_engine)
{
    return this->getSfLayer(id_in_engine).gain;
}

void konfytPatch::setSfLayerGain(int id_in_engine, float newGain)
{
    konfytPatchLayer g = this->getSfLayer_LayerItem(id_in_engine);
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

QList<konfytPatchLayer> konfytPatch::getLayerItems()
{
    return this->layerList;
}

// Returns the layeritem in the patch that matches the unique ID
// of the layerItem specified.
konfytPatchLayer konfytPatch::getLayerItem(konfytPatchLayer item)
{
    // Find layer that matches patch_id
    for (int i=0; i < this->layerList.count(); i++) {
        if (this->layerList.at(i).ID_in_patch == item.ID_in_patch) {
            return this->layerList.at(i);
        }
    }

    // No layer was found that matches the ID. This is probably a logic error somewhere.
    error_abort("getLayerItem: LayerItem with patch_id " + n2s(item.ID_in_patch) + " is not in the patch's layerList.");
}

// Return list of port ids of the patch's midi output ports.
QList<int> konfytPatch::getMidiOutputPortList_ids()
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
QList<int> konfytPatch::getAudioInPortList_ids()
{
    QList<int> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_AudioIn) {
            l.append(layerList.at(i).audioInPortData.portIdInProject);
        }
    }
    return l;
}

QList<layerAudioInStruct> konfytPatch::getAudioInPortList_struct()
{
    QList<layerAudioInStruct> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_AudioIn) {
            l.append(layerList.at(i).audioInPortData);
        }
    }
    return l;
}

// Return list of midiOutputPortStruct's of the patch's midi output ports.
QList<layerMidiOutStruct> konfytPatch::getMidiOutputPortList_struct()
{
    QList<layerMidiOutStruct> l;
    for (int i=0; i<this->layerList.count(); i++) {
        if (layerList.at(i).getLayerType() == KonfytLayerType_MidiOut) {
            l.append(layerList.at(i).midiOutputPortData);
        }
    }
    return l;
}



konfytPatchLayer konfytPatch::addAudioInPort(int newPort)
{
    // Set up a default new audio input port with the specified port number
    layerAudioInStruct a = layerAudioInStruct();
    a.name = "Audio Input Port " + n2s(newPort);
    a.portIdInProject = newPort;
    a.gain = DEFAULT_GAIN_FOR_NEW_LAYER;
    a.mute = false;
    a.solo = false;

    return addAudioInPort(a);
}

konfytPatchLayer konfytPatch::addAudioInPort(layerAudioInStruct newPort)
{
    konfytPatchLayer g;

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
konfytPatchLayer konfytPatch::addMidiOutputPort(int newPort)
{
    layerMidiOutStruct m = layerMidiOutStruct();
    m.portIdInProject = newPort;

    return addMidiOutputPort(m);
}

konfytPatchLayer konfytPatch::addMidiOutputPort(layerMidiOutStruct newPort)
{
    konfytPatchLayer g;

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


konfytPatchLayer konfytPatch::addPlugin(layerCarlaPluginStruct newPlugin)
{
    // Create and initialise new layeritem.
    // Item is initialised with a unique ID, which will later be used to
    // uniquely identify it.
    konfytPatchLayer g;
    g.initLayer(this->id_counter++, newPlugin);

    // Add to layer list
    layerList.append(g);

    return g;
}


layerCarlaPluginStruct konfytPatch::getPlugin(int index_in_engine)
{
    return this->getPlugin_LayerItem(index_in_engine).carlaPluginData;
}

konfytPatchLayer konfytPatch::getPlugin_LayerItem(int index_in_engine)
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

    konfytPatchLayer g;
    return g;
}

int konfytPatch::getPluginCount()
{
    return this->getPluginLayerList().count();
}


void konfytPatch::setPluginGain(int index_in_engine, float newGain)
{
    konfytPatchLayer g = this->getPlugin_LayerItem(index_in_engine);
    g.setGain(newGain);
    this->replaceLayer(g);
}

float konfytPatch::getPluginGain(int index_in_engine)
{
    return this->getPlugin(index_in_engine).gain;
}

QList<konfytPatchLayer> konfytPatch::getPluginLayerList()
{
    QList<konfytPatchLayer> l;
    for (int i=0; i < this->layerList.count(); i++) {
        if ( this->layerList.at(i).getLayerType() == KonfytLayerType_CarlaPlugin ) {
            l.append(this->layerList.at(i));
        }
    }
    return l;
}




