#ifndef KONFYT_PATCH_ENGINE_H
#define KONFYT_PATCH_ENGINE_H

#include <QObject>
#include "konfytPatch.h"
#include "konfytFluidsynthEngine.h"
#include <jack/jack.h>
#include "konfytJackEngine.h"
#include "konfytProject.h"
#include "konfytCarlaEngine.h"

#define CARLA_CLIENT_POSTFIX "_plugins"

class konfytPatchEngine : public QObject
{
    Q_OBJECT
public:
    explicit konfytPatchEngine(QObject *parent = 0);

    // ----------------------------------------------------
    // Engine related functions
    // ----------------------------------------------------
    void initPatchEngine(konfytJackEngine* newJackClient);
    void panic(bool p);
    float getMasterGain();
    void setMasterGain(float newGain);

    void setProject(konfytProject* project);

    // ----------------------------------------------------
    // Loading patches and programs
    // ----------------------------------------------------
    bool loadPatch(konfytPatch* newPatch);  // Load new patch, replacing current patch.
    void reloadPatch();                     // Reload the current patch (e.g. use if patch changed)
    void unloadPatch(konfytPatch* patch);
    void unloadLayer(konfytPatch*patch, konfytPatchLayer *item);
    konfytPatchLayer reloadLayer(konfytPatchLayer *item);

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
    void setLayerFilter(konfytPatchLayer* layerItem, konfytMidiFilter filter);
    void setLayerGain(konfytPatchLayer* layerItem, float newGain);
    void setLayerGain(int layerIndex, float newGain);
    void setLayerSolo(konfytPatchLayer* layerItem, bool solo);
    void setLayerSolo(int layerIndex, bool solo);
    void setLayerMute(konfytPatchLayer* layerItem, bool mute);
    void setLayerMute(int layerIndex, bool mute);
    void setLayerBus(konfytPatchLayer* layerItem, int bus); // currentPatch
    void setLayerBus(konfytPatch* patch, konfytPatchLayer* layerItem, int bus);

    int getNumLayers();
    void removeLayer(konfytPatchLayer *item); // Perform action on currentPatch
    void removeLayer(konfytPatch* patch, konfytPatchLayer* item);


    // Soundfont / Fluidsynth layers
    konfytPatchLayer addProgramLayer(konfytSoundfontProgram newProgram);

    // Plugin layers
    konfytPatchLayer addSfzLayer(QString path);
    konfytPatchLayer addLV2Layer(QString path);
    konfytPatchLayer addCarlaInternalLayer(QString URI);

    // Midi output port layers
    konfytPatchLayer addMidiOutPortToPatch(int port);

    // Audio input layers
    konfytPatchLayer addAudioInPortToPatch(int port);

    void error_abort(QString msg);
    
private:
    konfytPatch* currentPatch;
    konfytProject* currentProject;
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
    konfytCarlaEngine* carlaEngine;

    // ----------------------------------------------------
    // Jack
    // ----------------------------------------------------
    konfytJackEngine* jack;


signals:
    void userMessage(QString msg);
    
public slots:
    void userMessageFromEngine(QString msg);
    
};

#endif // KONFYT_PATCH_ENGINE_H
