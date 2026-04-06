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

#include "konfytPatchEngine.h"


KonfytPatchEngine::KonfytPatchEngine(QObject *parent) :
    QObject(parent)
{
}

KonfytPatchEngine::~KonfytPatchEngine()
{
    delete sfzEngine;
}

void KonfytPatchEngine::initPatchEngine(KonfytJackEngine* jackEngine,
                                        KonfytJSEngine *scriptEngine,
                                        KonfytAppInfo appInfo)
{
    this->jack = jackEngine;
    this->scriptEngine = scriptEngine;

    setupAndInitFluidsynthEngine();
    setupAndInitSfzEngine(appInfo);
}

/* Returns names of JACK clients that refer to engines in use by us. */
QStringList KonfytPatchEngine::ourJackClientNames()
{
    QStringList ret;
    ret.append(sfzEngine->jackClientName());
    return ret;
}

void KonfytPatchEngine::panic(bool p)
{
    // Indicate panic situation to JACK
    jack->panic(p);
}

void KonfytPatchEngine::setProject(ProjectPtr project)
{
    mCurrentProject = project;
    connect(project.data(), &Project::patchURIsNeedUpdating,
            this, &KonfytPatchEngine::onProjectPatchURIsNeedUdating);
    connect(project.data(), &Project::projectModifiedStateChanged,
            this, &KonfytPatchEngine::onProjectModifiedStateChanged);

    onProjectPatchURIsNeedUdating();
}

/* Reload current patch (e.g. if patch changed). */
void KonfytPatchEngine::reloadPatch()
{
    loadPatchAndSetCurrent(mCurrentPatch);
}

/* Ensure patch and all layers are unloaded from their respective engines. */
void KonfytPatchEngine::unloadPatch(Patch *patch)
{
    if ( !mPatches.contains(patch) ) { return; }

    foreach (PatchLayerPtr layer, patch->layers()) {
        unloadLayerFromEngines(layer);
    }

    mPatches.removeAll(patch);
    if (mCurrentPatch == patch) { mCurrentPatch = nullptr; }
}

/* Load newPatch, replacing the current newPatch. */
void KonfytPatchEngine::loadPatchAndSetCurrent(Patch *newPatch)
{
    if (newPatch == nullptr) { return; }

    bool switchToDifferentPatch = (newPatch != mCurrentPatch);

    if (mCurrentPatch && switchToDifferentPatch) {

        // Switching away from current patch.
        // Deactivate routes and restore snapshot depending on reset option.
        foreach (PatchLayerPtr layer, mCurrentPatch->layers()) {

            // Deactivate layer
            if (!mCurrentPatch->alwaysActive) {
                setLayerActive(layer, false);
            }

            // Restore snapshot
            if (!mCurrentPatch->alwaysActive) {

                // Determine inherited reset option from project and patch
                KonfytReset projectOption = KonfytReset::NoReset;
                if (mCurrentProject) {
                    projectOption = mCurrentProject->getResetOption();
                }
                KonfytReset patchOption = mCurrentPatch->getResetOption();

                KonfytReset inheritedOption = konfytResetFromInherits(
                                                {patchOption, projectOption},
                                                KonfytReset::NoReset);

                // Restore layer reset snapshot, providing inherited option
                // from project and patch which will be used if the layer's
                // is set to inherited.
                layer->restoreResetSnapshotIfAllowed(inheritedOption);
            }

        }
    }

    if (switchToDifferentPatch) {
        // Switched to patch. Create a reset snapshot for each layer.
        // Only do this when switching to a different patch, as this function
        // could be called on the current patch when loading new layers, in
        // which situation we don't want to create a snapshot for the newly
        // created layers.
        newPatch->createLayerResetSnapshots();
    }

    mCurrentPatch = newPatch;
    loadPatch(newPatch);
}

void KonfytPatchEngine::loadPatch(Patch *patch)
{
    bool patchIsNew = false;
    if ( !mPatches.contains(patch) ) {
        patchIsNew = true;
        mPatches.append(patch);
    }

    // SFZ layers
    foreach (PatchLayerPtr layer, patch->getPluginLayerList()) {
        // If layer indexInEngine is -1, the layer hasn't been loaded yet.
        if (patchIsNew) { layer->sfzData.indexInEngine = -1; }
        if (layer->sfzData.indexInEngine == -1) {
            loadSfzLayer(layer);
            // Gain, solo, mute and bus are set later
        }
    }

    // Fluidsynth layers
    foreach (PatchLayerPtr layer, patch->getSfLayerList()) {
        if (patchIsNew) { layer->soundfontData.synthInEngine = nullptr; }
        if ( layer->soundfontData.synthInEngine == nullptr ) {
            loadSoundfontLayer(layer);
            // Gain, solo, mute, bus and routing is done later
        }
    }

    // Audio input ports
    foreach (PatchLayerPtr layer, patch->getAudioInLayerList()) {
        if (layer->audioInPortData.jackRouteLeft == nullptr) {
            // Routes haven't been created yet
            loadAudioInputPort(layer);
        }
    }


    // Midi output ports
    foreach (PatchLayerPtr layer, patch->getMidiOutputLayerList()) {
        if (layer->midiOutputPortData.jackRoute == nullptr) {
            // Route hasn't been created yet
            loadMidiOutputPort(layer);
        }
    }

    // All layers are now loaded. Now set gains, activate routes, etc.
    foreach (PatchLayerPtr layer, patch->layers()) {
        updateLayerRouting(layer);
        updateLayerGain(layer);
        updateLayerPatchMidiFilterInJackEngine(patch, layer);
    }

    // Set layers active based on solo and mute
    if (patch->alwaysActive || (patch == mCurrentPatch)) {
        activatePatchLayerRoutesForSoloMute(patch);
    }
}

PatchLayerPtr KonfytPatchEngine::addSfProgramLayer(
        QString soundfontPath,
        KonfytSoundPreset newProgram)
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, PatchLayerPtr());

    PatchLayerPtr layer = mCurrentPatch->addSfLayer(soundfontPath,
                                                          newProgram);
    updatePatchLayersURIs(mCurrentPatch);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(mCurrentProject->audioBus_getFirstBusId(-1));

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(mCurrentProject->midiInPort_getFirstPortId(-1));

    // reloadPatch() will also take care of applying patch-wide MIDI filter to layer

    reloadPatch();

    return layer;
}

void KonfytPatchEngine::removeLayer(PatchLayerPtr layer)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    removeLayer(mCurrentPatch, layer);
}

void KonfytPatchEngine::removeLayer(Patch *patch, PatchLayerPtr layer)
{
    unloadLayerFromEngines(layer);
    patch->removeLayer(layer);
    updatePatchLayersURIs(mCurrentPatch);
    loadPatch(patch); // To refresh solo and mute
}

void KonfytPatchEngine::moveLayer(PatchLayerPtr layer, int newIndex)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    mCurrentPatch->moveLayer(layer, newIndex);
    updatePatchLayersURIs(mCurrentPatch);
}

void KonfytPatchEngine::addLayer(PatchLayerPtr layer)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    mCurrentPatch->addLayer(layer);
    updatePatchLayersURIs(mCurrentPatch);

    // reloadPatch() will also take care of applying patch-wide MIDI filter to layer

    reloadPatch();
}

void KonfytPatchEngine::unloadLayerFromEngines(PatchLayerPtr layer)
{
    if (layer->layerType() == PatchLayer::TypeSoundfontProgram) {
        if (layer->soundfontData.synthInEngine) {
            // First remove from JACK engine
            if (layer->soundfontData.portsInJackEngine) {
                jack->removeSoundfont(layer->soundfontData.portsInJackEngine);
                layer->soundfontData.portsInJackEngine = nullptr;
            }
            // Then from fluidsynthEngine
            fluidsynthEngine.removeSoundfontProgram(layer->soundfontData.synthInEngine);
            scriptEngine->removeLayerScript(layer);
            // Set unloaded in patch
            layer->soundfontData.synthInEngine = nullptr;
        }
    } else if (layer->layerType() == PatchLayer::TypeSfz) {
        if (layer->sfzData.indexInEngine >= 0) {
            // First remove from JACK
            if (layer->sfzData.portsInJackEngine) {
                jack->removePlugin(layer->sfzData.portsInJackEngine);
                layer->sfzData.portsInJackEngine = nullptr;
            }
            // Then from SFZ engine
            sfzEngine->removeSfz(layer->sfzData.indexInEngine);
            scriptEngine->removeLayerScript(layer);
            // Set unloaded in patch
            layer->sfzData.indexInEngine = -1;
        }
    } else if (layer->layerType() == PatchLayer::TypeMidiOut) {
        if (layer->midiOutputPortData.jackRoute) {
            jack->removeMidiRoute(layer->midiOutputPortData.jackRoute);
            scriptEngine->removeLayerScript(layer);
            layer->midiOutputPortData.jackRoute = nullptr;
        }
    } else if (layer->layerType() == PatchLayer::TypeAudioIn) {
        if (layer->audioInPortData.jackRouteLeft) {
            jack->removeAudioRoute(layer->audioInPortData.jackRouteLeft);
            layer->audioInPortData.jackRouteLeft = nullptr;
        }
        if (layer->audioInPortData.jackRouteRight) {
            jack->removeAudioRoute(layer->audioInPortData.jackRouteRight);
            layer->audioInPortData.jackRouteRight = nullptr;
        }
    }

    emit patchLayerUnloaded(layer);
}

void KonfytPatchEngine::reloadLayer(PatchLayerPtr layer)
{
    unloadLayerFromEngines(layer);
    reloadPatch();
}

bool KonfytPatchEngine::isPatchLoaded(Patch *patch)
{
    return mPatches.contains(patch);
}

PatchLayerPtr KonfytPatchEngine::addSfzLayer(QString path)
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, PatchLayerPtr());

    PatchLayer::SfzData plugin;
    plugin.path = path;

    // Add the plugin to the patch
    PatchLayerPtr layer = mCurrentPatch->addPlugin(plugin, "sfz");
    updatePatchLayersURIs(mCurrentPatch);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(mCurrentProject->audioBus_getFirstBusId(-1));

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(mCurrentProject->midiInPort_getFirstPortId(-1));

    // reloadPatch() will also take care of applying patch-wide MIDI filter to layer

    reloadPatch();

    return layer;
}

void KonfytPatchEngine::updateLayerRouting(PatchLayerPtr layer)
{
    if (layer->hasError()) { return; }

    PatchLayer::LayerType layerType = layer->layerType();

    Project::AudioPortPtr bus;
    if ( (layerType == PatchLayer::TypeSoundfontProgram) ||
         (layerType == PatchLayer::TypeSfz) ||
         (layerType == PatchLayer::TypeAudioIn) )
    {
        if ( !mCurrentProject->audioBus_exists(layer->busIdInProject()) ) {
            // Invalid bus. Default to the first one.
            print(QString("WARNING: Layer %1 invalid bus %2")
                  .arg(layer->name()).arg(layer->busIdInProject()));

            QList<int> busList = mCurrentProject->audioBus_getAllBusIds();
            KONFYT_ASSERT(busList.count());
            if (busList.count()) {
                bus = mCurrentProject->audioBus_getBus(busList[0]);
                print("   Defaulting to bus " + n2s(busList[0]));
            }
        } else {
            bus = mCurrentProject->audioBus_getBus(layer->busIdInProject());
        }

        if (!bus) {
            print(QString("Error: updateLayerRouting: bus null for layer %1")
                  .arg(layer->uri));
            return;
        }
    }
    Project::MidiPortPtr midiInPort;
    if ( (layerType == PatchLayer::TypeSoundfontProgram) ||
         (layerType == PatchLayer::TypeSfz) ||
         (layerType == PatchLayer::TypeMidiOut) )
    {
        if ( !mCurrentProject->midiInPort_exists(layer->midiInPortIdInProject()) ) {
            // Invalid Midi in port. Default to first one.
            print(QString("WARNING: Layer %1 invalid MIDI input port %2")
                  .arg(layer->name()).arg(layer->midiInPortIdInProject()));

            QList<int> midiInPortList = mCurrentProject->midiInPort_getAllPortIds();
            KONFYT_ASSERT(midiInPortList.count());
            if (midiInPortList.count()) {
                midiInPort = mCurrentProject->midiInPort_getPort(midiInPortList[0]);
                print("   Defaulting to port " + n2s(midiInPortList[0]));
            }
        } else {
            midiInPort = mCurrentProject->midiInPort_getPort(layer->midiInPortIdInProject());
        }

        if (!midiInPort) {
            print(QString("Error: updateLayerRouting: midiInPort null for layer %1")
                  .arg(layer->uri));
            return;
        }
    }

    if (layerType ==  PatchLayer::TypeSoundfontProgram) {

        PatchLayer::SoundfontData sfData = layer->soundfontData;
        jack->setSoundfontRouting( sfData.portsInJackEngine, midiInPort->jackPort,
                                   bus->leftJackPort, bus->rightJackPort );

    } else if (layerType == PatchLayer::TypeSfz) {

        PatchLayer::SfzData pluginData = layer->sfzData;
        jack->setPluginRouting(pluginData.portsInJackEngine,
                               midiInPort->jackPort,
                               bus->leftJackPort, bus->rightJackPort);

    } else if (layerType == PatchLayer::TypeMidiOut) {

        PatchLayer::MidiOutData portData = layer->midiOutputPortData;
        if (mCurrentProject->midiOutPort_exists(portData.portIdInProject)) {

            Project::MidiPortPtr prjMidiOutPort =
                mCurrentProject->midiOutPort_getPort( portData.portIdInProject );
            if (prjMidiOutPort) {
                jack->setMidiRoute(portData.jackRoute, midiInPort->jackPort,
                               prjMidiOutPort->jackPort);
            } else {
                print(QString("Error: updateLayerRouting: midiOutPort null for id %1")
                      .arg(portData.portIdInProject));
            }

        } else {
            print(QString("WARNING: Layer %1 invalid MIDI out port %2")
                  .arg(layer->name()).arg(portData.portIdInProject));
        }

    } else if (layerType == PatchLayer::TypeAudioIn) {

        // The port number in audioInLayerStruct refers to a stereo port pair index in the project.
        // The bus number in audioInLayerStruct refers to a bus in the project with a left and right jack port.
        // We have to retrieve the port pair and bus from the project in order to get the left and right port Jack port numbers.
        PatchLayer::AudioInData audioPortData = layer->audioInPortData;
        if (mCurrentProject->audioInPort_exists(audioPortData.portIdInProject)) {

            Project::AudioPortPtr portPair =
                    mCurrentProject->audioInPort_getPort(
                        audioPortData.portIdInProject);
            if (portPair) {
                // Left channel Bus routing
                jack->setAudioRoute(audioPortData.jackRouteLeft,
                                    portPair->leftJackPort,
                                    bus->leftJackPort);
                // Right channel Bus routing
                jack->setAudioRoute(audioPortData.jackRouteRight,
                                    portPair->rightJackPort,
                                    bus->rightJackPort);
            } else {
                print(QString("Error: updateLayerRouting: audioInPort null for id %1")
                      .arg(audioPortData.portIdInProject));
            }

        } else {
            print(QString("WARNING: Layer %1 invalid audio input port %2")
                  .arg(layer->name()).arg(audioPortData.portIdInProject));
        }

    } else if (layerType == PatchLayer::TypeUninitialized) {

        KONFYT_ASSERT_FAIL("Layer type uninitialized.");

    } else {

        KONFYT_ASSERT_FAIL("Unknown layer type.");

    }
}

void KonfytPatchEngine::updateLayerGain(PatchLayerPtr layer)
{
    if (layer->hasError()) { return; }

    PatchLayer::LayerType layerType = layer->layerType();

    if (layerType ==  PatchLayer::TypeSoundfontProgram) {

        PatchLayer::SoundfontData sfData = layer->soundfontData;
        if (sfData.synthInEngine == nullptr) { return; } // Layer not loaded yet
        fluidsynthEngine.setGain( sfData.synthInEngine, konfytConvertGain(layer->gain()) );

    } else if (layerType == PatchLayer::TypeSfz) {

        PatchLayer::SfzData pluginData = layer->sfzData;
        if (pluginData.indexInEngine == -1) { return; } // Layer not loaded yet
        // Set gain of JACK ports instead of sfzEngine->setGain() since this
        // isn't implemented for all engine types yet.
        jack->setPluginGain(pluginData.portsInJackEngine, konfytConvertGain(layer->gain()) );

    } else if (layerType == PatchLayer::TypeAudioIn) {

        PatchLayer::AudioInData audioPortData = layer->audioInPortData;
        if (audioPortData.jackRouteLeft == nullptr) { return; } // Layer not loaded yet
        // Left channel Gain
        jack->setAudioRouteGain(audioPortData.jackRouteLeft,
                                konfytConvertGain(layer->gain()));
        // Right channel Gain
        jack->setAudioRouteGain(audioPortData.jackRouteRight,
                                konfytConvertGain(layer->gain()));

    } else if (layerType == PatchLayer::TypeMidiOut) {

        // Ignore

    } else if (layerType == PatchLayer::TypeUninitialized) {

        KONFYT_ASSERT_FAIL("Layer type uninitialized.");

    } else {

        KONFYT_ASSERT_FAIL("Unknown layer type.");

    }
}

void KonfytPatchEngine::activatePatchLayerRoutesForSoloMute(Patch *patch)
{
    if (patch == nullptr) { return; }

    QList<PatchLayerPtr> layerList = patch->layers();

    // Determine solo
    bool solo = false;
    foreach (PatchLayerPtr layer, layerList) {
        if (layer->isSolo()) {
            solo = true;
            break;
        }
    }

    // Activate/deactivate routing based on solo/mute
    foreach (PatchLayerPtr layer, layerList) {

        if (layer->hasError()) { continue; }

        bool activate = false;
        if (solo && layer->isSolo()) { activate = true; }
        if (!solo) { activate = true; }
        if (layer->isMute()) { activate = false; }

        setLayerActive(layer, activate);
    }
}

void KonfytPatchEngine::setLayerActive(PatchLayerPtr layer, bool active)
{
    if (layer->hasError()) { return; }

    PatchLayer::LayerType layerType = layer->layerType();

    // Activate route(s) in JACK engine
    if (layerType ==  PatchLayer::TypeSoundfontProgram) {

        jack->setSoundfontActive(layer->soundfontData.portsInJackEngine, active);

    } else if (layerType == PatchLayer::TypeSfz) {

        jack->setPluginActive(layer->sfzData.portsInJackEngine, active);

    } else if (layerType == PatchLayer::TypeMidiOut) {

        jack->setMidiRouteActive(layer->midiOutputPortData.jackRoute, active);

    } else if (layerType == PatchLayer::TypeAudioIn) {

        jack->setAudioRouteActive(layer->audioInPortData.jackRouteLeft, active);
        jack->setAudioRouteActive(layer->audioInPortData.jackRouteRight, active);

    } else if (layerType == PatchLayer::TypeUninitialized) {

        KONFYT_ASSERT_FAIL("Layer type uninitialized.");

    } else {

        KONFYT_ASSERT_FAIL("Unknown layer type.");

    }
}

void KonfytPatchEngine::updateLayerPatchMidiFilterInJackEngine(
        Patch *patch, PatchLayerPtr layer)
{
    KONFYT_ASSERT_RETURN(patch != nullptr);
    KONFYT_ASSERT_RETURN(!layer.isNull());

    if (layer->hasError()) { return; }

    if (layer->layerType() == PatchLayer::TypeSoundfontProgram) {

        jack->setSoundfontMidiPreFilter(layer->soundfontData.portsInJackEngine,
                                        patch->patchMidiFilter);

    } else if (layer->layerType() == PatchLayer::TypeSfz) {

        jack->setPluginMidiPreFilter(layer->sfzData.portsInJackEngine,
                                     patch->patchMidiFilter);

    } else if (layer->layerType() == PatchLayer::TypeMidiOut) {

        jack->setRouteMidiPreFilter(layer->midiOutputPortData.jackRoute,
                                    patch->patchMidiFilter);

    } else {
        // Ignore for other layers, but do not throw error.
    }
}

void KonfytPatchEngine::updateLayerBlockMidiDirectThroughInJack(PatchLayerPtr patchLayer)
{
    bool block = !patchLayer->isPassMidiThrough();

    if (patchLayer->layerType() == PatchLayer::TypeSoundfontProgram) {

        jack->setSoundfontBlockMidiDirectThrough(
                    patchLayer->soundfontData.portsInJackEngine, block);

    } else if (patchLayer->layerType() == PatchLayer::TypeSfz) {

        jack->setPluginBlockMidiDirectThrough(
                    patchLayer->sfzData.portsInJackEngine, block);

    } else if (patchLayer->layerType() == PatchLayer::TypeMidiOut) {

        jack->setRouteBlockMidiDirectThrough(
                    patchLayer->midiOutputPortData.jackRoute, block);

    }
}

void KonfytPatchEngine::setupAndInitFluidsynthEngine()
{
    connect(&fluidsynthEngine, &KonfytFluidsynthEngine::print,
            this, [=](QString msg)
    {
        print("Fluidsynth: " + msg);
    });
    fluidsynthEngine.initFluidsynth(jack->getSampleRate());
    jack->setFluidsynthEngine(&fluidsynthEngine);
}

void KonfytPatchEngine::setupAndInitSfzEngine(KonfytAppInfo appInfo)
{
    if (appInfo.bridge) {
        // Create bridge engine (each plugin is hosted in new Konfyt process)
        sfzEngine = new KonfytBridgeEngine();
        static_cast<KonfytBridgeEngine*>(sfzEngine)->setKonfytExePath(
                    appInfo.exePath);
    }
#ifdef KONFYT_USE_CARLA
    else if (appInfo.carla) {
        // Use local Carla engine
        sfzEngine = new KonfytCarlaEngine();
    }
#endif
    else {
        // Use Linuxsampler via LSCP
        sfzEngine = new KonfytLscpEngine();
    }

    connect(sfzEngine, &KonfytBaseSoundEngine::print, this, [=](QString msg)
    {
        print(sfzEngine->engineName() + ": " + msg);
    });
    connect(sfzEngine, &KonfytBaseSoundEngine::statusInfo,
            this, &KonfytPatchEngine::statusInfo);
    connect(sfzEngine, &KonfytBaseSoundEngine::initDone,
            this, &KonfytPatchEngine::onSfzEngineInitDone);
    connect(sfzEngine, &KonfytBaseSoundEngine::errorStringChanged,
            this, &KonfytPatchEngine::sfzEngineErrorStringChanged);

    sfzEngine->initEngine(jack);
}

/* Update layer URIs in specified patch.
 * This must be called if some action caused the patch and/or its layers indices
 * to change, i.e. insert, remove and move of patches or layers. */
void KonfytPatchEngine::updatePatchLayersURIs(Patch *patch)
{
    KONFYT_ASSERT_RETURN(patch != nullptr);
    if (!mCurrentProject) { return; }

    int ipatch = mCurrentProject->getPatchIndex(patch);
    for (int i=0; i < patch->layerCount(); i++) {
        PatchLayerPtr layer = patch->layer(i);
        layer->uri = QString("P%1L%2").arg(ipatch+1).arg(i+1);
    }
}

/* Return the current patch. */
Patch *KonfytPatchEngine::currentPatch()
{
    return mCurrentPatch;
}

void KonfytPatchEngine::setPatchFilter(Patch *patch, MidiFilter filter)
{
    KONFYT_ASSERT_RETURN(patch != nullptr);
    KONFYT_ASSERT_RETURN(mPatches.contains(patch));

    patch->patchMidiFilter = filter;
    foreach (PatchLayerPtr layer, patch->layers()) {
        updateLayerPatchMidiFilterInJackEngine(patch, layer);
    }
}

void KonfytPatchEngine::setMidiPickupRange(int range)
{
    mMidiPickupRange = range;
}

void KonfytPatchEngine::setLayerGain(PatchLayerPtr patchLayer, float newGain)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);
    KONFYT_ASSERT_RETURN(!patchLayer.isNull());

    patchLayer->setGain(newGain);

    updateLayerGain(patchLayer);
}

void KonfytPatchEngine::setLayerGain(int layerIndex, float newGain)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    setLayerGain( mCurrentPatch->layer(layerIndex), newGain );
}

void KonfytPatchEngine::setLayerGainByMidi(int layerIndex, int midiValue)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    PatchLayerPtr patchLayer = mCurrentPatch->layer(layerIndex);
    KONFYT_ASSERT_RETURN(!patchLayer.isNull());

    patchLayer->setGainMidiPickupRange(mMidiPickupRange);
    patchLayer->setGainByMidi(midiValue);
    updateLayerGain(patchLayer);
}

void KonfytPatchEngine::setLayerGainByMidiRelative(int layerIndex, int midiValue)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    PatchLayerPtr patchLayer = mCurrentPatch->layer(layerIndex);
    KONFYT_ASSERT_RETURN(!patchLayer.isNull());

    patchLayer->addGainRelativeMidiValue(midiValue);
    updateLayerGain(patchLayer);
}

void KonfytPatchEngine::setLayerSolo(PatchLayerPtr patchLayer, bool solo)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);
    KONFYT_ASSERT_RETURN(!patchLayer.isNull());

    patchLayer->setSolo(solo);
    activatePatchLayerRoutesForSoloMute(mCurrentPatch);
}

/* Set layer solo, using layer index as parameter. */
void KonfytPatchEngine::setLayerSolo(int layerIndex, bool solo)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    setLayerSolo( mCurrentPatch->layer(layerIndex), solo );
}

void KonfytPatchEngine::setLayerMute(PatchLayerPtr patchLayer, bool mute)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);
    KONFYT_ASSERT_RETURN(!patchLayer.isNull());

    patchLayer->setMute(mute);
    activatePatchLayerRoutesForSoloMute(mCurrentPatch);
}

/* Set layer mute, using layer index as parameter. */
void KonfytPatchEngine::setLayerMute(int layerIndex, bool mute)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    setLayerMute( mCurrentPatch->layer(layerIndex), mute );
}

void KonfytPatchEngine::setLayerBus(PatchLayerPtr patchLayer, int bus)
{
    patchLayer->setBusIdInProject(bus);
    updateLayerRouting(patchLayer);
}

void KonfytPatchEngine::setLayerMidiInPort(PatchLayerPtr patchLayer, int portId)
{
    patchLayer->setMidiInPortIdInProject(portId);
    updateLayerRouting(patchLayer);
}

void KonfytPatchEngine::setLayerScript(PatchLayerPtr patchLayer, QString script)
{
    patchLayer->setScript(script);
    scriptEngine->addOrUpdateLayerScript(patchLayer);
}

void KonfytPatchEngine::setLayerScriptEnabled(PatchLayerPtr patchLayer, bool enable)
{
    patchLayer->setScriptEnabled(enable);
    scriptEngine->setScriptEnabled(patchLayer, enable);
}

void KonfytPatchEngine::setLayerPassMidiThrough(PatchLayerPtr patchLayer, bool pass)
{
    patchLayer->setPassMidiThrough(pass);
    updateLayerBlockMidiDirectThroughInJack(patchLayer);
}

void KonfytPatchEngine::sendCurrentPatchMidi()
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    foreach (PatchLayerPtr layer, mCurrentPatch->getMidiOutputLayerList()) {
        sendLayerMidi(layer);
    }
}

void KonfytPatchEngine::sendLayerMidi(PatchLayerPtr patchLayer)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    if (patchLayer->hasError()) { return; }
    if (patchLayer->layerType() == PatchLayer::TypeMidiOut) {
        jack->sendMidiEventsOnRoute(patchLayer->midiOutputPortData.jackRoute,
                                    patchLayer->getMidiSendListEvents());
    }
}

int KonfytPatchEngine::getNumLayers() const
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, 0);

    return mCurrentPatch->layerCount();
}

void KonfytPatchEngine::setLayerFilter(PatchLayerPtr patchLayer, MidiFilter filter)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    patchLayer->setMidiFilter(filter);

    // And also in the respective engine.
    if (patchLayer->layerType() == PatchLayer::TypeSoundfontProgram) {

        jack->setSoundfontMidiFilter(patchLayer->soundfontData.portsInJackEngine, filter);

    } else if (patchLayer->layerType() == PatchLayer::TypeSfz) {

        jack->setPluginMidiFilter(patchLayer->sfzData.portsInJackEngine, filter);

    } else if (patchLayer->layerType() == PatchLayer::TypeMidiOut) {

        jack->setRouteMidiFilter(patchLayer->midiOutputPortData.jackRoute, filter);

    }
}

PatchLayerPtr KonfytPatchEngine::addMidiOutPortToPatch(int port)
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, PatchLayerPtr());

    PatchLayerPtr layer = mCurrentPatch->addMidiOutputPort(port);
    updatePatchLayersURIs(mCurrentPatch);

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(mCurrentProject->midiInPort_getFirstPortId(-1));

    // reloadPatch() will also take care of applying patch-wide MIDI filter to layer

    reloadPatch();

    return layer;
}

PatchLayerPtr KonfytPatchEngine::addAudioInPortToPatch(int port)
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, PatchLayerPtr());

    PatchLayerPtr layer = mCurrentPatch->addAudioInPort(port);
    updatePatchLayersURIs(mCurrentPatch);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(mCurrentProject->audioBus_getFirstBusId(-1));

    reloadPatch();

    return layer;
}

void KonfytPatchEngine::loadSfzLayer(PatchLayerPtr layer)
{
    // Load in SFZ engine
    int ID = sfzEngine->addSfz( layer->sfzData.path );

    if (ID < 0) {
        layer->setErrorMessage("Failed to load SFZ: " + layer->sfzData.path);
        return;
    }

    layer->setErrorMessage("");

    layer->sfzData.indexInEngine = ID;
    layer->setName(sfzEngine->pluginName(ID));
    // Add to JACK engine

    // Find the plugin midi input port
    layer->sfzData.midiInPort = sfzEngine->midiInJackPortName(ID);

    // Find the plugin audio output ports
    QStringList audioLR = sfzEngine->audioOutJackPortNames(ID);
    layer->sfzData.audioOutPortLeft = audioLR[0];
    layer->sfzData.audioOutPortRight = audioLR[1];

    // The plugin object now contains the midi input port and
    // audio left and right output ports. Give this to JACK, which will:
    // - create a midi output port and connect it to the plugin midi input port,
    // - create audio input ports and connect it to the plugin audio output ports.
    // - assign the midi filter
    KonfytJackPortsSpec spec;
    spec.name = layer->name();
    spec.midiOutConnectTo = layer->sfzData.midiInPort;
    spec.midiFilter = layer->midiFilter();
    spec.audioInLeftConnectTo = layer->sfzData.audioOutPortLeft;
    spec.audioInRightConnectTo = layer->sfzData.audioOutPortRight;
    KfJackPluginPorts* jackPorts = jack->addPluginPortsAndConnect( spec );
    layer->sfzData.portsInJackEngine = jackPorts;

    // Add to script engine
    scriptEngine->addOrUpdateLayerScript(layer);
    updateLayerBlockMidiDirectThroughInJack(layer);

    emit patchLayerLoaded(layer); // TODO 2023-10-06 Why only for SFZ layers?
}

void KonfytPatchEngine::loadSoundfontLayer(PatchLayerPtr layer)
{
    // Load in Fluidsynth engine
    layer->soundfontData.synthInEngine =
            fluidsynthEngine.addSoundfontProgram(
                layer->soundfontData.soundfontFilePath,
                layer->soundfontData.program);
    if (!layer->soundfontData.synthInEngine) {
        layer->setErrorMessage("Failed to load soundfont.");
        return;
    }

    // Add to Jack engine (this also assigns the midi filter)
    KfJackPluginPorts* jackPorts = jack->addSoundfont(
                layer->soundfontData.synthInEngine);
    layer->soundfontData.portsInJackEngine = jackPorts;
    jack->setSoundfontMidiFilter(jackPorts, layer->midiFilter());

    // Add to script engine
    scriptEngine->addOrUpdateLayerScript(layer);
    updateLayerBlockMidiDirectThroughInJack(layer);
}

void KonfytPatchEngine::loadAudioInputPort(PatchLayerPtr layer)
{
    // Get source ports
    int portId = layer->audioInPortData.portIdInProject;
    if (!mCurrentProject->audioInPort_exists(portId)) {
        layer->setErrorMessage("No audio-in port in project: " + n2s(portId));
        print("loadPatch: " + layer->errorMessage());
        return;
    }
    Project::AudioPortPtr srcPorts = mCurrentProject->audioInPort_getPort(portId);
    KONFYT_ASSERT_RETURN(!srcPorts.isNull());

    // Get destination ports (bus)
    int busId = layer->busIdInProject();
    if (!mCurrentProject->audioBus_exists(busId)) {
        layer->setErrorMessage("No bus in project: " + n2s(busId));
        print("loadPatch: " + layer->errorMessage());
        return;
    }
    Project::AudioPortPtr destPorts = mCurrentProject->audioBus_getBus(busId);
    KONFYT_ASSERT_RETURN(!destPorts.isNull());

    // Route for left port
    layer->audioInPortData.jackRouteLeft = jack->addAudioRoute(
                srcPorts->leftJackPort, destPorts->leftJackPort);
    // Route for right port
    layer->audioInPortData.jackRouteRight = jack->addAudioRoute(
                srcPorts->rightJackPort, destPorts->rightJackPort);
}

void KonfytPatchEngine::loadMidiOutputPort(PatchLayerPtr layer)
{
    // Get source port
    int srcId = layer->midiInPortIdInProject();
    if (!mCurrentProject->midiInPort_exists(srcId)) {
        layer->setErrorMessage("No MIDI-in port in project: " + n2s(srcId));
        print("loadPatch: " + layer->errorMessage());
        return;
    }
    Project::MidiPortPtr srcPort = mCurrentProject->midiInPort_getPort(srcId);
    KONFYT_ASSERT_RETURN(!srcPort.isNull());

    // Get destination port
    int destId = layer->midiOutputPortData.portIdInProject;
    if (!mCurrentProject->midiOutPort_exists(destId)) {
        layer->setErrorMessage("No MIDI-out port in project: " + n2s(destId));
        print("loadPatch: " + layer->errorMessage());
        return;
    }
    Project::MidiPortPtr destPort = mCurrentProject->midiOutPort_getPort(destId);
    KONFYT_ASSERT_RETURN(!destPort.isNull());

    // Create route
    layer->midiOutputPortData.jackRoute = jack->addMidiRoute(
                srcPort->jackPort, destPort->jackPort);

    // Set MIDI Filter
    if (layer->midiOutputPortData.jackRoute) {
        jack->setRouteMidiFilter(layer->midiOutputPortData.jackRoute,
                                 layer->midiFilter());
    }

    // Add to script engine
    scriptEngine->addOrUpdateLayerScript(layer);
    updateLayerBlockMidiDirectThroughInJack(layer);
}

void KonfytPatchEngine::onSfzEngineInitDone(QString error)
{
    if (error.isEmpty()) {
        print("SFZ engine initialised successfully.");

        // Set all patches SFZ layers as not loaded yet, and load all patches.
        foreach (Patch* patch, mPatches) {
            foreach (PatchLayerPtr layer, patch->getPluginLayerList()) {
                // Unload layer but do not try to remove from sfz engine
                if (layer->sfzData.portsInJackEngine) {
                    jack->removePlugin(layer->sfzData.portsInJackEngine);
                    layer->sfzData.portsInJackEngine = nullptr;
                }
                layer->sfzData.indexInEngine = -1;
            }
            loadPatch(patch);
        }

    } else {
        print("SFZ engine initialisation error: " + error);
    }
    emit sfzEngineErrorStringChanged(error);
}

void KonfytPatchEngine::onProjectPatchURIsNeedUdating()
{
    if (!mCurrentProject) { return; }

    foreach (Patch* patch, mCurrentProject->getPatchList()) {
        updatePatchLayersURIs(patch);
    }
}

void KonfytPatchEngine::onProjectModifiedStateChanged(bool modified)
{
    if (!modified) {
        // Modified == false means project was saved. Create new patch reset
        // snapshots.
        foreach (Patch* patch, mCurrentProject->getPatchList()) {
            patch->createLayerResetSnapshots();
        }
    }
}

