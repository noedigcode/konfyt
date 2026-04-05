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
    typedef QSharedPointer<KonfytProject> ProjectPtr;

    explicit KonfytPatchEngine(QObject *parent = 0);
    ~KonfytPatchEngine();

    // ----------------------------------------------------
    // Engine related functions
    // ----------------------------------------------------
    void initPatchEngine(KonfytJackEngine* jackEngine,
                         KonfytJSEngine* scriptEngine,
                         KonfytAppInfo appInfo);
    QStringList ourJackClientNames();
    void panic(bool p);
    void setMidiPickupRange(int range);

    void setProject(ProjectPtr project);

    // ----------------------------------------------------
    // Patches
    // ----------------------------------------------------
    void loadPatchAndSetCurrent(KonfytPatch* newPatch);
    void loadPatch(KonfytPatch* patch);
    void reloadPatch();
    void unloadPatch(KonfytPatch* patch);
    void unloadLayerFromEngines(KonfytPatchLayerPtr layer);
    void reloadLayer(KonfytPatchLayerPtr layer);
    bool isPatchLoaded(KonfytPatch* patch);

    KonfytPatch* currentPatch();
    void setPatchFilter(KonfytPatch* patch, KonfytMidiFilter filter);

    // ----------------------------------------------------
    // Modify layers
    // ----------------------------------------------------

    // General use for any type of layer
    void setLayerFilter(KonfytPatchLayerPtr patchLayer, KonfytMidiFilter filter);
    void setLayerGain(KonfytPatchLayerPtr patchLayer, float newGain);
    void setLayerGain(int layerIndex, float newGain);
    void setLayerGainByMidi(int layerIndex, int midiValue);
    void setLayerGainByMidiRelative(int layerIndex, int midiValue);
    void setLayerSolo(KonfytPatchLayerPtr patchLayer, bool solo);
    void setLayerSolo(int layerIndex, bool solo);
    void setLayerMute(KonfytPatchLayerPtr patchLayer, bool mute);
    void setLayerMute(int layerIndex, bool mute);
    void setLayerBus(KonfytPatchLayerPtr patchLayer, int bus);
    void setLayerMidiInPort(KonfytPatchLayerPtr patchLayer, int portId);

    void setLayerScript(KonfytPatchLayerPtr patchLayer, QString script);
    void setLayerScriptEnabled(KonfytPatchLayerPtr patchLayer, bool enable);
    void setLayerPassMidiThrough(KonfytPatchLayerPtr patchLayer, bool pass);

    void sendCurrentPatchMidi();
    void sendLayerMidi(KonfytPatchLayerPtr patchLayer);

    int getNumLayers() const;
    void removeLayer(KonfytPatchLayerPtr layer); // currentPatch
    void removeLayer(KonfytPatch* patch, KonfytPatchLayerPtr layer);
    void moveLayer(KonfytPatchLayerPtr layer, int newIndex);

    void addLayer(KonfytPatchLayerPtr layer);
    KonfytPatchLayerPtr addSfProgramLayer(QString soundfontPath,
                                          KonfytSoundPreset newProgram);
    KonfytPatchLayerPtr addSfzLayer(QString path);
    KonfytPatchLayerPtr addMidiOutPortToPatch(int port);
    KonfytPatchLayerPtr addAudioInPortToPatch(int port);

signals:
    void print(QString msg);
    void statusInfo(QString msg);
    void patchLayerLoaded(KonfytPatchLayerPtr layer);
    void patchLayerUnloaded(KonfytPatchLayerPtr layer);
    void sfzEngineErrorStringChanged(QString errorString);
    
private:
    KonfytPatch* mCurrentPatch = nullptr;
    ProjectPtr mCurrentProject;
    int mMidiPickupRange = 127;

    QList<KonfytPatch*> patches;

    void loadSfzLayer(KonfytPatchLayerPtr layer);
    void loadSoundfontLayer(KonfytPatchLayerPtr layer);
    void loadAudioInputPort(KonfytPatchLayerPtr layer);
    void loadMidiOutputPort(KonfytPatchLayerPtr layer);
    void updateLayerRouting(KonfytPatchLayerPtr layer);
    void updateLayerGain(KonfytPatchLayerPtr layer);
    void activatePatchLayerRoutesForSoloMute(KonfytPatch* patch);
    void setLayerActive(KonfytPatchLayerPtr layer, bool active);
    void updateLayerPatchMidiFilterInJackEngine(KonfytPatch* patch,
                                                KonfytPatchLayerPtr layer);

    void updateLayerBlockMidiDirectThroughInJack(KonfytPatchLayerPtr patchLayer);

    KonfytFluidsynthEngine fluidsynthEngine;
    void setupAndInitFluidsynthEngine();

    KonfytBaseSoundEngine* sfzEngine;
    void setupAndInitSfzEngine(KonfytAppInfo appInfo);

    KonfytJSEngine* scriptEngine = nullptr;

    KonfytJackEngine* jack = nullptr;

    void updatePatchLayersURIs(KonfytPatch* patch);

private slots:
    void onSfzEngineInitDone(QString error);
    void onProjectPatchURIsNeedUdating();
    void onProjectModifiedStateChanged(bool modified);
};

#endif // KONFYT_PATCH_ENGINE_H
