/******************************************************************************
 *
 * Copyright 2021 Gideon van der Kolf
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

#include <iostream>
#include <math.h>

KonfytPatchEngine::KonfytPatchEngine(QObject *parent) :
    QObject(parent)
{
}

KonfytPatchEngine::~KonfytPatchEngine()
{
    delete sfzEngine;
}

void KonfytPatchEngine::initPatchEngine(KonfytJackEngine* newJackClient,
                                        KonfytAppInfo appInfo)
{
    // Jack client (probably received from MainWindow) so we can directly create ports
    this->jack = newJackClient;

    // Fluidsynth Engine
    connect(&fluidsynthEngine, &KonfytFluidsynthEngine::print, [=](QString msg){
        print("Fluidsynth: " + msg);
    });
    fluidsynthEngine.initFluidsynth(jack->getSampleRate());
    jack->setFluidsynthEngine(&fluidsynthEngine);

    // Initialise SFZ Backend

    bridge = appInfo.bridge;
    if (bridge) {
        // Create bridge engine (each plugin is hosted in new Konfyt process)
        sfzEngine = new KonfytBridgeEngine();
        static_cast<KonfytBridgeEngine*>(sfzEngine)->setKonfytExePath(appInfo.exePath);
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

    connect(sfzEngine, &KonfytBaseSoundEngine::print, [=](QString msg){
        print(sfzEngine->engineName() + ": " + msg);
    });
    connect(sfzEngine, &KonfytBaseSoundEngine::statusInfo,
            this, &KonfytPatchEngine::statusInfo);
    connect(sfzEngine, &KonfytBaseSoundEngine::initDone,
            this, &KonfytPatchEngine::onSfzEngineInitDone);

    sfzEngine->initEngine(jack);
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
}

/* Reload current patch (e.g. if patch changed). */
void KonfytPatchEngine::reloadPatch()
{
    loadPatch(mCurrentPatch);
}

/* Ensure patch and all layers are unloaded from their respective engines. */
void KonfytPatchEngine::unloadPatch(KonfytPatch *patch)
{
    if ( !patches.contains(patch) ) { return; }

    foreach (KfPatchLayerWeakPtr layer, patch->layers()) {
        unloadLayer(layer);
    }

    patches.removeAll(patch);
    if (mCurrentPatch == patch) { mCurrentPatch = nullptr; }
}

/* Load patch, replacing the current patch. */
bool KonfytPatchEngine::loadPatch(KonfytPatch *newPatch)
{
    if (newPatch == nullptr) { return false; }

    bool ret = true; // return value. Set to false when an error occured somewhere.

    // Deactivate routes for current patch
    if (newPatch != mCurrentPatch) {
        if (mCurrentPatch != nullptr) {
            if (!mCurrentPatch->alwaysActive) {

                foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->layers()) {
                    setLayerActive(layer, false);
                }

            }
        }
    }

    bool patchIsNew = false;
    if ( !patches.contains(newPatch) ) {
        patchIsNew = true;
        patches.append(newPatch);
    }
    mCurrentPatch = newPatch;

    // ---------------------------------------------------
    // SFZ layers:
    // ---------------------------------------------------

    // For each SFZ layer in the patch...
    foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->getPluginLayerList()) {
        // If layer indexInEngine is -1, the layer hasn't been loaded yet.
        if (patchIsNew) { layer->sfzData.indexInEngine = -1; }
        if (layer->sfzData.indexInEngine == -1) {

            // Load in SFZ engine
            int ID = sfzEngine->addSfz( layer->sfzData.path );

            if (ID < 0) {
                layer->setErrorMessage("Failed to load SFZ: " + layer->sfzData.path);
                ret = false;
            } else {
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
                // - assigns the midi filter
                KonfytJackPortsSpec spec;
                spec.name = layer->name();
                spec.midiOutConnectTo = layer->sfzData.midiInPort;
                spec.midiFilter = layer->midiFilter();
                spec.audioInLeftConnectTo = layer->sfzData.audioOutPortLeft;
                spec.audioInRightConnectTo = layer->sfzData.audioOutPortRight;
                KfJackPluginPorts* jackPorts = jack->addPluginPortsAndConnect( spec );
                layer->sfzData.portsInJackEngine = jackPorts;

                emit patchLayerLoaded(layer);

                // Gain, solo, mute and bus are set later in refershAllGainsAndRouting()
            }
        }
    }

    // ---------------------------------------------------
    // Fluidsynth layers:
    // ---------------------------------------------------

    // For each soundfont program in the patch...
    foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->getSfLayerList()) {
        if (patchIsNew) { layer->soundfontData.synthInEngine = nullptr; }
        if ( layer->soundfontData.synthInEngine == nullptr ) {
            // Load in Fluidsynth engine
            layer->soundfontData.synthInEngine =
                    fluidsynthEngine.addSoundfontProgram(
                        layer->soundfontData.parentSoundfont,
                        layer->soundfontData.program);
            if (layer->soundfontData.synthInEngine) {
                // Add to Jack engine (this also assigns the midi filter)
                KfJackPluginPorts* jackPorts = jack->addSoundfont(
                            layer->soundfontData.synthInEngine);
                layer->soundfontData.portsInJackEngine = jackPorts;
                jack->setSoundfontMidiFilter(jackPorts, layer->midiFilter());
                // Gain, solo, mute, bus and routing is done later
            } else {
                layer->setErrorMessage("Failed to load soundfont.");
                ret = false;
            }
        }
    }

    // ---------------------------------------------------
    // Audio input ports:
    // ---------------------------------------------------

    foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->getAudioInLayerList()) {
        if (layer->audioInPortData.jackRouteLeft == nullptr) {
            // Routes haven't been created yet

            // Get source ports
            int portId = layer->audioInPortData.portIdInProject;
            if (!mCurrentProject->audioInPort_exists(portId)) {
                layer->setErrorMessage("No audio-in port in project: " + n2s(portId));
                print("loadPatch: " + layer->errorMessage());
                continue;
            }
            PrjAudioInPort srcPorts = mCurrentProject->audioInPort_getPort(portId);

            // Get destination ports (bus)
            if (!mCurrentProject->audioBus_exists(layer->busIdInProject())) {
                layer->setErrorMessage("No bus in project: " + n2s(portId));
                print("loadPatch: " + layer->errorMessage());
                continue;
            }
            PrjAudioBus destPorts = mCurrentProject->audioBus_getBus(layer->busIdInProject());

            // Route for left port
            layer->audioInPortData.jackRouteLeft = jack->addAudioRoute(
                        srcPorts.leftJackPort, destPorts.leftJackPort);
            // Route for right port
            layer->audioInPortData.jackRouteRight = jack->addAudioRoute(
                        srcPorts.rightJackPort, destPorts.rightJackPort);
        }
    }


    // ---------------------------------------------------
    // Midi output ports:
    // ---------------------------------------------------

    foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->getMidiOutputLayerList()) {
        if (layer->midiOutputPortData.jackRoute == nullptr) {
            // Route hasn't been created yet

            // Get source port
            if (!mCurrentProject->midiInPort_exists(layer->midiInPortIdInProject())) {
                layer->setErrorMessage("No MIDI-in port in project: " + n2s(layer->midiInPortIdInProject()));
                print("loadPatch: " + layer->errorMessage());
                continue;
            }
            PrjMidiPort srcPort = mCurrentProject->midiInPort_getPort(layer->midiInPortIdInProject());

            // Get destination port
            if (!mCurrentProject->midiOutPort_exists(layer->midiOutputPortData.portIdInProject)) {
                layer->setErrorMessage("No MIDI-out port in project: " + n2s(layer->midiOutputPortData.portIdInProject));
                print("loadPatch: " + layer->errorMessage());
                continue;
            }
            PrjMidiPort destPort = mCurrentProject->midiOutPort_getPort(layer->midiOutputPortData.portIdInProject);

            // Create route
            layer->midiOutputPortData.jackRoute = jack->addMidiRoute(
                        srcPort.jackPort, destPort.jackPort);
        }

        // Set MIDI Filter
        jack->setRouteMidiFilter(layer->midiOutputPortData.jackRoute,
                                 layer->midiFilter());
    }

    // ---------------------------------------------------
    // The rest
    // ---------------------------------------------------

    // All layers are now loaded. Now set gains, activate routes, etc.
    foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->layers()) {
        updateLayerRouting(layer);
    }
    updatePatchLayersGain(mCurrentPatch);
    updatePatchLayersSoloMute(mCurrentPatch);

    return ret;
}

KfPatchLayerWeakPtr KonfytPatchEngine::addSfProgramLayer(
        QString soundfontPath,
        KonfytSoundPreset newProgram)
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, KfPatchLayerWeakPtr());

    KfPatchLayerSharedPtr layer = mCurrentPatch->addSfLayer(soundfontPath,
                                                            newProgram).toStrongRef();

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(mCurrentProject->audioBus_getFirstBusId(-1));

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(mCurrentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();

    return layer.toWeakRef();
}

void KonfytPatchEngine::removeLayer(KfPatchLayerWeakPtr layer)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    removeLayer(mCurrentPatch, layer);
}

void KonfytPatchEngine::removeLayer(KonfytPatch *patch, KfPatchLayerWeakPtr layer)
{
    // Unload from respective engine
    unloadLayer(layer);

    patch->removeLayer(layer);

    if (patch == mCurrentPatch) { reloadPatch(); }
}

void KonfytPatchEngine::moveLayer(KfPatchLayerWeakPtr layer, int newIndex)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    mCurrentPatch->moveLayer(layer, newIndex);
}

void KonfytPatchEngine::unloadLayer(KfPatchLayerWeakPtr layer)
{
    KfPatchLayerSharedPtr l = layer.toStrongRef();
    if (l->layerType() == KonfytPatchLayer::TypeSoundfontProgram) {
        if (l->soundfontData.synthInEngine) {
            // First remove from JACK engine
            jack->removeSoundfont(l->soundfontData.portsInJackEngine);
            // Then from fluidsynthEngine
            fluidsynthEngine.removeSoundfontProgram(l->soundfontData.synthInEngine);
            // Set unloaded in patch
            l->soundfontData.synthInEngine = nullptr;
        }
    } else if (l->layerType() == KonfytPatchLayer::TypeSfz) {
        if (l->sfzData.indexInEngine >= 0) {
            // First remove from JACK
            jack->removePlugin(l->sfzData.portsInJackEngine);
            // Then from SFZ engine
            sfzEngine->removeSfz(l->sfzData.indexInEngine);
            // Set unloaded in patch
            l->sfzData.indexInEngine = -1;
        }
    } else if (l->layerType() == KonfytPatchLayer::TypeMidiOut) {
        jack->removeMidiRoute(l->midiOutputPortData.jackRoute);
    } else if (l->layerType() == KonfytPatchLayer::TypeAudioIn) {
        jack->removeAudioRoute(l->audioInPortData.jackRouteLeft);
        jack->removeAudioRoute(l->audioInPortData.jackRouteRight);
    }
}

void KonfytPatchEngine::reloadLayer(KfPatchLayerWeakPtr layer)
{
    unloadLayer(layer);
    reloadPatch();
}

bool KonfytPatchEngine::isPatchLoaded(KonfytPatch *patch)
{
    return patches.contains(patch);
}

KfPatchLayerWeakPtr KonfytPatchEngine::addSfzLayer(QString path)
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, KfPatchLayerWeakPtr());

    LayerSfzData plugin;
    plugin.path = path;

    // Add the plugin to the patch
    KfPatchLayerSharedPtr layer = mCurrentPatch->addPlugin(plugin, "sfz").toStrongRef();

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(mCurrentProject->audioBus_getFirstBusId(-1));

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(mCurrentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();

    return layer.toWeakRef();
}

/* Converts gain between 0 and 1 from linear to an exponential function that is more
 * suited for human hearing. Input is clipped between 0 and 1. */
float KonfytPatchEngine::convertGain(float linearGain)
{
    if (linearGain < 0) { linearGain = 0; }
    if (linearGain > 1) { linearGain = 1; }

    return pow(linearGain, 3.0); // x^3
}

void KonfytPatchEngine::updateLayerRouting(KfPatchLayerSharedPtr layer)
{
    if (layer->hasError()) { return; }

    KonfytPatchLayer::LayerType layerType = layer->layerType();

    PrjAudioBus bus;
    if ( (layerType == KonfytPatchLayer::TypeSoundfontProgram) ||
         (layerType == KonfytPatchLayer::TypeSfz) ||
         (layerType == KonfytPatchLayer::TypeAudioIn) )
    {
        if ( !mCurrentProject->audioBus_exists(layer->busIdInProject()) ) {
            // Invalid bus. Default to the first one.
            print(QString("WARNING: Layer %1 invalid bus %2")
                  .arg(layer->name()).arg(layer->busIdInProject()));

            QList<int> busList = mCurrentProject->audioBus_getAllBusIds();
            if (busList.count()) {
                bus = mCurrentProject->audioBus_getBus(busList[0]);
                print("   Defaulting to bus " + n2s(busList[0]));
            } else {
                error_abort("updateLayerRouting: Project has no buses.");
            }
        } else {
            bus = mCurrentProject->audioBus_getBus(layer->busIdInProject());
        }
    }
    PrjMidiPort midiInPort;
    if ( (layerType == KonfytPatchLayer::TypeSoundfontProgram) ||
         (layerType == KonfytPatchLayer::TypeSfz) ||
         (layerType == KonfytPatchLayer::TypeMidiOut) )
    {
        if ( !mCurrentProject->midiInPort_exists(layer->midiInPortIdInProject()) ) {
            // Invalid Midi in port. Default to first one.
            print(QString("WARNING: Layer %1 invalid MIDI input port %2")
                  .arg(layer->name()).arg(layer->midiInPortIdInProject()));

            QList<int> midiInPortList = mCurrentProject->midiInPort_getAllPortIds();
            if (midiInPortList.count()) {
                midiInPort = mCurrentProject->midiInPort_getPort(midiInPortList[0]);
                print("   Defaulting to port " + n2s(midiInPortList[0]));
            } else {
                error_abort("updateLayerRouting: Project has no MIDI Input Ports.");
            }
        } else {
            midiInPort = mCurrentProject->midiInPort_getPort(layer->midiInPortIdInProject());
        }
    }

    if (layerType ==  KonfytPatchLayer::TypeSoundfontProgram) {

        LayerSoundfontData sfData = layer->soundfontData;
        jack->setSoundfontRouting( sfData.portsInJackEngine, midiInPort.jackPort,
                                   bus.leftJackPort, bus.rightJackPort );

    } else if (layerType == KonfytPatchLayer::TypeSfz) {

        LayerSfzData pluginData = layer->sfzData;
        jack->setPluginRouting(pluginData.portsInJackEngine,
                               midiInPort.jackPort,
                               bus.leftJackPort, bus.rightJackPort);

    } else if (layerType == KonfytPatchLayer::TypeMidiOut) {

        LayerMidiOutData portData = layer->midiOutputPortData;
        if (mCurrentProject->midiOutPort_exists(portData.portIdInProject)) {

            PrjMidiPort prjMidiOutPort = mCurrentProject->midiOutPort_getPort( portData.portIdInProject );
            jack->setMidiRoute(portData.jackRoute, midiInPort.jackPort,
                               prjMidiOutPort.jackPort);

        } else {
            print(QString("WARNING: Layer %1 invalid MIDI out port %2")
                  .arg(layer->name()).arg(portData.portIdInProject));
        }

    } else if (layerType == KonfytPatchLayer::TypeAudioIn) {

        // The port number in audioInLayerStruct refers to a stereo port pair index in the project.
        // The bus number in audioInLayerStruct refers to a bus in the project with a left and right jack port.
        // We have to retrieve the port pair and bus from the project in order to get the left and right port Jack port numbers.
        LayerAudioInData audioPortData = layer->audioInPortData;
        if (mCurrentProject->audioInPort_exists(audioPortData.portIdInProject)) {

            PrjAudioInPort portPair = mCurrentProject->audioInPort_getPort(audioPortData.portIdInProject);
            // Left channel Bus routing
            jack->setAudioRoute(audioPortData.jackRouteLeft,
                                portPair.leftJackPort,
                                bus.leftJackPort);
            // Right channel Bus routing
            jack->setAudioRoute(audioPortData.jackRouteRight,
                                portPair.rightJackPort,
                                bus.rightJackPort);

        } else {
            print(QString("WARNING: Layer %1 invalid audio input port %2")
                  .arg(layer->name()).arg(audioPortData.portIdInProject));
        }

    } else if (layerType == KonfytPatchLayer::TypeUninitialized) {

        error_abort("updateLayerRouting: Layer type uninitialized.");

    } else {

        error_abort("updateLayerRouting: Unknown layer type.");

    }
}

void KonfytPatchEngine::updateLayerGain(KfPatchLayerSharedPtr layer)
{
    if (layer->hasError()) { return; }

    KonfytPatchLayer::LayerType layerType = layer->layerType();

    if (layerType ==  KonfytPatchLayer::TypeSoundfontProgram) {

        LayerSoundfontData sfData = layer->soundfontData;
        if (sfData.synthInEngine == nullptr) { return; } // Layer not loaded yet
        // Gain = layer gain * master gain
        fluidsynthEngine.setGain( sfData.synthInEngine, convertGain(layer->gain()*mMasterGain) );

    } else if (layerType == KonfytPatchLayer::TypeSfz) {

        LayerSfzData pluginData = layer->sfzData;
        if (pluginData.indexInEngine == -1) { return; } // Layer not loaded yet
        // Gain = layer gain * master gain
        // Set gain of JACK ports instead of sfzEngine->setGain() since this isn't implemented for all engine types yet.
        jack->setPluginGain(pluginData.portsInJackEngine, convertGain(layer->gain()*mMasterGain) );

    } else if (layerType == KonfytPatchLayer::TypeAudioIn) {

        // The port number in audioInLayerStruct refers to a stereo port pair index in the project.
        // The bus number in audioInLayerStruct refers to a bus in the project with a left and right jack port.
        // We have to retrieve the port pair and bus from the project in order to get the left and right port Jack port numbers.
        LayerAudioInData audioPortData = layer->audioInPortData;
        if (audioPortData.jackRouteLeft == nullptr) { return; } // Layer not loaded yet
        // Left channel Gain
        jack->setAudioRouteGain(audioPortData.jackRouteLeft,
                                convertGain(layer->gain()*mMasterGain));
        // Right channel Gain
        jack->setAudioRouteGain(audioPortData.jackRouteRight, convertGain(layer->gain()*mMasterGain));

    } else if (layerType == KonfytPatchLayer::TypeMidiOut) {

        // Ignore

    } else if (layerType == KonfytPatchLayer::TypeUninitialized) {

        error_abort("updateLayerGain: Layer type uninitialized.");

    } else {

        error_abort("updateLayerGain: Unknown layer type.");

    }
}

void KonfytPatchEngine::updatePatchLayersGain(KonfytPatch *patch)
{
    if (!patch) { return; }
    foreach (KfPatchLayerSharedPtr layer, patch->layers()) {
        updateLayerGain(layer);
    }
}

void KonfytPatchEngine::updatePatchLayersSoloMute(KonfytPatch *patch)
{
    if (patch == nullptr) { return; }

    QList<KfPatchLayerWeakPtr> layerList = mCurrentPatch->layers();

    // Determine solo
    bool solo = false;
    foreach (KfPatchLayerSharedPtr layer, layerList) {
        if (layer->isSolo()) {
            solo = true;
            break;
        }
    }

    // Activate/deactivate routing based on solo/mute
    foreach (KfPatchLayerSharedPtr layer, layerList) {

        if (layer->hasError()) { continue; }

        bool activate = false;
        if (solo && layer->isSolo()) { activate = true; }
        if (!solo) { activate = true; }
        if (layer->isMute()) { activate = false; }

        setLayerActive(layer, activate);
    }
}

void KonfytPatchEngine::setLayerActive(KfPatchLayerSharedPtr layer, bool active)
{
    if (layer->hasError()) { return; }

    KonfytPatchLayer::LayerType layerType = layer->layerType();

    // Activate route(s) in JACK engine
    if (layerType ==  KonfytPatchLayer::TypeSoundfontProgram) {

        jack->setSoundfontActive(layer->soundfontData.portsInJackEngine, active);

    } else if (layerType == KonfytPatchLayer::TypeSfz) {

        jack->setPluginActive(layer->sfzData.portsInJackEngine, active);

    } else if (layerType == KonfytPatchLayer::TypeMidiOut) {

        jack->setMidiRouteActive(layer->midiOutputPortData.jackRoute, active);

    } else if (layerType == KonfytPatchLayer::TypeAudioIn) {

        jack->setAudioRouteActive(layer->audioInPortData.jackRouteLeft, active);
        jack->setAudioRouteActive(layer->audioInPortData.jackRouteRight, active);

    } else if (layerType == KonfytPatchLayer::TypeUninitialized) {

        error_abort("setLayerActive: layer type uninitialized.");

    } else {

        error_abort("setLayerActive: unknown layer type.");

    }
}

/* Return the current patch. */
KonfytPatch *KonfytPatchEngine::currentPatch()
{
    return mCurrentPatch;
}

float KonfytPatchEngine::getMasterGain()
{
    return mMasterGain;
}

void KonfytPatchEngine::setMasterGain(float newGain)
{
    mMasterGain = newGain;

    updatePatchLayersGain(mCurrentPatch);
    // Also for all always-active patches
    foreach (KonfytPatch* patch, patches) {
        if (patch == mCurrentPatch) { continue; }
        if (!isPatchLoaded(patch)) { continue; }
        if (!patch->alwaysActive) { continue; }
        updatePatchLayersGain(patch);
    }
}

void KonfytPatchEngine::setMidiCatchupRange(int range)
{
    mMidiCatchupRange = range;
}

void KonfytPatchEngine::setLayerGain(KfPatchLayerWeakPtr patchLayer, float newGain)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    patchLayer.toStrongRef()->setGain(newGain);
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

    KfPatchLayerSharedPtr patchLayer = mCurrentPatch->layer(layerIndex);
    patchLayer->setGainMidiCatchupRange(mMidiCatchupRange);
    patchLayer->setGainByMidi(midiValue);
    updateLayerGain(patchLayer);
}

void KonfytPatchEngine::setLayerSolo(KfPatchLayerWeakPtr patchLayer, bool solo)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    patchLayer.toStrongRef()->setSolo(solo);
    updatePatchLayersSoloMute(mCurrentPatch);
}

/* Set layer solo, using layer index as parameter. */
void KonfytPatchEngine::setLayerSolo(int layerIndex, bool solo)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    setLayerSolo( mCurrentPatch->layer(layerIndex), solo );
}

void KonfytPatchEngine::setLayerMute(KfPatchLayerWeakPtr patchLayer, bool mute)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    patchLayer.toStrongRef()->setMute(mute);
    updatePatchLayersSoloMute(mCurrentPatch);
}

/* Set layer mute, using layer index as parameter. */
void KonfytPatchEngine::setLayerMute(int layerIndex, bool mute)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    setLayerMute( mCurrentPatch->layer(layerIndex), mute );
}

void KonfytPatchEngine::setLayerBus(KfPatchLayerWeakPtr patchLayer, int bus)
{
    patchLayer.toStrongRef()->setBusIdInProject(bus);
    updateLayerRouting(patchLayer);
}

void KonfytPatchEngine::setLayerMidiInPort(KfPatchLayerWeakPtr patchLayer, int portId)
{
    patchLayer.toStrongRef()->setMidiInPortIdInProject(portId);
    updateLayerRouting(patchLayer);
}

void KonfytPatchEngine::sendCurrentPatchMidi()
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    foreach (KfPatchLayerWeakPtr layer, mCurrentPatch->getMidiOutputLayerList()) {
        sendLayerMidi(layer);
    }
}

void KonfytPatchEngine::sendLayerMidi(KfPatchLayerWeakPtr patchLayer)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    KfPatchLayerSharedPtr l = patchLayer.toStrongRef();
    if (l->hasError()) { return; }
    if (l->layerType() == KonfytPatchLayer::TypeMidiOut) {
        jack->sendMidiEventsOnRoute(l->midiOutputPortData.jackRoute,
                                    l->getMidiSendListEvents());
    }
}

int KonfytPatchEngine::getNumLayers() const
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, 0);

    return mCurrentPatch->layerCount();
}

void KonfytPatchEngine::setLayerFilter(KfPatchLayerWeakPtr patchLayer, KonfytMidiFilter filter)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    KfPatchLayerSharedPtr l = patchLayer.toStrongRef();
    l->setMidiFilter(filter);

    // And also in the respective engine.
    if (l->layerType() == KonfytPatchLayer::TypeSoundfontProgram) {

        jack->setSoundfontMidiFilter(l->soundfontData.portsInJackEngine, filter);

    } else if (l->layerType() == KonfytPatchLayer::TypeSfz) {

        jack->setPluginMidiFilter(l->sfzData.portsInJackEngine, filter);

    } else if (l->layerType() == KonfytPatchLayer::TypeMidiOut) {

        jack->setRouteMidiFilter(l->midiOutputPortData.jackRoute, filter);

    }
}

void KonfytPatchEngine::setPatchName(QString newName)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    mCurrentPatch->setName(newName);
}

QString KonfytPatchEngine::getPatchName()
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, "");

    return mCurrentPatch->name();
}

void KonfytPatchEngine::setPatchNote(QString newNote)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    mCurrentPatch->setNote(newNote);
}

QString KonfytPatchEngine::getPatchNote()
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, "");

    return mCurrentPatch->note();
}

void KonfytPatchEngine::setPatchAlwaysActive(bool alwaysActive)
{
    KONFYT_ASSERT_RETURN(mCurrentPatch);

    mCurrentPatch->alwaysActive = alwaysActive;
}

KfPatchLayerWeakPtr KonfytPatchEngine::addMidiOutPortToPatch(int port)
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, KfPatchLayerWeakPtr());

    KfPatchLayerSharedPtr layer = mCurrentPatch->addMidiOutputPort(port).toStrongRef();

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(mCurrentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();

    return layer.toWeakRef();
}

KfPatchLayerWeakPtr KonfytPatchEngine::addAudioInPortToPatch(int port)
{
    KONFYT_ASSERT_RETURN_VAL(mCurrentPatch, KfPatchLayerWeakPtr());

    KfPatchLayerSharedPtr layer = mCurrentPatch->addAudioInPort(port).toStrongRef();

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(mCurrentProject->audioBus_getFirstBusId(-1));

    reloadPatch();

    return layer.toWeakRef();
}

/* Print error message to stdout, and abort app. */
void KonfytPatchEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: patchEngine:" << msg.toLocal8Bit().constData();
    abort();
}

void KonfytPatchEngine::onSfzEngineInitDone(QString error)
{
    if (error.isEmpty()) {
        print("SFZ engine initialised successfully.");
        reloadPatch();
    } else {
        print("SFZ engine initialisation error: " + error);
    }
}

