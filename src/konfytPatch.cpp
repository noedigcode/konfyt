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
Result Patch::savePatchToFile(QString filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return Result::failure(
            QString("Failed to open file for writing. File: %1. Error: %2")
                    .arg(filename).arg(file.errorString()));
    }

    QByteArray data = toXmlByteArray();
    qint64 nwritten = file.write(data);
    if (nwritten != data.count()) {
        return Result::failure(
            QString("Only wrote %1 of %2 to file. File: %1. Error: %2")
                    .arg(nwritten).arg(data.count()).arg(filename)
                    .arg(file.errorString()));
    }

    file.close();
    return Result::success();
}

Result Patch::loadPatchFromFile(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return Result::failure(
            QString("Error opening file for reading. File: %1. Error: %2")
                    .arg(filename).arg(file.errorString()));
    }
    QByteArray data = file.readAll();
    file.close();

    return fromXmlByteArray(data);
}

QByteArray Patch::toXmlByteArray() const
{
    Xml xml = toXml();
    QByteArray data = xml.toByteArray();
    return data;
}

Result Patch::fromXmlByteArray(QByteArray data)
{
    Xml xml;
    Xml::Result xmlResult = xml.loadFromData(data);
    if (!xmlResult.ok) {
        return Result::failure(QString("Error loading XML: %1")
                               .arg(xmlResult.toString()));
    }

    readFromXml(xml);

    return Result::success();
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

Xml Patch::toXml() const
{
    Xml xml(XML_PATCH);

    xml.setAttribute(XML_PATCH_NAME, mPatchName);
    xml.addTextChild(XML_PATCH_NOTE, this->note());
    xml.addTextChild(XML_PATCH_ALWAYSACTIVE, QVariant(alwaysActive).toString());
    xml.addTextChild(XML_PATCH_RESET_OPTION, konfytResetToString(mResetOption));
    xml.addChild(patchMidiFilter.toXml());

    foreach (PatchLayerPtr layer, mLayers) {
        xml.addChild(layer->toXml());
    }

    return xml;
}

void Patch::readFromXml(Xml xml)
{
    this->clearLayers();

    mPatchName = xml.attribute(XML_PATCH_NAME);
    setNote(xml.childText(XML_PATCH_NOTE));
    xml.setBoolFromChild(XML_PATCH_ALWAYSACTIVE, &alwaysActive);
    mResetOption = konfytResetFromString(
                xml.childText(XML_PATCH_RESET_OPTION),
                KonfytReset::Inherit);
    patchMidiFilter.readFromXml(xml.child(MidiFilter::XML_MIDIFILTER));

    QList<Xml> layersXml = xml.childrenNamed(
                {PatchLayer::XML_SF_LAYER,
                 PatchLayer::XML_SFZ_LAYER,
                 PatchLayer::XML_MIDIOUT_LAYER,
                 PatchLayer::XML_AUDIOIN_LAYER});
    foreach (Xml layerXml, layersXml) {
        PatchLayerPtr layer(new PatchLayer());
        layer->readFromXml(layerXml);
        mLayers.append(layer);
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

PatchLayerPtr Patch::addSfLayer(QString soundfontPath, KonfytSoundPreset preset)
{
    PatchLayer::SoundfontData sfData;
    sfData.program = preset;
    sfData.soundfontFilePath = soundfontPath;

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

    return addAudioInPort(a);
}

PatchLayerPtr Patch::addAudioInPort(PatchLayer::AudioInData newPort)
{
    PatchLayerPtr layer;

    // Set up layer item
    layer.reset(new PatchLayer());
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

PatchLayerPtr Patch::addPlugin(PatchLayer::SfzData newPlugin)
{
    PatchLayerPtr layer(new PatchLayer);
    layer->initLayer(newPlugin);

    mLayers.append(layer);

    return layer;
}

QList<PatchLayerPtr> Patch::getPluginLayerList() const
{
    return layersOfType(PatchLayer::TypeSfz);
}

