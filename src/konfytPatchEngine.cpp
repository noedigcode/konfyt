/******************************************************************************
 *
 * Copyright 2019 Gideon van der Kolf
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
    currentProject = NULL;
    currentPatch = NULL;
    bridge = false;
}

KonfytPatchEngine::~KonfytPatchEngine()
{
    delete sfzEngine;
}


// Get a userMessage signal from an engine, and pass it on to the gui.
void KonfytPatchEngine::userMessageFromEngine(QString msg)
{
    userMessage("patchEngine: " + msg);
}

void KonfytPatchEngine::initPatchEngine(KonfytJackEngine* newJackClient, KonfytAppInfo appInfo)
{
    // Jack client (probably received from MainWindow) so we can directly create ports
    this->jack = newJackClient;

    // Fluidsynth Engine
    konfytFluidsynthEngine* e = new konfytFluidsynthEngine();
    connect(e, &konfytFluidsynthEngine::userMessage,
            this, &KonfytPatchEngine::userMessageFromEngine);
    e->InitFluidsynth(jack->getSampleRate());
    fluidsynthEngine = e;
    jack->setFluidsynthEngine(e); // Give to Jack so it can get sound out of it.

    // Initialise SFZ Backend

    bridge = appInfo.bridge;
    if (bridge) {
        // Create bridge engine (each plugin is hosted in new Konfyt process)
        sfzEngine = new KonfytBridgeEngine();
        static_cast<KonfytBridgeEngine*>(sfzEngine)->setKonfytExePath(appInfo.exePath);
    } else if (appInfo.carla) {
        // Use local Carla engine
        sfzEngine = new KonfytCarlaEngine();
    } else {
        // Use Linuxsampler via LSCP
        sfzEngine = new KonfytLscpEngine();
    }

    connect(sfzEngine, &KonfytBaseSoundEngine::userMessage,
            this, &KonfytPatchEngine::userMessageFromEngine);
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
    // indicate panic situation to Jack
    jack->panic(p);
}

void KonfytPatchEngine::setProject(KonfytProject *project)
{
    this->currentProject = project;
}

// Reload current patch (e.g. if patch changed).
void KonfytPatchEngine::reloadPatch()
{
    loadPatch( currentPatch );
}

// Ensure patch and all layers are unloaded from their respective engines
void KonfytPatchEngine::unloadPatch(KonfytPatch *patch)
{
    if ( !patches.contains(patch) ) { return; }

    QList<KonfytPatchLayer> l = patch->getLayerItems();
    for (int i=0; i<l.count(); i++) {
        KonfytPatchLayer layer = l[i];
        unloadLayer(patch, &layer);
    }

    patches.removeAll(patch);
    if (currentPatch == patch) { currentPatch = NULL; }
}

// Load patch, replacing the current patch
bool KonfytPatchEngine::loadPatch(KonfytPatch *newPatch)
{
    if (newPatch == NULL) { return false; }

    bool r = true; // return value. Set to false when an error occured somewhere.

    // Deactivate routes for current patch
    if (newPatch != currentPatch) {
        if (currentPatch != NULL) {
            if (!currentPatch->alwaysActive) {
                jack->activateRoutesForPatch(currentPatch, false);
            }
        }
    }

    bool patchIsNew = false;
    if ( !patches.contains(newPatch) ) {
        patchIsNew = true;
        patches.append(newPatch);
    }
    currentPatch = newPatch;

    // ---------------------------------------------------
    // SFZ layers:
    // ---------------------------------------------------

    // For each SFZ layer in the patch...
    QList<KonfytPatchLayer> pluginList = currentPatch->getPluginLayerList();
    for (int i=0; i<pluginList.count(); i++) {
        // If layer indexInEngine is -1, the layer hasn't been loaded yet.
        KonfytPatchLayer layer = pluginList[i];
        if (patchIsNew) { layer.sfzData.indexInEngine = -1; }
        if ( layer.sfzData.indexInEngine == -1 ) {

            // Load in SFZ engine
            int ID = sfzEngine->addSfz( layer.sfzData.path );

            if (ID < 0) {
                layer.setErrorMessage("Failed to load SFZ: " + layer.sfzData.path);
                r = false;
            } else {
                layer.sfzData.indexInEngine = ID;
                layer.sfzData.name = sfzEngine->pluginName(ID);
                // Add to JACK engine

                // Find the plugin midi input port
                layer.sfzData.midi_in_port = sfzEngine->midiInJackPortName(ID);

                // Find the plugin audio output ports
                QStringList audioLR = sfzEngine->audioOutJackPortNames(ID);
                layer.sfzData.audio_out_port_left = audioLR[0];
                layer.sfzData.audio_out_port_right = audioLR[1];

                // The plugin object now contains the midi input port and
                // audio left and right output ports. Give this to JACK, which will:
                // - create a midi output port and connect it to the plugin midi input port,
                // - create audio input ports and connect it to the plugin audio output ports.
                // - assigns the midi filter
                KonfytJackPortsSpec spec;
                spec.name = layer.sfzData.name;
                spec.midiOutConnectTo = layer.sfzData.midi_in_port;
                spec.midiFilter = layer.sfzData.midiFilter;
                spec.audioInLeftConnectTo = layer.sfzData.audio_out_port_left;
                spec.audioInRightConnectTo = layer.sfzData.audio_out_port_right;
                int jackId = jack->addPluginPortsAndConnect( spec );
                layer.sfzData.portsIdInJackEngine = jackId;

                // Gain, solo, mute and bus are set later in refershAllGainsAndRouting()
            }
            // Update layer in patch
            currentPatch->replaceLayer(layer);
        }
    }

    // ---------------------------------------------------
    // Fluidsynth layers:
    // ---------------------------------------------------

    // For each soundfont program in the patch...
    QList<KonfytPatchLayer> sflist = currentPatch->getSfLayerList();
    for (int i=0; i < sflist.count(); i++) {
        // If layer indexInEngine is -1, layer hasn't been loaded yet.
        KonfytPatchLayer layer = sflist[i];
        if (patchIsNew) { layer.soundfontData.indexInEngine = -1; }
        if ( layer.soundfontData.indexInEngine == -1 ) {
            // Load in Fluidsynth engine
            int ID = fluidsynthEngine->addSoundfontProgram( layer.soundfontData.program );
            if (ID < 0) {
                layer.setErrorMessage("Failed to load soundfont.");
                r = false;
            } else {
                layer.soundfontData.indexInEngine = ID;
                // Add to Jack engine (this also assigns the midi filter)
                int jackId = jack->addSoundfont( layer.soundfontData );
                layer.soundfontData.idInJackEngine = jackId;
                // Gain, solo, mute and bus are set later in refreshAllGainsAndRouting()
            }
            // Update layer in patch
            currentPatch->replaceLayer(layer);
        }

    }


    // ---------------------------------------------------
    // Audio input ports:
    // ---------------------------------------------------

    QList<KonfytPatchLayer> audioInLayers = currentPatch->getAudioInLayerList();
    foreach (KonfytPatchLayer layer, audioInLayers) {
        if (layer.audioInPortData.jackRouteIdLeft < 0) {
            // Routes haven't been created yet

            // Get source ports
            int portId = layer.audioInPortData.portIdInProject;
            if (!currentProject->audioInPort_exists(portId)) {
                layer.setErrorMessage("No audio-in port in project: " + n2s(portId));
                userMessage("loadPatch: " + layer.getErrorMessage());
                // Update layer in patch
                currentPatch->replaceLayer(layer);
                continue;
            }
            PrjAudioInPort srcPorts = currentProject->audioInPort_getPort(portId);

            // Get destination ports (bus)
            if (!currentProject->audioBus_exists(layer.busIdInProject)) {
                layer.setErrorMessage("No bus in project: " + n2s(portId));
                userMessage("loadPatch: " + layer.getErrorMessage());
                // Update layer in patch
                currentPatch->replaceLayer(layer);
                continue;
            }
            PrjAudioBus destPorts = currentProject->audioBus_getBus(layer.busIdInProject);

            // Route for left port
            layer.audioInPortData.jackRouteIdLeft = jack->addAudioRoute(
                        srcPorts.leftJackPortId, destPorts.leftJackPortId);
            // Route for right port
            layer.audioInPortData.jackRouteIdRight = jack->addAudioRoute(
                        srcPorts.rightJackPortId, destPorts.rightJackPortId);

            // Update layer in patch
            currentPatch->replaceLayer(layer);
        }
    }


    // ---------------------------------------------------
    // Midi output ports:
    // ---------------------------------------------------

    QList<KonfytPatchLayer> midiOutLayers = currentPatch->getMidiOutputLayerList();
    foreach (KonfytPatchLayer layer, midiOutLayers) {
        if (layer.midiOutputPortData.jackRouteId < 0) {
            // Route hasn't been created yet

            // Get source port
            if (!currentProject->midiInPort_exists(layer.midiInPortIdInProject)) {
                layer.setErrorMessage("No MIDI-in port in project: " + n2s(layer.midiInPortIdInProject));
                userMessage("loadPatch: " + layer.getErrorMessage());
                // Update layer in patch
                currentPatch->replaceLayer(layer);
                continue;
            }
            PrjMidiPort srcPort = currentProject->midiInPort_getPort(layer.midiInPortIdInProject);

            // Get destination port
            if (!currentProject->midiOutPort_exists(layer.midiOutputPortData.portIdInProject)) {
                layer.setErrorMessage("No MIDI-out port in project: " + n2s(layer.midiOutputPortData.portIdInProject));
                userMessage("loadPatch: " + layer.getErrorMessage());
                // Update layer in patch
                currentPatch->replaceLayer(layer);
                continue;
            }
            PrjMidiPort destPort = currentProject->midiOutPort_getPort(layer.midiOutputPortData.portIdInProject);

            // Create route
            layer.midiOutputPortData.jackRouteId = jack->addMidiRoute(
                        srcPort.jackPortId, destPort.jackPortId);

            // Update layer in patch
            currentPatch->replaceLayer(layer);
        }

        // Set MIDI Filter
        jack->setRouteMidiFilter(layer.midiOutputPortData.jackRouteId,
                                 layer.midiOutputPortData.filter);
    }

    // ---------------------------------------------------
    // The rest
    // ---------------------------------------------------

    // All layers are now loaded. Now set gains, activate routes, etc.
    refreshAllGainsAndRouting();

    return r;
}

KonfytPatchLayer KonfytPatchEngine::addProgramLayer(KonfytSoundfontProgram newProgram)
{
    KonfytPatchLayer layer;

    if (currentPatch == NULL) { return layer; }

    layer = currentPatch->addProgram(newProgram);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    currentPatch->setLayerBus(&layer, currentProject->audioBus_getFirstBusId(-1));

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    currentPatch->setLayerMidiInPort(&layer, currentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();
    // When loading the patch, the LayerItem.sfData.indexInEngine is set.
    // In order for the returned LayerItem to contain this correct info,
    // we need to retrieve an updated one from the engine's patch.
    layer = currentPatch->getLayerItem(layer);
    return layer;
}

void KonfytPatchEngine::removeLayer(KonfytPatchLayer* item)
{
    Q_ASSERT( currentPatch != NULL );

    removeLayer(currentPatch, item);
}

void KonfytPatchEngine::removeLayer(KonfytPatch *patch, KonfytPatchLayer *item)
{
    // Unload from respective engine
    unloadLayer(patch, item);

    patch->removeLayer(item);
    if (patch == currentPatch) { reloadPatch(); }
}

void KonfytPatchEngine::unloadLayer(KonfytPatch *patch, KonfytPatchLayer *item)
{
    KonfytPatchLayer layer = patch->getLayerItem( *item );
    if (layer.getLayerType() == KonfytLayerType_SoundfontProgram) {
        if (layer.soundfontData.indexInEngine >= 0) {
            // First remove from jack
            jack->removeSoundfont(layer.soundfontData.idInJackEngine);
            // Then from fluidsynthEngine
            fluidsynthEngine->removeSoundfontProgram(layer.soundfontData.indexInEngine);
            // Set unloaded in patch
            layer.soundfontData.indexInEngine = -1;
            patch->replaceLayer(layer);
        }
    } else if (layer.getLayerType() == KonfytLayerType_Sfz) {
        if (layer.sfzData.indexInEngine >= 0) {
            // First remove from JACK
            jack->removePlugin(layer.sfzData.portsIdInJackEngine);
            // Then from SFZ engine
            sfzEngine->removeSfz(layer.sfzData.indexInEngine);
            // Set unloaded in patch
            layer.sfzData.indexInEngine = -1;
            patch->replaceLayer(layer);
        }
    }
}

KonfytPatchLayer KonfytPatchEngine::reloadLayer(KonfytPatchLayer *item)
{
    unloadLayer(currentPatch, item);
    reloadPatch();
    // Note that some IDs in the layer item might have changed when it was loaded again.
    // Get the new layer from the patch and return it so it can be updated elsewhere also.
    return currentPatch->getLayerItem(*item);
}


KonfytPatchLayer KonfytPatchEngine::addSfzLayer(QString path)
{
    Q_ASSERT( currentPatch != NULL );

    LayerSfzStruct plugin = LayerSfzStruct();
    plugin.gain = DEFAULT_GAIN_FOR_NEW_LAYER;
    plugin.mute = false;
    plugin.solo = false;
    plugin.name = "sfz";
    plugin.path = path;

    // Add the plugin to the patch
    KonfytPatchLayer g = currentPatch->addPlugin(plugin);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    currentPatch->setLayerBus(&g, currentProject->audioBus_getFirstBusId(-1));

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    currentPatch->setLayerMidiInPort(&g, currentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();


    // Note that the LayerItem g we created above does not have the correct
    // LayerSfzStruct indexInEngine. That is only assigned when loading the patch,
    // in loadPatch.
    // But it does have a unique id, which is how we can now get the 'real'
    // LayerItem from the loaded patch:

    return currentPatch->getLayerItem(g);
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
    if (currentPatch == NULL) { return; }

    QList<KonfytPatchLayer> layerList = currentPatch->getLayerItems();

    // Determine solo
    bool solo = false;
    foreach (KonfytPatchLayer layer, layerList) {
        if (layer.isSolo()) {
            solo = true;
            break;
        }
    }

    // Activate/deactivate routing based on solo/mute
    for (int i=0; i < layerList.count(); i++) {

        KonfytPatchLayer layer = layerList[i];
        if (layer.hasError()) { continue; }

        KonfytLayerType layerType = layer.getLayerType();

        PrjAudioBus bus;
        if ( (layerType == KonfytLayerType_SoundfontProgram) ||
             (layerType == KonfytLayerType_Sfz) ||
             (layerType == KonfytLayerType_AudioIn) )
        {
            if ( !currentProject->audioBus_exists(layer.busIdInProject) ) {
                // Invalid bus. Default to the first one.
                userMessage("WARNING: Layer " + n2s(i+1) + " invalid bus " + n2s(layer.busIdInProject));

                QList<int> busList = currentProject->audioBus_getAllBusIds();
                if (busList.count()) {
                    bus = currentProject->audioBus_getBus(busList[0]);
                    userMessage("   Defaulting to bus " + n2s(busList[0]));
                } else {
                    error_abort("refreshAllGainsAndRouting: Project has no busses.");
                }
            } else {
                bus = currentProject->audioBus_getBus(layer.busIdInProject);
            }
        }
        PrjMidiPort midiInPort;
        if ( (layerType == KonfytLayerType_SoundfontProgram) ||
             (layerType == KonfytLayerType_Sfz) ||
             (layerType == KonfytLayerType_MidiOut) )
        {
            if ( !currentProject->midiInPort_exists(layer.midiInPortIdInProject) ) {
                // Invalid Midi in port. Default to first one.
                userMessage("WARNING: Layer " + n2s(i+1) + " invalid MIDI input port " + n2s(layer.midiInPortIdInProject));

                QList<int> midiInPortList = currentProject->midiInPort_getAllPortIds();
                if (midiInPortList.count()) {
                    midiInPort = currentProject->midiInPort_getPort(midiInPortList[0]);
                    userMessage("   Defaulting to port " + n2s(midiInPortList[0]));
                } else {
                    error_abort("refreshAllGainsAndRouting: Project has no MIDI Input Ports.");
                }
            } else {
                midiInPort = currentProject->midiInPort_getPort(layer.midiInPortIdInProject);
            }
        }


        bool activate = false;
        if (solo && layer.isSolo()) { activate = true; }
        if (!solo) { activate = true; }
        if (layer.isMute()) { activate = false; }

        if (layerType ==  KonfytLayerType_SoundfontProgram) {

            LayerSoundfontStruct sfData = layer.soundfontData;
            // Gain = layer gain * master gain
            fluidsynthEngine->setGain( sfData.indexInEngine, convertGain(sfData.gain*masterGain) );

            // Set MIDI and bus routing
            jack->setSoundfontRouting( sfData.idInJackEngine, midiInPort.jackPortId,
                                       bus.leftJackPortId, bus.rightJackPortId );

            // Activate route in JACK engine
            jack->setSoundfontActive(sfData.idInJackEngine, activate);

        } else if (layerType == KonfytLayerType_Sfz) {

            LayerSfzStruct pluginData = layer.sfzData;
            // Gain = layer gain * master gain
            // Set gain of JACK ports instead of sfzEngine->setGain() since this isn't implemented for all engine types yet.
            jack->setPluginGain(pluginData.portsIdInJackEngine, convertGain(pluginData.gain*masterGain) );

            // Set MIDI and bus routing
            jack->setPluginRouting(pluginData.portsIdInJackEngine,
                                   midiInPort.jackPortId,
                                   bus.leftJackPortId, bus.rightJackPortId);

            // Activate route in JACK engine
            jack->setPluginActive(pluginData.portsIdInJackEngine, activate);

        } else if (layerType == KonfytLayerType_MidiOut) {

            // Set solo and mute in jack client
            LayerMidiOutStruct portData = layer.midiOutputPortData;
            if (currentProject->midiOutPort_exists(portData.portIdInProject)) {

                PrjMidiPort prjMidiOutPort = currentProject->midiOutPort_getPort( portData.portIdInProject );

                // Set routing
                jack->setMidiRoute(portData.jackRouteId, midiInPort.jackPortId,
                                   prjMidiOutPort.jackPortId);

                // Activate route
                jack->setMidiRouteActive(portData.jackRouteId, activate);

            } else {
                userMessage("WARNING: Layer " + n2s(i+1) + " invalid MIDI out port " + n2s(portData.portIdInProject));
            }

        } else if (layerType == KonfytLayerType_AudioIn) {

            // The port number in audioInLayerStruct refers to a stereo port pair index in the project.
            // The bus number in audioInLayerStruct refers to a bus in the project with a left and right jack port.
            // We have to retrieve the port pair and bus from the project in order to get the left and right port Jack port numbers.
            LayerAudioInStruct audioPortData = layer.audioInPortData;
            if (currentProject->audioInPort_exists(audioPortData.portIdInProject)) {

                PrjAudioInPort portPair = currentProject->audioInPort_getPort(audioPortData.portIdInProject);
                // Left channel
                // Gain
                jack->setAudioRouteGain(audioPortData.jackRouteIdLeft, convertGain(audioPortData.gain*masterGain));
                // Bus routing
                jack->setAudioRoute(audioPortData.jackRouteIdLeft,
                                    portPair.leftJackPortId,
                                    bus.leftJackPortId);
                // Activate route
                jack->setAudioRouteActive(audioPortData.jackRouteIdLeft, activate);
                // Right channel
                // Gain
                jack->setAudioRouteGain(audioPortData.jackRouteIdRight, convertGain(audioPortData.gain*masterGain));
                // Bus routing
                jack->setAudioRoute(audioPortData.jackRouteIdRight,
                                    portPair.rightJackPortId,
                                    bus.rightJackPortId);
                // Activate route
                jack->setAudioRouteActive(audioPortData.jackRouteIdRight, activate);

            } else {
                userMessage("WARNING: Layer " + n2s(i+1) + " invalid audio in port " + n2s(audioPortData.portIdInProject));
            }

        } else if (layerType == KonfytLayerType_Uninitialized) {

            error_abort("refreshAllGains(): layer type uninitialized.");

        } else {

            error_abort("patchEngine::refreshAllGains(): unknown layer type.");

        }
    }

}

// Return the current patch
KonfytPatch *KonfytPatchEngine::getPatch()
{
    return currentPatch;
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

int KonfytPatchEngine::getNumLayers()
{
    Q_ASSERT( currentPatch != NULL );

    return currentPatch->getNumLayers();
}

void KonfytPatchEngine::setLayerGain(KonfytPatchLayer *layerItem, float newGain)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setLayerGain(layerItem, newGain);
    refreshAllGainsAndRouting();
}

void KonfytPatchEngine::setLayerGain(int layerIndex, float newGain)
{
    Q_ASSERT( currentPatch != NULL );

    QList<KonfytPatchLayer> l =  currentPatch->getLayerItems();
    if ( (layerIndex >= 0) && (layerIndex < l.count()) ) {
        KonfytPatchLayer g = l.at(layerIndex);
        currentPatch->setLayerGain(&g, newGain);
        refreshAllGainsAndRouting();
    } else {
        // Logic error somewhere else.
        error_abort("setLayerGain: layerIndex " + n2s(layerIndex) + " out of range.");
    }
}

void KonfytPatchEngine::setLayerSolo(KonfytPatchLayer *layerItem, bool solo)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setLayerSolo(layerItem, solo);
    refreshAllGainsAndRouting();
}

/* Set layer solo, using layer index as parameter. */
void KonfytPatchEngine::setLayerSolo(int layerIndex, bool solo)
{
    Q_ASSERT( currentPatch != NULL );

    QList<KonfytPatchLayer> l =  currentPatch->getLayerItems();
    if ( (layerIndex >= 0) && (layerIndex < l.count()) ) {
        KonfytPatchLayer g = l.at(layerIndex);
        setLayerSolo( &g, solo );
    } else {
        // Logic error somewhere else.
        error_abort("setLayerSolo: layerIndex " + n2s(layerIndex) + " out of range.");
    }
}



void KonfytPatchEngine::setLayerMute(KonfytPatchLayer *layerItem, bool mute)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setLayerMute(layerItem, mute);
    refreshAllGainsAndRouting();
}

/* Set layer mute, using layer index as parameter. */
void KonfytPatchEngine::setLayerMute(int layerIndex, bool mute)
{
    Q_ASSERT( currentPatch != NULL );

    QList<KonfytPatchLayer> l =  currentPatch->getLayerItems();
    if ( (layerIndex >= 0) && (layerIndex < l.count()) ) {
        KonfytPatchLayer g = l.at(layerIndex);
        currentPatch->setLayerMute(&g, mute);
        refreshAllGainsAndRouting();
    } else {
        // Logic error somewhere else.
        error_abort( "setLayerMute: layerIndex " + n2s(layerIndex) + " out of range." );
    }
}

void KonfytPatchEngine::setLayerBus(KonfytPatchLayer *layerItem, int bus)
{
    Q_ASSERT( currentPatch != NULL );
    setLayerBus(currentPatch, layerItem, bus);
}

void KonfytPatchEngine::setLayerBus(KonfytPatch *patch, KonfytPatchLayer *layerItem, int bus)
{
    patch->setLayerBus(layerItem, bus);
    if (patch == currentPatch) { refreshAllGainsAndRouting(); }
}

void KonfytPatchEngine::setLayerMidiInPort(KonfytPatchLayer *layerItem, int portId)
{
    Q_ASSERT( currentPatch != NULL );
    setLayerMidiInPort(currentPatch, layerItem, portId);
}

void KonfytPatchEngine::setLayerMidiInPort(KonfytPatch *patch, KonfytPatchLayer *layerItem, int portId)
{
    patch->setLayerMidiInPort(layerItem, portId);
    if (patch == currentPatch) { refreshAllGainsAndRouting(); }
}

void KonfytPatchEngine::setLayerMidiSendList(KonfytPatchLayer *layerItem, QList<KonfytMidiEvent> events)
{
    Q_ASSERT( currentPatch != NULL );
    currentPatch->setLayerMidiSendEvents(layerItem, events);
}

void KonfytPatchEngine::sendCurrentPatchMidi()
{
    Q_ASSERT( currentPatch != NULL );
    foreach (KonfytPatchLayer layer, currentPatch->getMidiOutputLayerList()) {
        if (layer.hasError()) { continue; }
        jack->sendMidiEventsOnRoute(layer.midiOutputPortData.jackRouteId,
                                    layer.midiSendList);
    }
}

void KonfytPatchEngine::sendLayerMidi(KonfytPatchLayer *layerItem)
{
    Q_ASSERT( currentPatch != NULL );
    KonfytPatchLayer l = currentPatch->getLayerItem(*layerItem);
    if (l.hasError()) { return; }
    if (l.getLayerType() == KonfytLayerType_MidiOut) {
        jack->sendMidiEventsOnRoute(l.midiOutputPortData.jackRouteId,
                                    l.midiSendList);
    }
}

void KonfytPatchEngine::setLayerFilter(KonfytPatchLayer *layerItem, KonfytMidiFilter filter)
{
    Q_ASSERT( currentPatch != NULL );

    // Set filter in patch
    currentPatch->setLayerFilter(layerItem, filter);

    // And also in the respective engine.
    if (layerItem->getLayerType() == KonfytLayerType_SoundfontProgram) {

        jack->setSoundfontMidiFilter(layerItem->soundfontData.idInJackEngine, filter);

    } else if (layerItem->getLayerType() == KonfytLayerType_Sfz) {

        jack->setPluginMidiFilter( layerItem->sfzData.portsIdInJackEngine,
                                         layerItem->sfzData.midiFilter);

    } else if (layerItem->getLayerType() == KonfytLayerType_MidiOut) {

        LayerMidiOutStruct layerPort = layerItem->midiOutputPortData;
        jack->setRouteMidiFilter(layerPort.jackRouteId, layerPort.filter);

    }
}



void KonfytPatchEngine::setPatchName(QString newName)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setName(newName);
}

QString KonfytPatchEngine::getPatchName()
{
    Q_ASSERT( currentPatch != NULL );

    return currentPatch->getName();
}

void KonfytPatchEngine::setPatchNote(QString newNote)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setNote(newNote);
}

QString KonfytPatchEngine::getPatchNote()
{
    Q_ASSERT( currentPatch != NULL );

    return currentPatch->getNote();
}

void KonfytPatchEngine::setPatchAlwaysActive(bool alwaysActive)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->alwaysActive = alwaysActive;
}

KonfytPatchLayer KonfytPatchEngine::addMidiOutPortToPatch(int port)
{
    Q_ASSERT( currentPatch != NULL );

    KonfytPatchLayer g = currentPatch->addMidiOutputPort(port);

    // The MIDI in port defaults to 0, but the project may not have a port 0.
    // Find the first port in the project.
    currentPatch->setLayerMidiInPort(&g, currentProject->midiInPort_getFirstPortId(-1));

    reloadPatch();

    // Layer might have been updated, return up to date one from patch
    return currentPatch->getLayerItem(g);
}

KonfytPatchLayer KonfytPatchEngine::addAudioInPortToPatch(int port)
{
    Q_ASSERT( currentPatch != NULL );

    KonfytPatchLayer g = currentPatch->addAudioInPort( port );

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    currentPatch->setLayerBus(&g, currentProject->audioBus_getFirstBusId(-1));

    reloadPatch();

    // Layer might have been updated, return up to date one from patch
    return currentPatch->getLayerItem(g);
}

// Print error message to stdout, and abort app.
void KonfytPatchEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: patchEngine:" << msg.toLocal8Bit().constData();
    abort();
}

