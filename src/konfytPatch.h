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
#include <QWeakPointer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>


#define XML_PATCH "sfpatch" // TODO: Change this to simply "patch"
#define XML_PATCH_NAME "name"
#define XML_PATCH_NOTE "patchNote"
#define XML_PATCH_ALWAYSACTIVE "alwaysActive"
#define XML_PATCH_RESET_OPTION "resetOption"


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
    KonfytMidiFilter patchMidiFilter = KonfytMidiFilter::allPassFilter(); // Patch-wide MIDI filter
    KonfytReset getResetOption();
    void setResetOption(KonfytReset option);

    // ----------------------------------------------------
    // General layer related functions
    // ----------------------------------------------------
    QList<KonfytPatchLayerPtr> layers();
    KonfytPatchLayerPtr layer(int index);
    int layerCount() const;
    bool isValidLayerIndex(int layerIndex) const;
    void removeLayer(KonfytPatchLayerPtr layer);
    void moveLayer(KonfytPatchLayerPtr layer, int newIndex);
    void clearLayers();
    int layerIndex(KonfytPatchLayerPtr layer) const;
    void createLayerResetSnapshots();

    void addLayer(KonfytPatchLayerPtr layer);

    // ----------------------------------------------------
    // Soundfont layer related functions
    // ----------------------------------------------------
    KonfytPatchLayerPtr addSfLayer(QString soundfontPath, KonfytSoundPreset preset);
    QList<KonfytPatchLayerPtr> getSfLayerList() const;

    // ----------------------------------------------------
    // SFZ Plugin functions
    // ----------------------------------------------------
    KonfytPatchLayerPtr addPlugin(LayerSfzData newPlugin, QString name);
    QList<KonfytPatchLayerPtr> getPluginLayerList() const;

    // ----------------------------------------------------
    // Midi routing
    // ----------------------------------------------------
    QList<int> getMidiOutputPortListProjectIds() const;
    QList<KonfytPatchLayerPtr> getMidiOutputLayerList() const;
    KonfytPatchLayerPtr addMidiOutputPort(int newPort);
    KonfytPatchLayerPtr addMidiOutputPort(LayerMidiOutData newPort);

    // ----------------------------------------------------
    // Audio input ports
    // ----------------------------------------------------
    QList<int> getAudioInPortListProjectIds() const;
    QList<KonfytPatchLayerPtr> getAudioInLayerList() const;
    KonfytPatchLayerPtr addAudioInPort(int newPort);
    KonfytPatchLayerPtr addAudioInPort(LayerAudioInData newPort, QString name);

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
    KonfytReset mResetOption = KonfytReset::Inherit;

    // List of layers (all types, order is important for user in the patch)
    QList<KonfytPatchLayerPtr> layerList;

    QList<KonfytPatchLayerPtr> layersOfType(KonfytPatchLayer::LayerType layerType) const;

    void writeToXmlStream(QXmlStreamWriter* stream) const;
    void readFromXmlStream(QXmlStreamReader* r, QString* errors = nullptr);

    void xmlReadLayer(QXmlStreamReader* r, QString* errors = nullptr);

    void appendError(QString *errorString, QString msg);
};

#endif // KONFYT_PATCH_H
