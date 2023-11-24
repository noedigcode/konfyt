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
    void loadPatchAndSetCurrent(KonfytPatch* patch);
    void loadPatch(KonfytPatch* patch);
    void reloadPatch();
    void unloadPatch(KonfytPatch* patch);
    void unloadLayerFromEngines(KfPatchLayerWeakPtr layer);
    void reloadLayer(KfPatchLayerWeakPtr layer);
    bool isPatchLoaded(KonfytPatch* patch);

    KonfytPatch* currentPatch();
    void setPatchFilter(KonfytPatch* patch, KonfytMidiFilter filter);

    // ----------------------------------------------------
    // Modify layers
    // ----------------------------------------------------

    // General use for any type of layer
    void setLayerFilter(KfPatchLayerWeakPtr patchLayer, KonfytMidiFilter filter);
    void setLayerGain(KfPatchLayerWeakPtr patchLayer, float newGain);
    void setLayerGain(int layerIndex, float newGain);
    void setLayerGainByMidi(int layerIndex, int midiValue);
    void setLayerGainByMidiRelative(int layerIndex, int midiValue);
    void setLayerSolo(KfPatchLayerWeakPtr patchLayer, bool solo);
    void setLayerSolo(int layerIndex, bool solo);
    void setLayerMute(KfPatchLayerWeakPtr patchLayer, bool mute);
    void setLayerMute(int layerIndex, bool mute);
    void setLayerBus(KfPatchLayerWeakPtr patchLayer, int bus);
    void setLayerMidiInPort(KfPatchLayerWeakPtr patchLayer, int portId);

    void setLayerScript(KfPatchLayerSharedPtr patchLayer, QString script);
    void setLayerScriptEnabled(KfPatchLayerSharedPtr patchLayer, bool enable);
    void setLayerPassMidiThrough(KfPatchLayerSharedPtr patchLayer, bool pass);

    void sendCurrentPatchMidi();
    void sendLayerMidi(KfPatchLayerWeakPtr patchLayer);

    int getNumLayers() const;
    void removeLayer(KfPatchLayerWeakPtr layer); // currentPatch
    void removeLayer(KonfytPatch* patch, KfPatchLayerWeakPtr layer);
    void moveLayer(KfPatchLayerWeakPtr layer, int newIndex);

    // Soundfont / Fluidsynth layers
    KfPatchLayerWeakPtr addSfProgramLayer(QString soundfontPath,
                                          KonfytSoundPreset newProgram);

    // SFZ layers
    KfPatchLayerWeakPtr addSfzLayer(QString path);

    // Midi output port layers
    KfPatchLayerWeakPtr addMidiOutPortToPatch(int port);

    // Audio input layers
    KfPatchLayerWeakPtr addAudioInPortToPatch(int port);

signals:
    void print(QString msg);
    void statusInfo(QString msg);
    void patchLayerLoaded(KfPatchLayerWeakPtr layer);
    
private:
    KonfytPatch* mCurrentPatch = nullptr;
    ProjectPtr mCurrentProject;
    int mMidiPickupRange = 127;

    QList<KonfytPatch*> patches;

    void loadSfzLayer(KfPatchLayerSharedPtr layer);
    void loadSoundfontLayer(KfPatchLayerSharedPtr layer);
    void loadAudioInputPort(KfPatchLayerSharedPtr layer);
    void loadMidiOutputPort(KfPatchLayerSharedPtr layer);
    void updateLayerRouting(KfPatchLayerSharedPtr layer);
    void updateLayerGain(KfPatchLayerSharedPtr layer);
    void updatePatchLayersSoloMute(KonfytPatch* patch);
    void setLayerActive(KfPatchLayerSharedPtr layer, bool active);
    void updateLayerPatchMidiFilterInJackEngine(KonfytPatch* patch,
                                                KfPatchLayerSharedPtr layer);

    void updateLayerBlockMidiDirectThroughInJack(KfPatchLayerSharedPtr patchLayer);

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
};

#endif // KONFYT_PATCH_ENGINE_H
