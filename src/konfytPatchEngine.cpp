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

#include "konfytPatchEngine.h"

#include <iostream>

KonfytPatchEngine::KonfytPatchEngine(QObject *parent) :
    QObject(parent)
{
}

KonfytPatchEngine::~KonfytPatchEngine()
{
    delete sfzEngine;
}

void KonfytPatchEngine::initPatchEngine(KonfytJackEngine* newJackClient, KonfytAppInfo appInfo)
{
    // Jack client (probably received from MainWindow) so we can directly create ports
    this->jack = newJackClient;

    // Fluidsynth Engine
    KonfytFluidsynthEngine* e = new KonfytFluidsynthEngine();
    connect(e, &KonfytFluidsynthEngine::userMessage, [=](QString msg){
        userMessage("Fluidsynth: " + msg);
    });
    e->initFluidsynth(jack->getSampleRate());
    fluidsynthEngine = e;
    jack->setFluidsynthEngine(e); // Give to Jack so it can get sound out of it.

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

    connect(sfzEngine, &KonfytBaseSoundEngine::userMessage, [=](QString msg){
        userMessage(sfzEngine->engineName() + ": " + msg);
    });
    connect(sfzEngine, &KonfytBaseSoundEngine::statusInfo,
            this, &KonfytPatchEngine::statusInfo);

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

void KonfytPatchEngine::setProject(KonfytProject *project)
{
    this->currentProject = project;
}

/* Reload current patch (e.g. if patch changed). */
void KonfytPatchEngine::reloadPatch()
{
    loadPatch( mCurrentPatch );
}

/* Ensure patch and all layers are unloaded from their respective engines. */
void KonfytPatchEngine::unloadPatch(KonfytPatch *patch)
{
    if ( !patches.contains(patch) ) { return; }

    foreach (KfPatchLayerWeakPtr layer, patch->layers()) {
        unloadLayer(layer);
    }

    patches.removeAll(patch);
    if (mCurrentPatch == patch) { mCurrentPatch = NULL; }
}

/* Load patch, replacing the current patch. */
bool KonfytPatchEngine::loadPatch(KonfytPatch *newPatch)
{
    if (newPatch == NULL) { return false; }

    bool ret = true; // return value. Set to false when an error occured somewhere.

    // Deactivate routes for current patch
    if (newPatch != mCurrentPatch) {
        if (mCurrentPatch != NULL) {
            if (!mCurrentPatch->alwaysActive) {

                // MIDI output port layers
                foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->getMidiOutputLayerList()) {
                    if (layer->hasError()) { continue; }
                    jack->setMidiRouteActive(layer->midiOutputPortData.jackRoute, false);
                }

                // SFZ layers
                foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->getPluginLayerList()) {
                    if (layer->hasError()) { continue; }
                    jack->setPluginActive(layer->sfzData.portsInJackEngine, false);
                }

                // Soundfont layers
                foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->getSfLayerList()) {
                    if (layer->hasError()) { continue; }
                    jack->setSoundfontActive(layer->soundfontData.portsInJackEngine, false);
                }

                // Audio input port layers
                foreach (KfPatchLayerSharedPtr layer, mCurrentPatch->getAudioInLayerList()) {
                    if (layer->hasError()) { continue; }
                    jack->setAudioRouteActive(layer->audioInPortData.jackRouteLeft, false);
                    jack->setAudioRouteActive(layer->audioInPortData.jackRouteRight, false);
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
            layer->soundfontData.synthInEngine = fluidsynthEngine->addSoundfontProgram( layer->soundfontData.program );
            if (layer->soundfontData.synthInEngine) {
                // Add to Jack engine (this also assigns the midi filter)
                KfJackPluginPorts* jackPorts = jack->addSoundfont(layer->soundfontData.synthInEngine);
                layer->soundfontData.portsInJackEngine = jackPorts;
                jack->setSoundfontMidiFilter(jackPorts, layer->midiFilter());
                // Gain, solo, mute and bus are set later in refreshAllGainsAndRouting()
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
            if (!currentProject->audioInPort_exists(portId)) {
                layer->setErrorMessage("No audio-in port in project: " + n2s(portId));
                userMessage("loadPatch: " + layer->errorMessage());
                continue;
            }
            PrjAudioInPort srcPorts = currentProject->audioInPort_getPort(portId);

            // Get destination ports (bus)
            if (!currentProject->audioBus_exists(layer->busIdInProject())) {
                layer->setErrorMessage("No bus in project: " + n2s(portId));
                userMessage("loadPatch: " + layer->errorMessage());
                continue;
            }
            PrjAudioBus destPorts = currentProject->audioBus_getBus(layer->busIdInProject());

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
            if (!currentProject->midiInPort_exists(layer->midiInPortIdInProject())) {
                layer->setErrorMessage("No MIDI-in port in project: " + n2s(layer->midiInPortIdInProject()));
                userMessage("loadPatch: " + layer->errorMessage());
                continue;
            }
            PrjMidiPort srcPort = currentProject->midiInPort_getPort(layer->midiInPortIdInProject());

            // Get destination port
            if (!currentProject->midiOutPort_exists(layer->midiOutputPortData.portIdInProject)) {
                layer->setErrorMessage("No MIDI-out port in project: " + n2s(layer->midiOutputPortData.portIdInProject));
                userMessage("loadPatch: " + layer->errorMessage());
                continue;
            }
            PrjMidiPort destPort = currentProject->midiOutPort_getPort(layer->midiOutputPortData.portIdInProject);

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
    refreshAllGainsAndRouting();

    return ret;
}

KfPatchLayerWeakPtr KonfytPatchEngine::addProgramLayer(KonfytSoundfontProgram newProgram)
{
    Q_ASSERT( mCurrentPatch != NULL );

    KfPatchLayerSharedPtr layer = mCurrentPatch->addProgram(newProgram).toStrongRef();

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(currentProject->audioBus_getFirstBusId(-1));

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(currentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();

    return layer.toWeakRef();
}

void KonfytPatchEngine::removeLayer(KfPatchLayerWeakPtr layer)
{
    Q_ASSERT( mCurrentPatch != NULL );

    removeLayer(mCurrentPatch, layer);
}

void KonfytPatchEngine::removeLayer(KonfytPatch *patch, KfPatchLayerWeakPtr layer)
{
    // Unload from respective engine
    unloadLayer(layer);

    patch->removeLayer(layer);

    if (patch == mCurrentPatch) { reloadPatch(); }
}

void KonfytPatchEngine::unloadLayer(KfPatchLayerWeakPtr layer)
{
    KfPatchLayerSharedPtr l = layer.toStrongRef();
    if (l->layerType() == KonfytPatchLayer::TypeSoundfontProgram) {
        if (l->soundfontData.synthInEngine) {
            // First remove from JACK engine
            jack->removeSoundfont(l->soundfontData.portsInJackEngine);
            // Then from fluidsynthEngine
            fluidsynthEngine->removeSoundfontProgram(l->soundfontData.synthInEngine);
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
    Q_ASSERT( mCurrentPatch != NULL );

    LayerSfzData plugin;
    plugin.path = path;

    // Add the plugin to the patch
    KfPatchLayerSharedPtr layer = mCurrentPatch->addPlugin(plugin, "sfz").toStrongRef();

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(currentProject->audioBus_getFirstBusId(-1));

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(currentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();

    return layer.toWeakRef();
}

/* Converts gain between 0 and 1 from linear to an exponential function that is more
 * suited for human hearing. Input is clipped between 0 and 1. */
float KonfytPatchEngine::convertGain(float linearGain)
{
    if (linearGain < 0) { linearGain = 0; }
    if (linearGain > 1) { linearGain = 1; }

    return linearGain*linearGain*linearGain; // x^3
}


/* Ensure that all engine gains, solos, mutes and audio out routing are set
 * according to current patch and masterGain. */
void KonfytPatchEngine::refreshAllGainsAndRouting()
{
    if (mCurrentPatch == NULL) { return; }

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
    for (int i=0; i < layerList.count(); i++) {

        KfPatchLayerSharedPtr layer = layerList[i];
        if (layer->hasError()) { continue; }

        KonfytPatchLayer::LayerType layerType = layer->layerType();

        PrjAudioBus bus;
        if ( (layerType == KonfytPatchLayer::TypeSoundfontProgram) ||
             (layerType == KonfytPatchLayer::TypeSfz) ||
             (layerType == KonfytPatchLayer::TypeAudioIn) )
        {
            if ( !currentProject->audioBus_exists(layer->busIdInProject()) ) {
                // Invalid bus. Default to the first one.
                userMessage("WARNING: Layer " + n2s(i+1) + " invalid bus " + n2s(layer->busIdInProject()));

                QList<int> busList = currentProject->audioBus_getAllBusIds();
                if (busList.count()) {
                    bus = currentProject->audioBus_getBus(busList[0]);
                    userMessage("   Defaulting to bus " + n2s(busList[0]));
                } else {
                    error_abort("refreshAllGainsAndRouting: Project has no buses.");
                }
            } else {
                bus = currentProject->audioBus_getBus(layer->busIdInProject());
            }
        }
        PrjMidiPort midiInPort;
        if ( (layerType == KonfytPatchLayer::TypeSoundfontProgram) ||
             (layerType == KonfytPatchLayer::TypeSfz) ||
             (layerType == KonfytPatchLayer::TypeMidiOut) )
        {
            if ( !currentProject->midiInPort_exists(layer->midiInPortIdInProject()) ) {
                // Invalid Midi in port. Default to first one.
                userMessage("WARNING: Layer " + n2s(i+1) + " invalid MIDI input port " + n2s(layer->midiInPortIdInProject()));

                QList<int> midiInPortList = currentProject->midiInPort_getAllPortIds();
                if (midiInPortList.count()) {
                    midiInPort = currentProject->midiInPort_getPort(midiInPortList[0]);
                    userMessage("   Defaulting to port " + n2s(midiInPortList[0]));
                } else {
                    error_abort("refreshAllGainsAndRouting: Project has no MIDI Input Ports.");
                }
            } else {
                midiInPort = currentProject->midiInPort_getPort(layer->midiInPortIdInProject());
            }
        }


        bool activate = false;
        if (solo && layer->isSolo()) { activate = true; }
        if (!solo) { activate = true; }
        if (layer->isMute()) { activate = false; }

        if (layerType ==  KonfytPatchLayer::TypeSoundfontProgram) {

            LayerSoundfontData sfData = layer->soundfontData;
            // Gain = layer gain * master gain
            fluidsynthEngine->setGain( sfData.synthInEngine, convertGain(layer->gain()*masterGain) );

            // Set MIDI and bus routing
            jack->setSoundfontRouting( sfData.portsInJackEngine, midiInPort.jackPort,
                                       bus.leftJackPort, bus.rightJackPort );

            // Activate route in JACK engine
            jack->setSoundfontActive(sfData.portsInJackEngine, activate);

        } else if (layerType == KonfytPatchLayer::TypeSfz) {

            LayerSfzData pluginData = layer->sfzData;
            // Gain = layer gain * master gain
            // Set gain of JACK ports instead of sfzEngine->setGain() since this isn't implemented for all engine types yet.
            jack->setPluginGain(pluginData.portsInJackEngine, convertGain(layer->gain()*masterGain) );

            // Set MIDI and bus routing
            jack->setPluginRouting(pluginData.portsInJackEngine,
                                   midiInPort.jackPort,
                                   bus.leftJackPort, bus.rightJackPort);

            // Activate route in JACK engine
            jack->setPluginActive(pluginData.portsInJackEngine, activate);

        } else if (layerType == KonfytPatchLayer::TypeMidiOut) {

            // Set solo and mute in jack client
            LayerMidiOutData portData = layer->midiOutputPortData;
            if (currentProject->midiOutPort_exists(portData.portIdInProject)) {

                PrjMidiPort prjMidiOutPort = currentProject->midiOutPort_getPort( portData.portIdInProject );

                // Set routing
                jack->setMidiRoute(portData.jackRoute, midiInPort.jackPort,
                                   prjMidiOutPort.jackPort);

                // Activate route
                jack->setMidiRouteActive(portData.jackRoute, activate);

            } else {
                userMessage("WARNING: Layer " + n2s(i+1) + " invalid MIDI out port " + n2s(portData.portIdInProject));
            }

        } else if (layerType == KonfytPatchLayer::TypeAudioIn) {

            // The port number in audioInLayerStruct refers to a stereo port pair index in the project.
            // The bus number in audioInLayerStruct refers to a bus in the project with a left and right jack port.
            // We have to retrieve the port pair and bus from the project in order to get the left and right port Jack port numbers.
            LayerAudioInData audioPortData = layer->audioInPortData;
            if (currentProject->audioInPort_exists(audioPortData.portIdInProject)) {

                PrjAudioInPort portPair = currentProject->audioInPort_getPort(audioPortData.portIdInProject);
                // Left channel
                // Gain
                jack->setAudioRouteGain(audioPortData.jackRouteLeft,
                                        convertGain(layer->gain()*masterGain));
                // Bus routing
                jack->setAudioRoute(audioPortData.jackRouteLeft,
                                    portPair.leftJackPort,
                                    bus.leftJackPort);
                // Activate route
                jack->setAudioRouteActive(audioPortData.jackRouteLeft, activate);
                // Right channel
                // Gain
                jack->setAudioRouteGain(audioPortData.jackRouteRight, convertGain(layer->gain()*masterGain));
                // Bus routing
                jack->setAudioRoute(audioPortData.jackRouteRight,
                                    portPair.rightJackPort,
                                    bus.rightJackPort);
                // Activate route
                jack->setAudioRouteActive(audioPortData.jackRouteRight, activate);

            } else {
                userMessage("WARNING: Layer " + n2s(i+1) + " invalid audio in port " + n2s(audioPortData.portIdInProject));
            }

        } else if (layerType == KonfytPatchLayer::TypeUninitialized) {

            error_abort("refreshAllGains(): layer type uninitialized.");

        } else {

            error_abort("patchEngine::refreshAllGains(): unknown layer type.");

        }
    }

}

/* Return the current patch. */
KonfytPatch *KonfytPatchEngine::currentPatch()
{
    return mCurrentPatch;
}

float KonfytPatchEngine::getMasterGain()
{
    return masterGain;
}

void KonfytPatchEngine::setMasterGain(float newGain)
{
    masterGain = newGain;
    refreshAllGainsAndRouting();
}

void KonfytPatchEngine::setLayerGain(KfPatchLayerWeakPtr patchLayer, float newGain)
{
    Q_ASSERT( mCurrentPatch != NULL );

    patchLayer.toStrongRef()->setGain(newGain);
    refreshAllGainsAndRouting();
}

void KonfytPatchEngine::setLayerGain(int layerIndex, float newGain)
{
    Q_ASSERT( mCurrentPatch != NULL );

    setLayerGain( mCurrentPatch->layer(layerIndex), newGain );
}

void KonfytPatchEngine::setLayerSolo(KfPatchLayerWeakPtr patchLayer, bool solo)
{
    Q_ASSERT( mCurrentPatch != NULL );

    patchLayer.toStrongRef()->setSolo(solo);
    refreshAllGainsAndRouting();
}

/* Set layer solo, using layer index as parameter. */
void KonfytPatchEngine::setLayerSolo(int layerIndex, bool solo)
{
    Q_ASSERT( mCurrentPatch != NULL );

    setLayerSolo( mCurrentPatch->layer(layerIndex), solo );
}

void KonfytPatchEngine::setLayerMute(KfPatchLayerWeakPtr patchLayer, bool mute)
{
    Q_ASSERT( mCurrentPatch != NULL );

    patchLayer.toStrongRef()->setMute(mute);
    refreshAllGainsAndRouting();
}

/* Set layer mute, using layer index as parameter. */
void KonfytPatchEngine::setLayerMute(int layerIndex, bool mute)
{
    Q_ASSERT( mCurrentPatch != NULL );

    setLayerMute( mCurrentPatch->layer(layerIndex), mute );
}

void KonfytPatchEngine::setLayerBus(KfPatchLayerWeakPtr patchLayer, int bus)
{
    Q_ASSERT( mCurrentPatch != NULL );

    setLayerBus(mCurrentPatch, patchLayer, bus);
}

void KonfytPatchEngine::setLayerBus(KonfytPatch *patch, KfPatchLayerWeakPtr patchLayer, int bus)
{
    patchLayer.toStrongRef()->setBusIdInProject(bus);
    if (patch == mCurrentPatch) { refreshAllGainsAndRouting(); }
}

void KonfytPatchEngine::setLayerMidiInPort(KfPatchLayerWeakPtr patchLayer, int portId)
{
    Q_ASSERT( mCurrentPatch != NULL );

    setLayerMidiInPort(mCurrentPatch, patchLayer, portId);
}

void KonfytPatchEngine::setLayerMidiInPort(KonfytPatch *patch, KfPatchLayerWeakPtr patchLayer, int portId)
{
    patchLayer.toStrongRef()->setMidiInPortIdInProject(portId);
    if (patch == mCurrentPatch) { refreshAllGainsAndRouting(); }
}

void KonfytPatchEngine::sendCurrentPatchMidi()
{
    Q_ASSERT( mCurrentPatch != NULL );

    foreach (KfPatchLayerWeakPtr layer, mCurrentPatch->getMidiOutputLayerList()) {
        sendLayerMidi(layer);
    }
}

void KonfytPatchEngine::sendLayerMidi(KfPatchLayerWeakPtr patchLayer)
{
    Q_ASSERT( mCurrentPatch != NULL );

    KfPatchLayerSharedPtr l = patchLayer.toStrongRef();
    if (l->hasError()) { return; }
    if (l->layerType() == KonfytPatchLayer::TypeMidiOut) {
        jack->sendMidiEventsOnRoute(l->midiOutputPortData.jackRoute,
                                    l->getMidiSendListEvents());
    }
}

int KonfytPatchEngine::getNumLayers() const
{
    Q_ASSERT( mCurrentPatch != NULL );

    return mCurrentPatch->layerCount();
}

void KonfytPatchEngine::setLayerFilter(KfPatchLayerWeakPtr patchLayer, KonfytMidiFilter filter)
{
    Q_ASSERT( mCurrentPatch != NULL );

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
    Q_ASSERT( mCurrentPatch != NULL );

    mCurrentPatch->setName(newName);
}

QString KonfytPatchEngine::getPatchName()
{
    Q_ASSERT( mCurrentPatch != NULL );

    return mCurrentPatch->name();
}

void KonfytPatchEngine::setPatchNote(QString newNote)
{
    Q_ASSERT( mCurrentPatch != NULL );

    mCurrentPatch->setNote(newNote);
}

QString KonfytPatchEngine::getPatchNote()
{
    Q_ASSERT( mCurrentPatch != NULL );

    return mCurrentPatch->note();
}

void KonfytPatchEngine::setPatchAlwaysActive(bool alwaysActive)
{
    Q_ASSERT( mCurrentPatch != NULL );

    mCurrentPatch->alwaysActive = alwaysActive;
}

KfPatchLayerWeakPtr KonfytPatchEngine::addMidiOutPortToPatch(int port)
{
    Q_ASSERT( mCurrentPatch != NULL );

    KfPatchLayerSharedPtr layer = mCurrentPatch->addMidiOutputPort(port).toStrongRef();

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    layer->setMidiInPortIdInProject(currentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();

    return layer.toWeakRef();
}

KfPatchLayerWeakPtr KonfytPatchEngine::addAudioInPortToPatch(int port)
{
    Q_ASSERT( mCurrentPatch != NULL );

    KfPatchLayerSharedPtr layer = mCurrentPatch->addAudioInPort(port).toStrongRef();

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    layer->setBusIdInProject(currentProject->audioBus_getFirstBusId(-1));

    reloadPatch();

    return layer.toWeakRef();
}

/* Print error message to stdout, and abort app. */
void KonfytPatchEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: patchEngine:" << msg.toLocal8Bit().constData();
    abort();
}

