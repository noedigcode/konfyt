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

#include "konfytDefines.h"

#include <iostream>


void KonfytPatch::error_abort(QString msg)
{
    std::cout << "\n\n" << "Konfyt ERROR, ABORTING: konfytPatch:"
              << msg.toLocal8Bit().constData() << "\n\n";
    abort();
}

QList<KfPatchLayerWeakPtr> KonfytPatch::getSfLayerList() const
{
    return layersOfType(KonfytPatchLayer::TypeSoundfontProgram);
}

/* Saves the patch to a XML patch file. */
bool KonfytPatch::savePatchToFile(QString filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // error message.
        return false;
    }

    QXmlStreamWriter stream(&file);
    writeToXmlStream(&stream);

    file.close();
    return true;
}

/* Loads a patch from a patch XML file. If errors string pointer is specified,
 * any parsing errors are appended to it. */
bool KonfytPatch::loadPatchFromFile(QString filename, QString *errors)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendError(errors, "Failed to open file for reading.");
        return false;
    }

    QXmlStreamReader r(&file);
    readFromXmlStream(&r, errors);

    file.close();
    return true;
}

QByteArray KonfytPatch::toByteArray()
{
    QByteArray data;
    QXmlStreamWriter stream(&data);
    writeToXmlStream(&stream);
    return data;
}

void KonfytPatch::fromByteArray(QByteArray data)
{
    QXmlStreamReader r(data);
    readFromXmlStream(&r);
}

QList<KfPatchLayerWeakPtr> KonfytPatch::layersOfType(KonfytPatchLayer::LayerType layerType) const
{
    QList<KfPatchLayerWeakPtr> l;
    foreach (KfPatchLayerSharedPtr layer, layerList) {
        if (layer->layerType() == layerType) {
            l.append(layer.toWeakRef());
        }
    }

    return l;
}

void KonfytPatch::writeToXmlStream(QXmlStreamWriter *stream) const
{
    stream->setAutoFormatting(true);
    stream->writeStartDocument();

    stream->writeComment("This is a Konfyt patch file.");

    stream->writeStartElement(XML_PATCH); // change this to just patch, here and when loading
    stream->writeAttribute(XML_PATCH_NAME,this->patchName);

    stream->writeTextElement(XML_PATCH_NOTE, this->note());
    stream->writeTextElement(XML_PATCH_ALWAYSACTIVE, QVariant(alwaysActive).toString());

    // Step through all the layers. It's important to save them in the correct order.
    foreach (KfPatchLayerSharedPtr layer, layerList) {

        if (layer->layerType() == KonfytPatchLayer::TypeSoundfontProgram) {

            // --------------------------------------------

            LayerSoundfontData sfdata = layer->soundfontData;

            stream->writeStartElement(XML_PATCH_SFLAYER);
            // Layer properties
            KonfytSoundfontProgram p = sfdata.program;
            stream->writeTextElement(XML_PATCH_SFLAYER_FILENAME, p.parent_soundfont);
            stream->writeTextElement(XML_PATCH_SFLAYER_BANK, n2s(p.bank));
            stream->writeTextElement(XML_PATCH_SFLAYER_PROGRAM, n2s(p.program));
            stream->writeTextElement(XML_PATCH_SFLAYER_NAME, p.name);
            stream->writeTextElement(XML_PATCH_SFLAYER_GAIN, n2s(layer->gain()));
            stream->writeTextElement(XML_PATCH_SFLAYER_BUS, n2s(layer->busIdInProject()));
            stream->writeTextElement(XML_PATCH_SFLAYER_SOLO, bool2str(layer->isSolo()));
            stream->writeTextElement(XML_PATCH_SFLAYER_MUTE, bool2str(layer->isMute()));
            stream->writeTextElement(XML_PATCH_SFLAYER_MIDI_IN, n2s(layer->midiInPortIdInProject()));

            // Midi filter
            layer->midiFilter().writeToXMLStream(stream);

            stream->writeEndElement(); // sfLayer

            // --------------------------------------------

        } else if (layer->layerType() == KonfytPatchLayer::TypeSfz) {

            // ----------------------------------------------------------

            LayerSfzData sfzdata = layer->sfzData;

            stream->writeStartElement(XML_PATCH_SFZLAYER);
            stream->writeTextElement(XML_PATCH_SFZLAYER_NAME, layer->name());
            stream->writeTextElement(XML_PATCH_SFZLAYER_PATH, sfzdata.path);
            stream->writeTextElement(XML_PATCH_SFZLAYER_GAIN, n2s(layer->gain()));
            stream->writeTextElement(XML_PATCH_SFZLAYER_BUS, n2s(layer->busIdInProject()) );
            stream->writeTextElement(XML_PATCH_SFZLAYER_SOLO, bool2str(layer->isSolo()));
            stream->writeTextElement(XML_PATCH_SFZLAYER_MUTE, bool2str(layer->isMute()));
            stream->writeTextElement(XML_PATCH_SFZLAYER_MIDI_IN, n2s(layer->midiInPortIdInProject()));

            // Midi filter
            layer->midiFilter().writeToXMLStream(stream);

            stream->writeEndElement();

            // ----------------------------------------------------------

        } else if (layer->layerType() == KonfytPatchLayer::TypeMidiOut) {

            // --------------------------------------------------

            LayerMidiOutData m = layer->midiOutputPortData;

            stream->writeStartElement(XML_PATCH_MIDIOUT);
            stream->writeTextElement(XML_PATCH_MIDIOUT_PORT, n2s( m.portIdInProject ));
            stream->writeTextElement(XML_PATCH_MIDIOUT_SOLO, bool2str(layer->isSolo()));
            stream->writeTextElement(XML_PATCH_MIDIOUT_MUTE, bool2str(layer->isMute()));
            stream->writeTextElement(XML_PATCH_MIDIOUT_MIDI_IN, n2s(layer->midiInPortIdInProject()));

            // Midi filter
            layer->midiFilter().writeToXMLStream(stream);

            // MIDI Send list
            if (layer->midiSendList.count()) {
                stream->writeStartElement(XML_PATCH_MIDISENDLIST);

                foreach (MidiSendItem item, layer->midiSendList) {
                    item.writeToXMLStream(stream);
                }

                stream->writeEndElement();
            }

            stream->writeEndElement();

            // --------------------------------------------------

        } else if (layer->layerType() == KonfytPatchLayer::TypeAudioIn) {

            LayerAudioInData a = layer->audioInPortData;

            stream->writeStartElement(XML_PATCH_AUDIOIN);

            stream->writeTextElement(XML_PATCH_AUDIOIN_NAME, layer->name());
            stream->writeTextElement(XML_PATCH_AUDIOIN_PORT, n2s(a.portIdInProject));
            stream->writeTextElement(XML_PATCH_AUDIOIN_GAIN, n2s(layer->gain()));
            stream->writeTextElement(XML_PATCH_AUDIOIN_BUS, n2s(layer->busIdInProject()));
            stream->writeTextElement(XML_PATCH_AUDIOIN_SOLO, bool2str(layer->isSolo()));
            stream->writeTextElement(XML_PATCH_AUDIOIN_MUTE, bool2str(layer->isMute()));

            stream->writeEndElement();
        }

    }

    stream->writeEndElement(); // patch

    stream->writeEndDocument();
}

void KonfytPatch::readFromXmlStream(QXmlStreamReader *r, QString *errors)
{
    r->setNamespaceProcessing(false);

    this->clearLayers();

    while (r->readNextStartElement()) { // patch

        // Get the patch name attribute
        QXmlStreamAttributes ats =  r->attributes();
        if (ats.count()) {
            this->patchName = ats.at(0).value().toString();
        }

        while (r->readNextStartElement()) {

            if (r->name() == XML_PATCH_NOTE) {

                this->setNote(r->readElementText());

            } else if (r->name() == XML_PATCH_ALWAYSACTIVE) {

                this->alwaysActive = QVariant(r->readElementText()).toBool();

            } else if (r->name() == XML_PATCH_SFLAYER) { // soundfont layer

                LayerSoundfontData sfLayer = LayerSoundfontData();
                int bus = 0;
                int midiIn = 0;
                float gain = 1.0;
                bool solo = false;
                bool mute = false;
                KonfytMidiFilter midiFilter;

                while (r->readNextStartElement()) { // layer properties

                    if (r->name() == XML_PATCH_SFLAYER_FILENAME) {
                        sfLayer.program.parent_soundfont = r->readElementText();
                    } else if (r->name() == XML_PATCH_SFLAYER_BANK) {
                        sfLayer.program.bank = r->readElementText().toInt();
                    } else if (r->name() == XML_PATCH_SFLAYER_PROGRAM) {
                        sfLayer.program.program = r->readElementText().toInt();
                    } else if (r->name() == XML_PATCH_SFLAYER_NAME) {
                        sfLayer.program.name = r->readElementText();
                    } else if (r->name() == XML_PATCH_SFLAYER_GAIN) {
                        gain = r->readElementText().toFloat();
                    } else if (r->name() == XML_PATCH_SFLAYER_SOLO) {
                        solo = (r->readElementText() == "1");
                    } else if (r->name() == XML_PATCH_SFLAYER_MUTE) {
                        mute = (r->readElementText() == "1");
                    } else if (r->name() == XML_MIDIFILTER) {
                        midiFilter.readFromXMLStream(r);
                    } else if (r->name() == XML_PATCH_SFLAYER_BUS) {
                        bus = r->readElementText().toInt();
                    } else if (r->name() == XML_PATCH_SFLAYER_MIDI_IN) {
                        midiIn = r->readElementText().toInt();
                    } else {
                        appendError(errors, "Unrecognized sfLayer element: " + r->name().toString() );
                        r->skipCurrentElement();
                    }

                }

                KfPatchLayerSharedPtr layer = this->addSfLayer(sfLayer);
                layer->setBusIdInProject(bus);
                layer->setMidiInPortIdInProject(midiIn);
                layer->setGain(gain);
                layer->setSolo(solo);
                layer->setMute(mute);
                layer->setMidiFilter(midiFilter);


            } else if (r->name() == XML_PATCH_SFZLAYER) { // SFZ Layer

                LayerSfzData p = LayerSfzData();
                int bus = 0;
                int midiIn = 0;
                QString name;
                float gain = 1.0;
                bool solo = false;
                bool mute = false;
                KonfytMidiFilter midiFilter;

                while (r->readNextStartElement()) {

                    if (r->name() == XML_PATCH_SFZLAYER_NAME) {
                        name = r->readElementText();
                    } else if (r->name() == XML_PATCH_SFZLAYER_PATH) {
                        p.path = r->readElementText();
                    } else if (r->name() == XML_PATCH_SFZLAYER_GAIN) {
                        gain = r->readElementText().toFloat();
                    } else if (r->name() == XML_PATCH_SFZLAYER_BUS) {
                        bus = r->readElementText().toInt();
                    } else if (r->name() == XML_PATCH_SFZLAYER_MIDI_IN) {
                        midiIn = r->readElementText().toInt();
                    } else if (r->name() == XML_PATCH_SFZLAYER_SOLO) {
                        solo = (r->readElementText() == "1");
                    } else if (r->name() == XML_PATCH_SFZLAYER_MUTE) {
                        mute = (r->readElementText() == "1");
                    } else if (r->name() == XML_MIDIFILTER) {
                        midiFilter.readFromXMLStream(r);
                    } else {
                        appendError(errors, "Unrecognized sfzLayer element: " + r->name().toString() );
                        r->skipCurrentElement();
                    }

                }

                KfPatchLayerSharedPtr layer = this->addPlugin(p, name);
                layer->setBusIdInProject(bus);
                layer->setMidiInPortIdInProject(midiIn);
                layer->setGain(gain);
                layer->setSolo(solo);
                layer->setMute(mute);
                layer->setMidiFilter(midiFilter);


            } else if (r->name() == XML_PATCH_MIDIOUT) {

                LayerMidiOutData mp;
                int midiIn = 0;
                QList<MidiSendItem> midiSendItems;
                bool solo = false;
                bool mute = false;
                KonfytMidiFilter midiFilter;

                while (r->readNextStartElement()) { // port
                    if (r->name() == XML_PATCH_MIDIOUT_PORT) {
                        mp.portIdInProject = r->readElementText().toInt();
                    } else if (r->name() == XML_PATCH_MIDIOUT_SOLO) {
                        solo = (r->readElementText() == "1");
                    } else if (r->name() == XML_PATCH_MIDIOUT_MUTE) {
                        mute = (r->readElementText() == "1");
                    } else if (r->name() == XML_MIDIFILTER) {
                        midiFilter.readFromXMLStream(r);
                    } else if (r->name() == XML_PATCH_MIDIOUT_MIDI_IN) {
                        midiIn = r->readElementText().toInt();
                    } else if (r->name() == XML_PATCH_MIDISENDLIST) {
                        while (r->readNextStartElement()) {
                            if (r->name() == XML_MIDI_SEND_ITEM) {
                                MidiSendItem item;
                                QString error = item.readFromXmlStream(r);
                                if (!error.isEmpty()) {
                                    appendError(errors, "MidiSendItem read error: " + error);
                                }
                                midiSendItems.append(item);
                            } else {
                                appendError(errors, "Unrecognized MIDI send list element: " + r->name().toString());
                                r->skipCurrentElement();
                            }
                        }
                    } else {
                        appendError(errors, "Unrecognized midiOutputPortLayer element: " + r->name().toString() );
                        r->skipCurrentElement();
                    }
                }

                // Add new midi port
                KfPatchLayerSharedPtr layer = this->addMidiOutputPort(mp);
                layer->setMidiInPortIdInProject(midiIn);
                layer->midiSendList = midiSendItems;
                layer->setSolo(solo);
                layer->setMute(mute);
                layer->setMidiFilter(midiFilter);

            } else if (r->name() == XML_PATCH_AUDIOIN) {

                LayerAudioInData a;
                int bus = 0;
                QString name;
                float gain = 1.0;
                bool solo = false;
                bool mute = false;

                while (r->readNextStartElement()) {
                    if (r->name() == XML_PATCH_AUDIOIN_NAME) { name = r->readElementText(); }
                    else if (r->name() == XML_PATCH_AUDIOIN_PORT) { a.portIdInProject = r->readElementText().toInt(); }
                    else if (r->name() == XML_PATCH_AUDIOIN_GAIN) { gain = r->readElementText().toFloat(); }
                    else if (r->name() == XML_PATCH_AUDIOIN_BUS) { bus = r->readElementText().toInt(); }
                    else if (r->name() == XML_PATCH_AUDIOIN_SOLO) { solo = (r->readElementText() == "1"); }
                    else if (r->name() == XML_PATCH_AUDIOIN_MUTE) { mute = (r->readElementText() == "1"); }
                    else {
                        appendError(errors,
                                    "Unrecognized audioInPortLayer element: " + r->name().toString() );
                        r->skipCurrentElement();
                    }
                }

                KfPatchLayerSharedPtr layer = this->addAudioInPort(a, name);
                layer->setBusIdInProject(bus);
                layer->setGain(gain);
                layer->setSolo(solo);
                layer->setMute(mute);

            } else {
                appendError(errors, "Unrecognized layer type: " + r->name().toString() );
                r->skipCurrentElement(); // not any of the layer types
            }
        }
    }
}

void KonfytPatch::appendError(QString *errorString, QString msg)
{
    if (errorString) {
        if (!errorString->isEmpty()) { errorString->append("\n"); }
        errorString->append(msg);
    }
}

/* Returns true if the layer index represents a valid layer.
 * This includes all types of layers. */
bool KonfytPatch::isValidLayerIndex(int layerIndex) const
{
    return ( (layerIndex >= 0) && (layerIndex < layerList.count()) );
}

void KonfytPatch::removeLayer(KfPatchLayerWeakPtr layer)
{
    layerList.removeAll(layer.toStrongRef());
}

/* Removes all the layers in the patch. */
void KonfytPatch::clearLayers()
{
    while (layerList.count()) {
        removeLayer(layerList.first());
    }
}

/* Returns the index of the layer in the patch, -1 if not found. */
int KonfytPatch::layerIndex(KfPatchLayerWeakPtr layer) const
{
    return layerList.indexOf(layer.toStrongRef());
}

KfPatchLayerWeakPtr KonfytPatch::addProgram(KonfytSoundfontProgram p)
{
    // Set up new soundfont program layer structure
    LayerSoundfontData sfData;
    sfData.program = p;

    // Set up a new layer item
    KfPatchLayerSharedPtr layer(new KonfytPatchLayer());
    layer->initLayer(sfData);

    layerList.append(layer);

    return layer.toWeakRef();
}

KfPatchLayerWeakPtr KonfytPatch::addSfLayer(LayerSoundfontData newSfLayer)
{
    KfPatchLayerSharedPtr layer(new KonfytPatchLayer());
    layer->initLayer(newSfLayer);

    // add to layer list
    layerList.append(layer);

    return layer.toWeakRef();
}

int KonfytPatch::layerCount() const
{
    return this->layerList.count();
}

void KonfytPatch::setName(QString newName)
{
    this->patchName = newName;
}

QString KonfytPatch::name() const
{
    return this->patchName;
}

void KonfytPatch::setNote(QString newNote)
{
    this->patchNote = newNote;
}

QString KonfytPatch::note() const
{
    return this->patchNote;
}

QList<KfPatchLayerWeakPtr> KonfytPatch::layers()
{
    // Create a list of weak ptrs from shared ptrs list
    QList<KfPatchLayerWeakPtr> l;
    foreach (KfPatchLayerWeakPtr layer, layerList) {
        l.append(layer);
    }
    return l;
}

KfPatchLayerWeakPtr KonfytPatch::layer(int index)
{
    QSharedPointer<KonfytPatchLayer> layer = layerList.at(index);
    return layer.toWeakRef();
}

/* Return list of port project ids of the patch's midi output ports. */
QList<int> KonfytPatch::getMidiOutputPortListProjectIds() const
{
    QList<KfPatchLayerWeakPtr> layers = layersOfType(KonfytPatchLayer::TypeMidiOut);
    QList<int> l;
    foreach (KfPatchLayerWeakPtr layer, layers) {
        l.append(layer.toStrongRef()->midiOutputPortData.portIdInProject);
    }
    return l;
}

/* Return list of port ids of patch's audio input ports. */
QList<int> KonfytPatch::getAudioInPortListProjectIds() const
{
    QList<KfPatchLayerWeakPtr> layers = layersOfType(KonfytPatchLayer::TypeAudioIn);
    QList<int> l;
    foreach (KfPatchLayerWeakPtr layer, layers) {
        l.append(layer.toStrongRef()->audioInPortData.portIdInProject);
    }
    return l;
}

QList<KfPatchLayerWeakPtr> KonfytPatch::getAudioInLayerList() const
{
    return layersOfType(KonfytPatchLayer::TypeAudioIn);
}

QList<KfPatchLayerWeakPtr> KonfytPatch::getMidiOutputLayerList() const
{
    return layersOfType(KonfytPatchLayer::TypeMidiOut);
}

KfPatchLayerWeakPtr KonfytPatch::addAudioInPort(int newPort)
{
    // Set up a default new audio input port with the specified port number
    LayerAudioInData a;
    a.portIdInProject = newPort;
    QString name = "Audio Input Port " + n2s(newPort);

    return addAudioInPort(a, name);
}

KfPatchLayerWeakPtr KonfytPatch::addAudioInPort(LayerAudioInData newPort, QString name)
{
    KfPatchLayerSharedPtr layer;

    // Check if port not already in patch
    QList<int> audioInPortList = this->getAudioInPortListProjectIds();
    if (audioInPortList.contains(newPort.portIdInProject) == false) {
        // Set up layer item
        layer.reset(new KonfytPatchLayer());
        layer->setName(name);
        layer->initLayer(newPort);
        // Add to layer list
        layerList.append(layer);
    } else {
        // We do not handle duplicate ports well yet. Throw an error.
        error_abort("addAudioInPort: Audio input port " + n2s(newPort.portIdInProject)
                    + " already exists in patch.");
    }

    return layer.toWeakRef();
}

KfPatchLayerWeakPtr KonfytPatch::addMidiOutputPort(int newPort)
{
    LayerMidiOutData m = LayerMidiOutData();
    m.portIdInProject = newPort;

    return addMidiOutputPort(m);
}

KfPatchLayerWeakPtr KonfytPatch::addMidiOutputPort(LayerMidiOutData newPort)
{
    KfPatchLayerSharedPtr layer;

    // Check if we already contain a MIDI out port with the same project id
    QList<int> midiOutputPortList = this->getMidiOutputPortListProjectIds();
    if (midiOutputPortList.contains(newPort.portIdInProject) == false) {
        layer.reset(new KonfytPatchLayer());
        layer->initLayer(newPort);
        // Add to layer list
        layerList.append(layer);
    } else {
        // We don't know how to handle duplicate ports yet. Let's just throw an error.
        error_abort("addMidiOutputPort: Midi output port " + n2s(newPort.portIdInProject)
                    + " already exists in patch.");
    }

    return layer.toWeakRef();
}

KfPatchLayerWeakPtr KonfytPatch::addPlugin(LayerSfzData newPlugin, QString name)
{
    KfPatchLayerSharedPtr layer(new KonfytPatchLayer);
    layer->setName(name);
    layer->initLayer(newPlugin);

    layerList.append(layer);

    return layer.toWeakRef();
}

QList<KfPatchLayerWeakPtr> KonfytPatch::getPluginLayerList() const
{
    return layersOfType(KonfytPatchLayer::TypeSfz);
}

