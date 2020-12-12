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

#ifndef KONFYT_PATCH_ENGINE_H
#define KONFYT_PATCH_ENGINE_H

#include "konfytBridgeEngine.h"
#include "konfytFluidsynthEngine.h"
#include "konfytJackEngine.h"
#include "konfytLscpEngine.h"
#include "konfytPatch.h"
#include "konfytProject.h"
#ifdef KONFYT_USE_CARLA
    #include "konfytCarlaEngine.h"
#endif

#include <jack/jack.h>

#include <QObject>

class KonfytPatchEngine : public QObject
{
    Q_OBJECT
public:
    typedef QSharedPointer<KonfytProject> ProjectPtr;

    explicit KonfytPatchEngine(QObject *parent = 0);
    ~KonfytPatchEngine();

    // ----------------------------------------------------
    // Engine related functions
    // ----------------------------------------------------
    void initPatchEngine(KonfytJackEngine* newJackClient, KonfytAppInfo appInfo);
    QStringList ourJackClientNames();
    void panic(bool p);
    float getMasterGain();
    void setMasterGain(float newGain);

    void setProject(ProjectPtr project);

    // ----------------------------------------------------
    // Loading patches and programs
    // ----------------------------------------------------
    bool loadPatch(KonfytPatch* newPatch);  // Load new patch, replacing current patch.
    void reloadPatch();                     // Reload the current patch (e.g. use if patch changed)
    void unloadPatch(KonfytPatch* patch);
    void unloadLayer(KfPatchLayerWeakPtr layer);
    void reloadLayer(KfPatchLayerWeakPtr layer);
    bool isPatchLoaded(KonfytPatch* patch);

    // ----------------------------------------------------
    // Modify current patch
    // ----------------------------------------------------
    KonfytPatch* currentPatch();
    void setPatchName(QString newName);
    QString getPatchName();
    void setPatchNote(QString newNote);
    QString getPatchNote();
    void setPatchAlwaysActive(bool alwaysActive);

    // ----------------------------------------------------
    // Modify layers
    // ----------------------------------------------------

    // General use for any type of layer
    void setLayerFilter(KfPatchLayerWeakPtr patchLayer, KonfytMidiFilter filter);
    void setLayerGain(KfPatchLayerWeakPtr patchLayer, float newGain);
    void setLayerGain(int layerIndex, float newGain);
    void setLayerSolo(KfPatchLayerWeakPtr patchLayer, bool solo);
    void setLayerSolo(int layerIndex, bool solo);
    void setLayerMute(KfPatchLayerWeakPtr patchLayer, bool mute);
    void setLayerMute(int layerIndex, bool mute);
    void setLayerBus(KfPatchLayerWeakPtr patchLayer, int bus);
    void setLayerMidiInPort(KfPatchLayerWeakPtr patchLayer, int portId);

    void sendCurrentPatchMidi();
    void sendLayerMidi(KfPatchLayerWeakPtr patchLayer);

    int getNumLayers() const;
    void removeLayer(KfPatchLayerWeakPtr layer); // currentPatch
    void removeLayer(KonfytPatch* patch, KfPatchLayerWeakPtr layer);

    // Soundfont / Fluidsynth layers
    KfPatchLayerWeakPtr addProgramLayer(KonfytSoundfontProgram newProgram);

    // SFZ layers
    KfPatchLayerWeakPtr addSfzLayer(QString path);

    // Midi output port layers
    KfPatchLayerWeakPtr addMidiOutPortToPatch(int port);

    // Audio input layers
    KfPatchLayerWeakPtr addAudioInPortToPatch(int port);

signals:
    void print(QString msg);
    void statusInfo(QString msg);
    
private:
    KonfytPatch* mCurrentPatch = nullptr;
    ProjectPtr mCurrentProject;
    float masterGain;
    float convertGain(float linearGain);

    QList<KonfytPatch*> patches;

    void updateLayerRouting(KfPatchLayerSharedPtr layer);
    void updateLayerGain(KfPatchLayerSharedPtr layer);
    void updatePatchLayersGain(KonfytPatch* patch);
    void updatePatchLayersSoloMute(KonfytPatch* patch);

    KonfytFluidsynthEngine* fluidsynthEngine;

    KonfytBaseSoundEngine* sfzEngine;
    bool bridge = false;

    KonfytJackEngine* jack;
    
    void error_abort(QString msg);
};

#endif // KONFYT_PATCH_ENGINE_H
