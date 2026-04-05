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


QList<KonfytPatchLayerPtr> KonfytPatch::getSfLayerList() const
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

QList<KonfytPatchLayerPtr> KonfytPatch::layersOfType(
        KonfytPatchLayer::LayerType layerType) const
{
    QList<KonfytPatchLayerPtr> l;
    foreach (KonfytPatchLayerPtr layer, layerList) {
        if (layer->layerType() == layerType) {
            l.append(layer);
        }
    }

    return l;
}

void KonfytPatch::writeToXmlStream(QXmlStreamWriter *stream) const
{
    stream->setAutoFormatting(true);
    stream->writeStartDocument();

    stream->writeComment("This is a Konfyt patch file.");

    stream->writeStartElement(XML_PATCH);
    stream->writeAttribute(XML_PATCH_NAME, this->patchName);

    stream->writeTextElement(XML_PATCH_NOTE, this->note());
    stream->writeTextElement(XML_PATCH_ALWAYSACTIVE, QVariant(alwaysActive).toString());
    stream->writeTextElement(XML_PATCH_RESET_OPTION, konfytResetToString(mResetOption));

    patchMidiFilter.writeToXMLStream(stream);

    // Save all layers in order.
    foreach (KonfytPatchLayerPtr layer, layerList) {
        layer->writeToXmlStream(stream);
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

            } else if (r->name() == XML_PATCH_RESET_OPTION) {

                mResetOption = konfytResetFromString(r->readElementText(),
                                                     KonfytReset::Inherit);

            } else if (r->name() == XML_MIDIFILTER) {

                this->patchMidiFilter.readFromXMLStream(r);

            } else if (r->name() == XML_PATCH_SFLAYER) {

                xmlReadLayer(r, errors);

            } else if (r->name() == XML_PATCH_SFZLAYER) {

                xmlReadLayer(r, errors);

            } else if (r->name() == XML_PATCH_MIDIOUT) {

                xmlReadLayer(r, errors);

            } else if (r->name() == XML_PATCH_AUDIOIN) {

                xmlReadLayer(r, errors);

            } else {
                appendError(errors, "Unrecognized layer type: " + r->name().toString() );
                r->skipCurrentElement(); // not any of the layer types
            }
        }
    }
}

void KonfytPatch::xmlReadLayer(QXmlStreamReader *r, QString *errors)
{
    KonfytPatchLayerPtr layer(new KonfytPatchLayer());
    layer->readFromXmlStream(r, errors);
    layerList.append(layer);
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

void KonfytPatch::removeLayer(KonfytPatchLayerPtr layer)
{
    layerList.removeAll(layer);
}

void KonfytPatch::moveLayer(KonfytPatchLayerPtr layer, int newIndex)
{
    KONFYT_ASSERT_RETURN(newIndex >= 0);
    KONFYT_ASSERT_RETURN(newIndex < layerList.count());

    int oldIndex = layerIndex(layer);
    KONFYT_ASSERT_RETURN(oldIndex >= 0);

    layerList.move(oldIndex, newIndex);
}

/* Removes all the layers in the patch. */
void KonfytPatch::clearLayers()
{
    while (layerList.count()) {
        removeLayer(layerList.first());
    }
}

/* Returns the index of the layer in the patch, -1 if not found. */
int KonfytPatch::layerIndex(KonfytPatchLayerPtr layer) const
{
    return layerList.indexOf(layer);
}

void KonfytPatch::createLayerResetSnapshots()
{
    foreach (KonfytPatchLayerPtr layer, layerList) {
        layer->createResetSnapshot();
    }
}

void KonfytPatch::addLayer(KonfytPatchLayerPtr layer)
{
    layerList.append(layer);
}

KonfytPatchLayerPtr KonfytPatch::addSfLayer(QString soundfontPath,
                                            KonfytSoundPreset preset)
{
    LayerSoundfontData sfData;
    sfData.program = preset;
    sfData.parentSoundfont = soundfontPath;

    KonfytPatchLayerPtr layer(new KonfytPatchLayer());
    layer->initLayer(sfData);

    layerList.append(layer);

    return layer;
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

KonfytReset KonfytPatch::getResetOption()
{
    return mResetOption;
}

void KonfytPatch::setResetOption(KonfytReset option)
{
    mResetOption = option;
}

QString KonfytPatch::note() const
{
    return this->patchNote;
}

QList<KonfytPatchLayerPtr> KonfytPatch::layers()
{
    // Create a list of weak ptrs from shared ptrs list
    QList<KonfytPatchLayerPtr> l;
    foreach (KonfytPatchLayerPtr layer, layerList) {
        l.append(layer);
    }
    return l;
}

KonfytPatchLayerPtr KonfytPatch::layer(int index)
{
    return layerList.value(index);
}

/* Return list of port project ids of the patch's midi output ports. */
QList<int> KonfytPatch::getMidiOutputPortListProjectIds() const
{
    QList<KonfytPatchLayerPtr> layers = layersOfType(KonfytPatchLayer::TypeMidiOut);
    QList<int> l;
    foreach (KonfytPatchLayerPtr layer, layers) {
        l.append(layer->midiOutputPortData.portIdInProject);
    }
    return l;
}

/* Return list of port ids of patch's audio input ports. */
QList<int> KonfytPatch::getAudioInPortListProjectIds() const
{
    QList<KonfytPatchLayerPtr> layers = layersOfType(KonfytPatchLayer::TypeAudioIn);
    QList<int> l;
    foreach (KonfytPatchLayerPtr layer, layers) {
        l.append(layer->audioInPortData.portIdInProject);
    }
    return l;
}

QList<KonfytPatchLayerPtr> KonfytPatch::getAudioInLayerList() const
{
    return layersOfType(KonfytPatchLayer::TypeAudioIn);
}

QList<KonfytPatchLayerPtr> KonfytPatch::getMidiOutputLayerList() const
{
    return layersOfType(KonfytPatchLayer::TypeMidiOut);
}

KonfytPatchLayerPtr KonfytPatch::addAudioInPort(int newPort)
{
    // Set up a default new audio input port with the specified port number
    LayerAudioInData a;
    a.portIdInProject = newPort;
    QString name = "Audio Input Port " + n2s(newPort);

    return addAudioInPort(a, name);
}

KonfytPatchLayerPtr KonfytPatch::addAudioInPort(LayerAudioInData newPort, QString name)
{
    KonfytPatchLayerPtr layer;

    // Set up layer item
    layer.reset(new KonfytPatchLayer());
    layer->setName(name);
    layer->initLayer(newPort);

    layerList.append(layer);

    return layer;
}

KonfytPatchLayerPtr KonfytPatch::addMidiOutputPort(int newPort)
{
    LayerMidiOutData m = LayerMidiOutData();
    m.portIdInProject = newPort;

    return addMidiOutputPort(m);
}

KonfytPatchLayerPtr KonfytPatch::addMidiOutputPort(LayerMidiOutData newPort)
{
    KonfytPatchLayerPtr layer;

    layer.reset(new KonfytPatchLayer());
    layer->initLayer(newPort);

    layerList.append(layer);

    return layer;
}

KonfytPatchLayerPtr KonfytPatch::addPlugin(LayerSfzData newPlugin, QString name)
{
    KonfytPatchLayerPtr layer(new KonfytPatchLayer);
    layer->setName(name);
    layer->initLayer(newPlugin);

    layerList.append(layer);

    return layer;
}

QList<KonfytPatchLayerPtr> KonfytPatch::getPluginLayerList() const
{
    return layersOfType(KonfytPatchLayer::TypeSfz);
}

