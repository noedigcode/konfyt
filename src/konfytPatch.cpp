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

#include "konfytPatch.h"

#include "konfytUtils.h"


QList<PatchLayerPtr> Patch::getSfLayerList() const
{
    return layersOfType(PatchLayer::TypeSoundfontProgram);
}

/* Saves the patch to a XML patch file. */
bool Patch::savePatchToFile(QString filename) const
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
bool Patch::loadPatchFromFile(QString filename, QString *errors)
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

QByteArray Patch::toByteArray()
{
    QByteArray data;
    QXmlStreamWriter stream(&data);
    writeToXmlStream(&stream);
    return data;
}

void Patch::fromByteArray(QByteArray data)
{
    QXmlStreamReader r(data);
    readFromXmlStream(&r);
}

QList<PatchLayerPtr> Patch::layersOfType(
        PatchLayer::LayerType layerType) const
{
    QList<PatchLayerPtr> l;
    foreach (PatchLayerPtr layer, mLayers) {
        if (layer->layerType() == layerType) {
            l.append(layer);
        }
    }

    return l;
}

void Patch::writeToXmlStream(QXmlStreamWriter *stream) const
{
    stream->setAutoFormatting(true);
    stream->writeStartDocument();

    stream->writeComment("This is a Konfyt patch file.");

    stream->writeStartElement(XML_PATCH);
    stream->writeAttribute(XML_PATCH_NAME, this->mPatchName);

    stream->writeTextElement(XML_PATCH_NOTE, this->note());
    stream->writeTextElement(XML_PATCH_ALWAYSACTIVE, QVariant(alwaysActive).toString());
    stream->writeTextElement(XML_PATCH_RESET_OPTION, konfytResetToString(mResetOption));

    patchMidiFilter.writeToXMLStream(stream);

    // Save all layers in order.
    foreach (PatchLayerPtr layer, mLayers) {
        layer->writeToXmlStream(stream);
    }

    stream->writeEndElement(); // patch

    stream->writeEndDocument();
}

void Patch::readFromXmlStream(QXmlStreamReader *r, QString *errors)
{
    r->setNamespaceProcessing(false);

    this->clearLayers();

    while (r->readNextStartElement()) { // patch

        // Get the patch name attribute
        QXmlStreamAttributes ats =  r->attributes();
        if (ats.count()) {
            this->mPatchName = ats.at(0).value().toString();
        }

        while (r->readNextStartElement()) {

            if (r->name() == XML_PATCH_NOTE) {

                this->setNote(r->readElementText());

            } else if (r->name() == XML_PATCH_ALWAYSACTIVE) {

                this->alwaysActive = QVariant(r->readElementText()).toBool();

            } else if (r->name() == XML_PATCH_RESET_OPTION) {

                mResetOption = konfytResetFromString(r->readElementText(),
                                                     KonfytReset::Inherit);

            } else if (r->name() == MidiFilter::XML_MIDIFILTER) {

                this->patchMidiFilter.readFromXMLStream(r);

            } else if (r->name() == PatchLayer::XML_SF_LAYER) {

                xmlReadLayer(r, errors);

            } else if (r->name() == PatchLayer::XML_SFZ_LAYER) {

                xmlReadLayer(r, errors);

            } else if (r->name() == PatchLayer::XML_MIDIOUT) {

                xmlReadLayer(r, errors);

            } else if (r->name() == PatchLayer::XML_AUDIOIN) {

                xmlReadLayer(r, errors);

            } else {
                appendError(errors, "Unrecognized layer type: " + r->name().toString() );
                r->skipCurrentElement(); // not any of the layer types
            }
        }
    }
}

void Patch::xmlReadLayer(QXmlStreamReader *r, QString *errors)
{
    PatchLayerPtr layer(new PatchLayer());
    layer->readFromXmlStream(r, errors);
    mLayers.append(layer);
}

void Patch::appendError(QString *errorString, QString msg)
{
    if (errorString) {
        if (!errorString->isEmpty()) { errorString->append("\n"); }
        errorString->append(msg);
    }
}

/* Returns true if the layer index represents a valid layer.
 * This includes all types of layers. */
bool Patch::isValidLayerIndex(int layerIndex) const
{
    return ( (layerIndex >= 0) && (layerIndex < mLayers.count()) );
}

void Patch::removeLayer(PatchLayerPtr layer)
{
    mLayers.removeAll(layer);
}

void Patch::moveLayer(PatchLayerPtr layer, int newIndex)
{
    KONFYT_ASSERT_RETURN(newIndex >= 0);
    KONFYT_ASSERT_RETURN(newIndex < mLayers.count());

    int oldIndex = layerIndex(layer);
    KONFYT_ASSERT_RETURN(oldIndex >= 0);

    mLayers.move(oldIndex, newIndex);
}

/* Removes all the layers in the patch. */
void Patch::clearLayers()
{
    while (mLayers.count()) {
        removeLayer(mLayers.first());
    }
}

/* Returns the index of the layer in the patch, -1 if not found. */
int Patch::layerIndex(PatchLayerPtr layer) const
{
    return mLayers.indexOf(layer);
}

void Patch::createLayerResetSnapshots()
{
    foreach (PatchLayerPtr layer, mLayers) {
        layer->createResetSnapshot();
    }
}

void Patch::addLayer(PatchLayerPtr layer)
{
    mLayers.append(layer);
}

PatchLayerPtr Patch::addSfLayer(QString soundfontPath,
                                            KonfytSoundPreset preset)
{
    PatchLayer::SoundfontData sfData;
    sfData.program = preset;
    sfData.parentSoundfont = soundfontPath;

    PatchLayerPtr layer(new PatchLayer());
    layer->initLayer(sfData);

    mLayers.append(layer);

    return layer;
}

int Patch::layerCount() const
{
    return this->mLayers.count();
}

void Patch::setName(QString newName)
{
    this->mPatchName = newName;
}

QString Patch::name() const
{
    return this->mPatchName;
}

void Patch::setNote(QString newNote)
{
    this->mPatchNote = newNote;
}

KonfytReset Patch::getResetOption()
{
    return mResetOption;
}

void Patch::setResetOption(KonfytReset option)
{
    mResetOption = option;
}

QString Patch::note() const
{
    return this->mPatchNote;
}

QList<PatchLayerPtr> Patch::layers()
{
    // Create a list of weak ptrs from shared ptrs list
    QList<PatchLayerPtr> l;
    foreach (PatchLayerPtr layer, mLayers) {
        l.append(layer);
    }
    return l;
}

PatchLayerPtr Patch::layer(int index)
{
    return mLayers.value(index);
}

/* Return list of port project ids of the patch's midi output ports. */
QList<int> Patch::getMidiOutputPortListProjectIds() const
{
    QList<PatchLayerPtr> layers = layersOfType(PatchLayer::TypeMidiOut);
    QList<int> l;
    foreach (PatchLayerPtr layer, layers) {
        l.append(layer->midiOutputPortData.portIdInProject);
    }
    return l;
}

/* Return list of port ids of patch's audio input ports. */
QList<int> Patch::getAudioInPortListProjectIds() const
{
    QList<PatchLayerPtr> layers = layersOfType(PatchLayer::TypeAudioIn);
    QList<int> l;
    foreach (PatchLayerPtr layer, layers) {
        l.append(layer->audioInPortData.portIdInProject);
    }
    return l;
}

QList<PatchLayerPtr> Patch::getAudioInLayerList() const
{
    return layersOfType(PatchLayer::TypeAudioIn);
}

QList<PatchLayerPtr> Patch::getMidiOutputLayerList() const
{
    return layersOfType(PatchLayer::TypeMidiOut);
}

PatchLayerPtr Patch::addAudioInPort(int newPort)
{
    // Set up a default new audio input port with the specified port number
    PatchLayer::AudioInData a;
    a.portIdInProject = newPort;
    QString name = "Audio Input Port " + n2s(newPort);

    return addAudioInPort(a, name);
}

PatchLayerPtr Patch::addAudioInPort(PatchLayer::AudioInData newPort, QString name)
{
    PatchLayerPtr layer;

    // Set up layer item
    layer.reset(new PatchLayer());
    layer->setName(name);
    layer->initLayer(newPort);

    mLayers.append(layer);

    return layer;
}

PatchLayerPtr Patch::addMidiOutputPort(int newPort)
{
    PatchLayer::MidiOutData m;
    m.portIdInProject = newPort;

    return addMidiOutputPort(m);
}

PatchLayerPtr Patch::addMidiOutputPort(PatchLayer::MidiOutData newPort)
{
    PatchLayerPtr layer;

    layer.reset(new PatchLayer());
    layer->initLayer(newPort);

    mLayers.append(layer);

    return layer;
}

PatchLayerPtr Patch::addPlugin(PatchLayer::SfzData newPlugin, QString name)
{
    PatchLayerPtr layer(new PatchLayer);
    layer->setName(name);
    layer->initLayer(newPlugin);

    mLayers.append(layer);

    return layer;
}

QList<PatchLayerPtr> Patch::getPluginLayerList() const
{
    return layersOfType(PatchLayer::TypeSfz);
}

