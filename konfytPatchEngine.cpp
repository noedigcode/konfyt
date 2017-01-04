#include "konfytPatchEngine.h"
#include <iostream>

konfytPatchEngine::konfytPatchEngine(QObject *parent) :
    QObject(parent)
{
    currentProject = NULL;
    currentPatch = NULL;
}


// Get a userMessage signal from an engine, and pass it on to the gui.
void konfytPatchEngine::userMessageFromEngine(QString msg)
{
    userMessage("patchEngine: " + msg);
}

void konfytPatchEngine::initPatchEngine(konfytJackEngine* newJackClient)
{
    // Jack client (probably received from MainWindow) so we can directly create ports
    this->jack = newJackClient;

    // Fluidsynth Engine
    konfytFluidsynthEngine* e = new konfytFluidsynthEngine();
    connect(e, SIGNAL(userMessage(QString)), this, SLOT(userMessageFromEngine(QString)));
    e->InitFluidsynth();
    fluidsynthEngine = e;
    jack->fluidsynthEngine = e; // Give to Jack so it can get sound out of it.

    // Initialise Carla Backend
    carlaEngine = new konfytCarlaEngine();
    connect(carlaEngine, SIGNAL(userMessage(QString)), this, SLOT(userMessageFromEngine(QString)));
    QString carlaJackClientName = jack->clientName() + CARLA_CLIENT_POSTFIX;
    carlaEngine->InitCarlaEngine( jack, carlaJackClientName );

}

void konfytPatchEngine::panic(bool p)
{
    // indicate panic situation to Jack
    jack->panic(p);
}

void konfytPatchEngine::setProject(konfytProject *project)
{
    this->currentProject = project;
}

// Reload current patch (e.g. if patch changed).
void konfytPatchEngine::reloadPatch()
{
    loadPatch( currentPatch );
}

// Ensure patch and all layers are unloaded from their respective engines
void konfytPatchEngine::unloadPatch(konfytPatch *patch)
{
    if ( !patches.contains(patch) ) { return; }

    QList<konfytPatchLayer> l = patch->getLayerItems();
    for (int i=0; i<l.count(); i++) {
        konfytPatchLayer layer = l[i];
        unloadLayer(patch, &layer);
    }

    patches.removeAll(patch);
    if (currentPatch == patch) { currentPatch = NULL; }
}

// Load patch, replacing the current patch
bool konfytPatchEngine::loadPatch(konfytPatch *newPatch)
{
    if (newPatch == NULL) { return false; }

    bool r = true; // return value. Set to false when an error occured somewhere.

    bool patchIsNew = false;
    if ( !patches.contains(newPatch) ) {
        patchIsNew = true;
        patches.append(newPatch);
    }
    currentPatch = newPatch;

    // ---------------------------------------------------
    // Carla plugin layers:
    // ---------------------------------------------------

    // For each carla plugin in the patch...
    QList<konfytPatchLayer> pluginList = currentPatch->getPluginLayerList();
    for (int i=0; i<pluginList.count(); i++) {
        // If layer indexInEngine is -1, the layer hasn't been loaded yet.
        konfytPatchLayer layer = pluginList[i];
        if (patchIsNew) { layer.carlaPluginData.indexInEngine = -1; }
        if ( layer.carlaPluginData.indexInEngine == -1 ) {
            // Load in Carla engine
            int ID = -1;
            if ( layer.carlaPluginData.pluginType == KonfytCarlaPluginType_LV2 ) {

                // Old experimental code is in a huge comment in konfytCarlaEngine

            } else if ( layer.carlaPluginData.pluginType == KonfytCarlaPluginType_SFZ ) {

                ID = carlaEngine->addSFZ( layer.carlaPluginData.path );

            } else if ( layer.carlaPluginData.pluginType == KonfytCarlaPluginType_Internal ) {

                // Old experimental code is in a huge comment in konfytCarlaEngine
            }

            if (ID < 0) {
                layer.setErrorMessage("Failed to load Carla plugin " + layer.carlaPluginData.path);
                r = false;
            } else {
                layer.carlaPluginData.indexInEngine = ID;
                layer.carlaPluginData.name = carlaEngine->pluginName(ID);
                // Add to jack engine

                // Find the plugin midi input port
                layer.carlaPluginData.midi_in_port = carlaEngine->midiInPort(ID);

                // Find the plugin audio output ports
                QStringList audioLR = carlaEngine->audioOutPorts(ID);
                layer.carlaPluginData.audio_out_port_left = audioLR[0];
                layer.carlaPluginData.audio_out_port_right = audioLR[1];

                // The plugin object now contains the midi input port and
                // audio left and right output ports. Give this to Jack, which will:
                // - create a midi output port and connect it to the plugin midi input port,
                // - create audio input ports and connect it to the plugin audio output ports.
                // - assigns the midi filter
                jack->addPluginPortsAndConnect( layer.carlaPluginData );

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
    QList<konfytPatchLayer> sflist = currentPatch->getSfLayerList();
    for (int i=0; i<sflist.count(); i++) {
        // If layer indexInEngine is -1, layer hasn't been loaded yet.
        konfytPatchLayer layer = sflist[i];
        if (patchIsNew) { layer.sfData.indexInEngine = -1; }
        if ( layer.sfData.indexInEngine == -1 ) {
            // Load in Fluidsynth engine
            int ID = fluidsynthEngine->addSoundfontProgram( layer.sfData.program );
            if (ID < 0) {
                layer.setErrorMessage("Failed to load soundfont.");
                r = false;
            } else {
                layer.sfData.indexInEngine = ID;
                // Add to Jack engine (this also assigns the midi filter)
                jack->addSoundfont( layer.sfData );
                // Gain, solo, mute and bus are set later in refreshAllGainsAndRouting()
            }
            // Update layer in patch
            currentPatch->replaceLayer(layer);
        }

    }



    // ---------------------------------------------------
    // Midi output ports:
    // ---------------------------------------------------

    // Set midi filters for midi output ports
    QList<layerMidiOutStruct> l = currentPatch->getMidiOutputPortList_struct();
    for (int i=0; i<l.count(); i++) {
        prjMidiOutPort projectPort = currentProject->midiOutPort_getPort( l[i].portIdInProject );
        jack->setPortFilter(KonfytJackPortType_MidiOut, projectPort.jackPort, l[i].filter);
    }

    // ---------------------------------------------------
    // The rest
    // ---------------------------------------------------

    // Activate all the necessary ports
    jack->activatePortsForPatch( currentPatch, currentProject );

    // All programs of the patch are now loaded.
    // Now, set all the gains.
    refreshAllGainsAndRouting();

    return r;
}

konfytPatchLayer konfytPatchEngine::addProgramLayer(konfytSoundfontProgram newProgram)
{
    if (currentPatch == NULL) { return; }

    konfytPatchLayer g = currentPatch->addProgram(newProgram);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    currentPatch->setLayerBus(&g, currentProject->audioBus_getFirstBusId(-1));

    reloadPatch();
    // When loading the patch, the LayerItem.sfData.indexInEngine is set.
    // In order for the returned LayerItem to contain this correct info,
    // we need to retrieve an updated one from the engine's patch.
    g = currentPatch->getLayerItem(g);
    return g;
}

void konfytPatchEngine::removeLayer(konfytPatchLayer* item)
{
    Q_ASSERT( currentPatch != NULL );

    removeLayer(currentPatch, item);
}

void konfytPatchEngine::removeLayer(konfytPatch *patch, konfytPatchLayer *item)
{
    // Unload from respective engine
    unloadLayer(patch, item);

    patch->removeLayer(item);
    if (patch == currentPatch) { reloadPatch(); }
}

void konfytPatchEngine::unloadLayer(konfytPatch *patch, konfytPatchLayer *item)
{
    konfytPatchLayer layer = patch->getLayerItem( *item );
    if (layer.getLayerType() == KonfytLayerType_SoundfontProgram) {
        if (layer.sfData.indexInEngine >= 0) {
            // First remove from jack
            jack->removeSoundfont(layer.sfData.indexInEngine);
            // Then from fluidsynthEngine
            fluidsynthEngine->removeSoundfontProgram(layer.sfData.indexInEngine);
            // Set unloaded in patch
            layer.sfData.indexInEngine = -1;
            patch->replaceLayer(layer);
        }
    } else if (layer.getLayerType() == KonfytLayerType_CarlaPlugin) {
        if (layer.carlaPluginData.indexInEngine >= 0) {
            // First remove from jack
            jack->removePlugin(layer.carlaPluginData.indexInEngine);
            // Then from carlaEngine
            carlaEngine->removeSFZ(layer.carlaPluginData.indexInEngine);
            // Set unloaded in patch
            layer.carlaPluginData.indexInEngine = -1;
            patch->replaceLayer(layer);
        }
    }
}

konfytPatchLayer konfytPatchEngine::reloadLayer(konfytPatchLayer *item)
{
    unloadLayer(currentPatch, item);
    reloadPatch();
    // Note that some IDs in the layer item might have changed when it was loaded again.
    // Get the new layer from the patch and return it so it can be updated elsewhere also.
    return currentPatch->getLayerItem(*item);
}


konfytPatchLayer konfytPatchEngine::addSfzLayer(QString path)
{
    Q_ASSERT( currentPatch != NULL );

    layerCarlaPluginStruct plugin = layerCarlaPluginStruct();
    plugin.pluginType = KonfytCarlaPluginType_SFZ;
    plugin.gain = DEFAULT_GAIN_FOR_NEW_LAYER;
    plugin.mute = false;
    plugin.solo = false;
    plugin.name = "sfz";
    plugin.path = path;

    // Add the plugin to the patch
    konfytPatchLayer g = currentPatch->addPlugin(plugin);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    currentPatch->setLayerBus(&g, currentProject->audioBus_getFirstBusId(-1));

    reloadPatch();


    // Note that the LayerItem g we created above does not have the correct
    // pluginInCarlaEngine index. That is only assigned when loading the patch,
    // in loadPatch.
    // But it does have a unique id, which is how we can now get the 'real'
    // LayerItem from the loaded patch:

    return currentPatch->getLayerItem(g);
}

konfytPatchLayer konfytPatchEngine::addLV2Layer(QString path)
{
    Q_ASSERT( currentPatch != NULL );

    layerCarlaPluginStruct plugin = layerCarlaPluginStruct();
    plugin.pluginType = KonfytCarlaPluginType_LV2;
    plugin.gain = DEFAULT_GAIN_FOR_NEW_LAYER;
    plugin.mute = false;
    plugin.solo = false;
    plugin.name = "lv2";
    plugin.path = path;

    // Add the plugin to the patch
    konfytPatchLayer g = currentPatch->addPlugin(plugin);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    currentPatch->setLayerBus(&g, currentProject->audioBus_getFirstBusId(-1));

    reloadPatch();

    // Note that the LayerItem g we created above does not have the correct
    // pluginInCarlaEngine index. That is only assigned when loading the patch,
    // in loadPatch.
    // But it does have a unique id, which is how we can now get the 'real'
    // LayerItem from the loaded patch:

    return currentPatch->getLayerItem(g);
}

konfytPatchLayer konfytPatchEngine::addCarlaInternalLayer(QString URI)
{
    Q_ASSERT( currentPatch != NULL );

    layerCarlaPluginStruct plugin = layerCarlaPluginStruct();
    plugin.pluginType = KonfytCarlaPluginType_Internal;
    plugin.gain = DEFAULT_GAIN_FOR_NEW_LAYER;
    plugin.mute = false;
    plugin.solo = false;
    plugin.name = URI;
    plugin.path = URI;

    // Add the plugin to the patch
    konfytPatchLayer g = currentPatch->addPlugin(plugin);

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    currentPatch->setLayerBus(&g, currentProject->audioBus_getFirstBusId(-1));

    reloadPatch();

    // Note that the LayerItem g we created above does not have the correct
    // pluginInCarlaEngine index. That is only assigned when loading the patch,
    // in loadPatch.
    // But it does have a unique id, which is how we can now get the 'real'
    // LayerItem from the loaded patch:

    return currentPatch->getLayerItem(g);
}


/* Converts gain between 0 and 1 from linear to an exponential function that is more
 * suited for human hearing. Input is clipped between 0 and 1. */
float konfytPatchEngine::convertGain(float linearGain)
{
    if (linearGain < 0) { linearGain = 0; }
    if (linearGain > 1) { linearGain = 1; }

    return linearGain*linearGain*linearGain; // x^3
}


/* Ensure that all engine gains, solos, mutes and audio out routing are set
 * according to current patch and masterGain. */
void konfytPatchEngine::refreshAllGainsAndRouting()
{
    if (currentPatch == NULL) { return; }

    QList<konfytPatchLayer> l = currentPatch->getLayerItems();
    for (int i=0; i<l.count(); i++) {

        konfytPatchLayer layer = l[i];

        if (layer.hasError()) {
            continue;
        }

        prjAudioBus bus;
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

        konfytLayerType type = layer.getLayerType();
        if (type ==  KonfytLayerType_SoundfontProgram) {

            layerSoundfontStruct sfData = layer.sfData;
            // Gain = layer gain * master gain
            fluidsynthEngine->setGain( sfData.indexInEngine, convertGain(sfData.gain*masterGain) );
            // Solo and mute in jack client
            jack->setSoundfontSolo( sfData.indexInEngine, sfData.solo );
            jack->setSoundfontMute( sfData.indexInEngine, sfData.mute );
            // Set routing to bus
            jack->setSoundfontRouting( sfData.indexInEngine, bus.leftJackPort, bus.rightJackPort );

        } else if (type == KonfytLayerType_CarlaPlugin) {

            layerCarlaPluginStruct pluginData = layer.carlaPluginData;
            // Gain = layer gain * master gain
            carlaEngine->setGain( pluginData.indexInEngine, convertGain(pluginData.gain*masterGain) );
            // Set solo and mute in jack client
            jack->setPluginSolo(pluginData.indexInEngine, pluginData.solo);
            jack->setPluginMute(pluginData.indexInEngine, pluginData.mute);
            // Set routing to bus
            jack->setPluginRouting(pluginData.indexInEngine, bus.leftJackPort, bus.rightJackPort);

        } else if (type == KonfytLayerType_MidiOut) {

            // Set solo and mute in jack client
            layerMidiOutStruct portData = layer.midiOutputPortData;
            prjMidiOutPort projectPort = currentProject->midiOutPort_getPort( portData.portIdInProject );
            jack->setPortSolo(KonfytJackPortType_MidiOut, projectPort.jackPort, portData.solo);
            jack->setPortMute(KonfytJackPortType_MidiOut, projectPort.jackPort, portData.mute);

        } else if (type == KonfytLayerType_AudioIn) {

            // Set gain, solo, mute and destination port (bus) in jack client

            // The port number in audioInLayerStruct refers to a stereo port pair index in the project.
            // The bus number in audioInLayerStruct refers to a bus in the project with a left and right jack port.
            // We have to retrieve the port pair and bus from the project in order to get the left and right port Jack port numbers.
            layerAudioInStruct audioPortData = layer.audioInPortData;
            prjAudioInPort portPair = currentProject->audioInPort_getPort(audioPortData.portIdInProject);
            // Left channel
            jack->setPortSolo(KonfytJackPortType_AudioIn, portPair.leftJackPort, audioPortData.solo);
            jack->setPortMute(KonfytJackPortType_AudioIn, portPair.leftJackPort, audioPortData.mute);
            jack->setPortGain(KonfytJackPortType_AudioIn, portPair.leftJackPort, convertGain(audioPortData.gain*masterGain));
            jack->setPortRouting(KonfytJackPortType_AudioIn, portPair.leftJackPort, bus.leftJackPort);
            // Right channel
            jack->setPortSolo(KonfytJackPortType_AudioIn, portPair.rightJackPort, audioPortData.solo);
            jack->setPortMute(KonfytJackPortType_AudioIn, portPair.rightJackPort, audioPortData.mute);
            jack->setPortGain(KonfytJackPortType_AudioIn, portPair.rightJackPort, convertGain(audioPortData.gain*masterGain));
            jack->setPortRouting(KonfytJackPortType_AudioIn, portPair.rightJackPort, bus.rightJackPort);

        } else if (type == KonfytLayerType_Uninitialized) {

            error_abort("refreshAllGains(): layer type uninitialized.");

        } else {

            error_abort("patchEngine::refreshAllGains(): unknown layer type.");

        }
    }

    // NOTE: Below was used previously when fluidsynth engine still handled its own soloing.
    // Now synchronise the solo across the engines.
    bool solo = false;
    //if (fluidsynthEngine->getSoloFlagInternal()) { solo = true; }
    if (jack->getSoloFlagInternal()) { solo = true; }
    //fluidsynthEngine->setSoloFlagExternal(solo);
    jack->setSoloFlagExternal(solo);

}





// Return the current patch
konfytPatch *konfytPatchEngine::getPatch()
{
    return currentPatch;
}


float konfytPatchEngine::getMasterGain()
{
    return masterGain;
}

void konfytPatchEngine::setMasterGain(float newGain)
{
    masterGain = newGain;
    refreshAllGainsAndRouting();
}

int konfytPatchEngine::getNumLayers()
{
    Q_ASSERT( currentPatch != NULL );

    return currentPatch->getNumLayers();
}

void konfytPatchEngine::setLayerGain(konfytPatchLayer *layerItem, float newGain)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setLayerGain(layerItem, newGain);
    refreshAllGainsAndRouting();
}

void konfytPatchEngine::setLayerGain(int layerIndex, float newGain)
{
    Q_ASSERT( currentPatch != NULL );

    QList<konfytPatchLayer> l =  currentPatch->getLayerItems();
    if ( (layerIndex >= 0) && (layerIndex < l.count()) ) {
        konfytPatchLayer g = l.at(layerIndex);
        currentPatch->setLayerGain(&g, newGain);
        refreshAllGainsAndRouting();
    } else {
        // Logic error somewhere else.
        error_abort("setLayerGain: layerIndex " + n2s(layerIndex) + " out of range.");
    }
}

void konfytPatchEngine::setLayerSolo(konfytPatchLayer *layerItem, bool solo)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setLayerSolo(layerItem, solo);
    refreshAllGainsAndRouting();
}

/* Set layer solo, using layer index as parameter. */
void konfytPatchEngine::setLayerSolo(int layerIndex, bool solo)
{
    Q_ASSERT( currentPatch != NULL );

    QList<konfytPatchLayer> l =  currentPatch->getLayerItems();
    if ( (layerIndex >= 0) && (layerIndex < l.count()) ) {
        konfytPatchLayer g = l.at(layerIndex);
        setLayerSolo( &g, solo );
    } else {
        // Logic error somewhere else.
        error_abort("setLayerSolo: layerIndex " + n2s(layerIndex) + " out of range.");
    }
}



void konfytPatchEngine::setLayerMute(konfytPatchLayer *layerItem, bool mute)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setLayerMute(layerItem, mute);
    refreshAllGainsAndRouting();
}

/* Set layer mute, using layer index as parameter. */
void konfytPatchEngine::setLayerMute(int layerIndex, bool mute)
{
    Q_ASSERT( currentPatch != NULL );

    QList<konfytPatchLayer> l =  currentPatch->getLayerItems();
    if ( (layerIndex >= 0) && (layerIndex < l.count()) ) {
        konfytPatchLayer g = l.at(layerIndex);
        currentPatch->setLayerMute(&g, mute);
        refreshAllGainsAndRouting();
    } else {
        // Logic error somewhere else.
        error_abort( "setLayerMute: layerIndex " + n2s(layerIndex) + " out of range." );
    }
}

void konfytPatchEngine::setLayerBus(konfytPatchLayer *layerItem, int bus)
{
    Q_ASSERT( currentPatch != NULL );
    setLayerBus(currentPatch, layerItem, bus);
}

void konfytPatchEngine::setLayerBus(konfytPatch *patch, konfytPatchLayer *layerItem, int bus)
{
    patch->setLayerBus(layerItem, bus);
    if (patch == currentPatch) { refreshAllGainsAndRouting(); }
}





void konfytPatchEngine::setLayerFilter(konfytPatchLayer *layerItem, konfytMidiFilter filter)
{
    Q_ASSERT( currentPatch != NULL );

    // Set filter in patch
    currentPatch->setLayerFilter(layerItem, filter);

    // And also in the respective engine.
    if (layerItem->getLayerType() == KonfytLayerType_SoundfontProgram) {

        jack->setSoundfontMidiFilter(layerItem->sfData.indexInEngine, filter);

    } else if (layerItem->getLayerType() == KonfytLayerType_CarlaPlugin) {

        this->jack->setPluginMidiFilter( layerItem->carlaPluginData.indexInEngine,
                                         layerItem->carlaPluginData.midiFilter);

    } else if (layerItem->getLayerType() == KonfytLayerType_MidiOut) {

        layerMidiOutStruct layerPort = layerItem->midiOutputPortData;
        prjMidiOutPort projectPort = currentProject->midiOutPort_getPort( layerPort.portIdInProject );
        this->jack->setPortFilter( KonfytJackPortType_MidiOut, projectPort.jackPort, layerPort.filter );

    }
}



void konfytPatchEngine::setPatchName(QString newName)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setName(newName);
}

QString konfytPatchEngine::getPatchName()
{
    Q_ASSERT( currentPatch != NULL );

    return currentPatch->getName();
}

void konfytPatchEngine::setPatchNote(QString newNote)
{
    Q_ASSERT( currentPatch != NULL );

    currentPatch->setNote(newNote);
}

QString konfytPatchEngine::getPatchNote()
{
    Q_ASSERT( currentPatch != NULL );

    return currentPatch->getNote();
}

konfytPatchLayer konfytPatchEngine::addMidiOutPortToPatch(int port)
{
    Q_ASSERT( currentPatch != NULL );

    konfytPatchLayer g = currentPatch->addMidiOutputPort(port);
    reloadPatch();
    return g;
}

konfytPatchLayer konfytPatchEngine::addAudioInPortToPatch(int port)
{
    Q_ASSERT( currentPatch != NULL );

    konfytPatchLayer g = currentPatch->addAudioInPort( port );

    // The bus defaults to 0, but the project may not have a bus 0.
    // Set the layer bus to the first one in the project.
    currentPatch->setLayerBus(&g, currentProject->audioBus_getFirstBusId(-1));

    reloadPatch();
    return g;
}

// Print error message to stdout, and abort app.
void konfytPatchEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: patchEngine:" << msg.toLocal8Bit().constData();
    abort();
}

