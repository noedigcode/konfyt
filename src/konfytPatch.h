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

#ifndef KONFYT_PATCH_H
#define KONFYT_PATCH_H

#include "konfytMidiFilter.h"
#include "konfytPatchLayer.h"

#include <QFile>
#include <QSharedPointer>


class Patch
{
public:

    // -----------------------------------------------------------------------
    // Patch Info

    QString name() const;
    void setName(QString newName);
    QString note() const;
    void setNote(QString newNote);
    bool alwaysActive = false;
    MidiFilter patchMidiFilter = MidiFilter::allPassFilter(); // Patch-wide MIDI filter
    KonfytReset getResetOption();
    void setResetOption(KonfytReset option);

    // -----------------------------------------------------------------------
    // General layer related functions

    QList<PatchLayerPtr> layers();
    PatchLayerPtr layer(int index);
    int layerCount() const;
    bool isValidLayerIndex(int layerIndex) const;
    void removeLayer(PatchLayerPtr layer);
    void moveLayer(PatchLayerPtr layer, int newIndex);
    void clearLayers();
    int layerIndex(PatchLayerPtr layer) const;
    void createLayerResetSnapshots();

    void addLayer(PatchLayerPtr layer);

    // -----------------------------------------------------------------------
    // Soundfont layer related functions

    PatchLayerPtr addSfLayer(QString soundfontPath, KonfytSoundPreset preset);
    QList<PatchLayerPtr> getSfLayerList() const;

    // -----------------------------------------------------------------------
    // SFZ Plugin functions

    PatchLayerPtr addPlugin(PatchLayer::SfzData newPlugin);
    QList<PatchLayerPtr> getPluginLayerList() const;

    // -----------------------------------------------------------------------
    // Midi routing

    QList<int> getMidiOutputPortListProjectIds() const;
    QList<PatchLayerPtr> getMidiOutputLayerList() const;
    PatchLayerPtr addMidiOutputPort(int newPort);
    PatchLayerPtr addMidiOutputPort(PatchLayer::MidiOutData newPort);

    // -----------------------------------------------------------------------
    // Audio input ports

    QList<int> getAudioInPortListProjectIds() const;
    QList<PatchLayerPtr> getAudioInLayerList() const;
    PatchLayerPtr addAudioInPort(int newPort);
    PatchLayerPtr addAudioInPort(PatchLayer::AudioInData newPort);

    // -----------------------------------------------------------------------
    // Save/load functions

    static constexpr const char* PATCH_FILE_EXTENSION_NODOT = "konfytpatch";

    Result savePatchToFile(QString filename) const;
    Result loadPatchFromFile(QString filename);

    QByteArray toXmlByteArray() const;
    Result fromXmlByteArray(QByteArray data);
    Xml toXml() const;
    void readFromXml(Xml xml);

private:
    QString mPatchName;
    QString mPatchNote;  // Custom note for user instructions or to describe the patch
    KonfytReset mResetOption = KonfytReset::Inherit;

    // List of layers (all types, order is important for user in the patch)
    QList<PatchLayerPtr> mLayers;

    QList<PatchLayerPtr> layersOfType(PatchLayer::LayerType layerType) const;

    static constexpr const char* XML_PATCH = "sfpatch";
    static constexpr const char* XML_PATCH_NAME = "name";
    static constexpr const char* XML_PATCH_NOTE = "patchNote";
    static constexpr const char* XML_PATCH_ALWAYSACTIVE = "alwaysActive";
    static constexpr const char* XML_PATCH_RESET_OPTION = "resetOption";
};

typedef QSharedPointer<Patch> PatchPtr;

#endif // KONFYT_PATCH_H
