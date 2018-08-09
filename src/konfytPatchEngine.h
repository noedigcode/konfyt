/******************************************************************************
 *
 * Copyright 2017 Gideon van der Kolf
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

#include <QObject>

#include <jack/jack.h>

#include "konfytBridgeEngine.h"
#include "konfytPatch.h"
#include "konfytFluidsynthEngine.h"
#include "konfytJackEngine.h"
#include "konfytProject.h"
#include "konfytCarlaEngine.h"

class konfytPatchEngine : public QObject
{
    Q_OBJECT
public:
    explicit konfytPatchEngine(QObject *parent = 0);

    // ----------------------------------------------------
    // Engine related functions
    // ----------------------------------------------------
    void initPatchEngine(KonfytJackEngine* newJackClient, KonfytAppInfo appInfo);
    void panic(bool p);
    float getMasterGain();
    void setMasterGain(float newGain);

    void setProject(KonfytProject* project);

    // ----------------------------------------------------
    // Loading patches and programs
    // ----------------------------------------------------
    bool loadPatch(konfytPatch* newPatch);  // Load new patch, replacing current patch.
    void reloadPatch();                     // Reload the current patch (e.g. use if patch changed)
    void unloadPatch(konfytPatch* patch);
    void unloadLayer(konfytPatch*patch, KonfytPatchLayer *item);
    KonfytPatchLayer reloadLayer(KonfytPatchLayer *item);

    // ----------------------------------------------------
    // Modify current patch
    // ----------------------------------------------------
    konfytPatch* getPatch();
    void setPatchName(QString newName);
    QString getPatchName();
    void setPatchNote(QString newNote);
    QString getPatchNote();

    // ----------------------------------------------------
    // Modify layers
    // ----------------------------------------------------

    // General use for any type of layer
    void setLayerFilter(KonfytPatchLayer* layerItem, KonfytMidiFilter filter);
    void setLayerGain(KonfytPatchLayer* layerItem, float newGain);
    void setLayerGain(int layerIndex, float newGain);
    void setLayerSolo(KonfytPatchLayer* layerItem, bool solo);
    void setLayerSolo(int layerIndex, bool solo);
    void setLayerMute(KonfytPatchLayer* layerItem, bool mute);
    void setLayerMute(int layerIndex, bool mute);
    void setLayerBus(KonfytPatchLayer* layerItem, int bus); // currentPatch
    void setLayerBus(konfytPatch* patch, KonfytPatchLayer* layerItem, int bus);
    void setLayerMidiInPort(KonfytPatchLayer* layerItem, int portId); // currentPatch
    void setLayerMidiInPort(konfytPatch* patch, KonfytPatchLayer* layerItem, int portId);

    int getNumLayers();
    void removeLayer(KonfytPatchLayer *item); // Perform action on currentPatch
    void removeLayer(konfytPatch* patch, KonfytPatchLayer* item);


    // Soundfont / Fluidsynth layers
    KonfytPatchLayer addProgramLayer(konfytSoundfontProgram newProgram);

    // Plugin layers
    KonfytPatchLayer addSfzLayer(QString path);
    KonfytPatchLayer addLV2Layer(QString path);
    KonfytPatchLayer addCarlaInternalLayer(QString URI);

    // Midi output port layers
    KonfytPatchLayer addMidiOutPortToPatch(int port);

    // Audio input layers
    KonfytPatchLayer addAudioInPortToPatch(int port);

    void error_abort(QString msg);
    
private:
    konfytPatch* currentPatch;
    KonfytProject* currentProject;
    float masterGain;
    float convertGain(float linearGain);

    QList<konfytPatch*> patches;

    void refreshAllGainsAndRouting();

    // ----------------------------------------------------
    // Fluidsynth / Soundfont related
    // ----------------------------------------------------
    konfytFluidsynthEngine* fluidsynthEngine;

    // ----------------------------------------------------
    // Carla plugins
    // ----------------------------------------------------
    KonfytBaseSoundEngine* carlaEngine;
    bool bridge;

    // ----------------------------------------------------
    // Jack
    // ----------------------------------------------------
    KonfytJackEngine* jack;


signals:
    void userMessage(QString msg);
    
public slots:
    void userMessageFromEngine(QString msg);
    
};

#endif // KONFYT_PATCH_ENGINE_H
