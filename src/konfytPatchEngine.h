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

#ifndef KONFYT_PATCH_ENGINE_H
#define KONFYT_PATCH_ENGINE_H

#include "konfytAudio.h"
#include "konfytBridgeEngine.h"
#include "konfytFluidsynthEngine.h"
#include "konfytJackEngine.h"
#include "konfytJs.h"
#include "konfytLscpEngine.h"
#include "konfytPatch.h"
#include "konfytProject.h"
#ifdef KONFYT_USE_CARLA
    #include "konfytCarlaEngine.h"
#endif

#include <QObject>

class KonfytPatchEngine : public QObject
{
    Q_OBJECT
public:
    typedef QSharedPointer<Project> ProjectPtr;

    explicit KonfytPatchEngine(QObject *parent = 0);
    ~KonfytPatchEngine();

    // -----------------------------------------------------------------------
    // Engine related functions

    void initPatchEngine(KonfytJackEngine* jackEngine,
                         KonfytJSEngine* scriptEngine,
                         KonfytAppInfo appInfo);
    QStringList ourJackClientNames();
    void panic(bool p);
    void setMidiPickupRange(int range);

    void setProject(ProjectPtr project);

    // -----------------------------------------------------------------------
    // Patches

    void loadPatchAndSetCurrent(Patch* newPatch);
    void loadPatch(Patch* patch);
    void reloadPatch();
    void unloadPatch(Patch* patch);
    void unloadLayerFromEngines(PatchLayerPtr layer);
    void reloadLayer(PatchLayerPtr layer);
    bool isPatchLoaded(Patch* patch);

    Patch* currentPatch();
    void setPatchFilter(Patch* patch, MidiFilter filter);

    // -----------------------------------------------------------------------
    // Modify layers

    void setLayerFilter(PatchLayerPtr patchLayer, MidiFilter filter);
    void setLayerGain(PatchLayerPtr patchLayer, float newGain);
    void setLayerGain(int layerIndex, float newGain);
    void setLayerGainByMidi(int layerIndex, int midiValue);
    void setLayerGainByMidiRelative(int layerIndex, int midiValue);
    void setLayerSolo(PatchLayerPtr patchLayer, bool solo);
    void setLayerSolo(int layerIndex, bool solo);
    void setLayerMute(PatchLayerPtr patchLayer, bool mute);
    void setLayerMute(int layerIndex, bool mute);
    void setLayerBus(PatchLayerPtr patchLayer, int bus);
    void setLayerMidiInPort(PatchLayerPtr patchLayer, int portId);

    void setLayerScript(PatchLayerPtr patchLayer, QString script);
    void setLayerScriptEnabled(PatchLayerPtr patchLayer, bool enable);
    void setLayerPassMidiThrough(PatchLayerPtr patchLayer, bool pass);

    void sendCurrentPatchMidi();
    void sendLayerMidi(PatchLayerPtr patchLayer);

    int getNumLayers() const;
    void removeLayer(PatchLayerPtr layer); // currentPatch
    void removeLayer(Patch* patch, PatchLayerPtr layer);
    void moveLayer(PatchLayerPtr layer, int newIndex);

    void addLayer(PatchLayerPtr layer);
    PatchLayerPtr addSfProgramLayer(QString soundfontPath,
                                    KonfytSoundPreset newProgram);
    PatchLayerPtr addSfzLayer(QString path);
    PatchLayerPtr addMidiOutPortToPatch(int port);
    PatchLayerPtr addAudioInPortToPatch(int port);

signals:
    void print(QString msg);
    void statusInfo(QString msg);
    void patchLayerLoaded(PatchLayerPtr layer);
    void patchLayerUnloaded(PatchLayerPtr layer);
    void sfzEngineErrorStringChanged(QString errorString);
    
private:
    Patch* mCurrentPatch = nullptr;
    ProjectPtr mCurrentProject;
    int mMidiPickupRange = 127;

    QList<Patch*> mPatches;

    void loadSfzLayer(PatchLayerPtr layer);
    void loadSoundfontLayer(PatchLayerPtr layer);
    void loadAudioInputPort(PatchLayerPtr layer);
    void loadMidiOutputPort(PatchLayerPtr layer);
    void updateLayerRouting(PatchLayerPtr layer);
    void updateLayerGain(PatchLayerPtr layer);
    void activatePatchLayerRoutesForSoloMute(Patch* patch);
    void setLayerActive(PatchLayerPtr layer, bool active);
    void updateLayerPatchMidiFilterInJackEngine(Patch* patch,
                                                PatchLayerPtr layer);

    void updateLayerBlockMidiDirectThroughInJack(PatchLayerPtr patchLayer);

    KonfytFluidsynthEngine fluidsynthEngine;
    void setupAndInitFluidsynthEngine();

    KonfytBaseSoundEngine* sfzEngine;
    void setupAndInitSfzEngine(KonfytAppInfo appInfo);

    KonfytJSEngine* scriptEngine = nullptr;

    KonfytJackEngine* jack = nullptr;

    void updatePatchLayersURIs(Patch* patch);

private slots:
    void onSfzEngineInitDone(QString error);
    void onProjectPatchURIsNeedUdating();
    void onProjectModifiedStateChanged(bool modified);
};

#endif // KONFYT_PATCH_ENGINE_H
