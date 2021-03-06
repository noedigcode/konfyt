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

#include "konfytJackEngine.h"

#include <iostream>


KonfytJackEngine::KonfytJackEngine(QObject *parent) :
    QObject(parent)
{
    initMidiClosureEvents();
}

KonfytJackEngine::~KonfytJackEngine()
{
    free(fadeOutValues);
}

/* Set panicCmd. The JACK process callback will behave accordingly. */
void KonfytJackEngine::panic(bool p)
{
    panicCmd = p;
}

void KonfytJackEngine::timerEvent(QTimerEvent* /*event*/)
{
    // JACK port connections
    if (connectCallback || registerCallback) {
        connectCallback = false;
        registerCallback = false;
        refreshAllPortsConnections();
        emit jackPortRegisteredOrConnected();
    }

    // Received MIDI events
    extractedMidiRx.append(midiRxBuffer.readAll());
    if (!extractedMidiRx.isEmpty()) {
        emit midiEventsReceived();
    }

    // Received audio data
    extractedAudioRx.append(audioRxBuffer.readAll());
    if (!extractedAudioRx.isEmpty()) {
        emit audioEventsReceived();
    }
}

void KonfytJackEngine::startTimer()
{
    this->timer.start(20, this);
}

void KonfytJackEngine::refreshAllPortsConnections()
{
    if (!clientIsActive()) { return; }

    // Refresh connections to each midi input port
    foreach (KfJackMidiPort* port, midiInPorts) {
        refreshConnections(port->jackPointer, port->connectionList, true);
    }

    // For each midi output port to external apps, refresh connections to all their clients
    foreach (KfJackMidiPort* port, midiOutPorts) {
        refreshConnections(port->jackPointer, port->connectionList, false);
    }

    // Refresh connections for plugin midi and audio ports
    foreach (KfJackPluginPorts* pluginPort, pluginPorts) {
        // Midi out
        refreshConnections(pluginPort->midi->jackPointer,
                           pluginPort->midi->connectionList, false);
        // Audio in left
        refreshConnections(pluginPort->audioInLeft->jackPointer,
                           pluginPort->audioInLeft->connectionList, true);
        // Audio in right
        refreshConnections(pluginPort->audioInRight->jackPointer,
                           pluginPort->audioInRight->connectionList, true);
    }

    // Refresh connections for audio input ports
    foreach (KfJackAudioPort* port, audioInPorts) {
        refreshConnections(port->jackPointer, port->connectionList, true);
    }

    // Refresh connections for audio output ports (aka buses)
    foreach (KfJackAudioPort* port, audioOutPorts) {
        refreshConnections(port->jackPointer, port->connectionList, false);
    }

    // Refresh other JACK connections
    foreach (const KonfytJackConPair &p, otherConsList) {
        jack_connect(mJackClient, p.srcPort.toLocal8Bit(), p.destPort.toLocal8Bit());
    }

}

void KonfytJackEngine::refreshConnections(jack_port_t *jackPort, QStringList clients, bool inNotOut)
{
    const char* src = nullptr;
    const char* dest = nullptr;
    const char* portname = jack_port_name(jackPort);
    if (inNotOut) {
        dest = portname;
    } else {
        src = portname;
    }
    foreach (QString s, clients) {
        const char* clientname = s.toLocal8Bit().constData();
        if (inNotOut) {
            src = clientname;
        } else {
            dest = clientname;
        }
        jack_connect(mJackClient, src, dest);
    }
}

/* Add new soundfont ports. Also assigns MIDI filter. */
KfJackPluginPorts* KonfytJackEngine::addSoundfont(KfFluidSynth *fluidSynth)
{
    /* Soundfonts use the same structures as SFZ plugins for now, but are much simpler
     * as midi is given to the fluidsynth engine and audio is recieved from it without
     * external Jack connections. */

    KfJackPluginPorts* p = new KfJackPluginPorts();
    p->audioInLeft = new KfJackAudioPort();
    p->audioInRight = new KfJackAudioPort();

    // Because our audio is not received from jack audio ports, we have to allocate
    // the buffers now so fluidsynth can write to them in the jack process callback.
    p->audioInLeft->buffer = malloc(sizeof(jack_default_audio_sample_t)*nframes);
    p->audioInRight->buffer = malloc(sizeof(jack_default_audio_sample_t)*nframes);

    p->midi = new KfJackMidiPort(); // Dummy port for note records, etc.
    p->fluidSynthInEngine = fluidSynth;

    p->midiRoute = addMidiRoute();
    p->audioLeftRoute = addAudioRoute();
    p->audioRightRoute = addAudioRoute();

    // Only MIDI route is used to activate/deactivate the sound. Audio routes are
    // always active.
    setAudioRouteActive(p->audioLeftRoute, true);
    setAudioRouteActive(p->audioRightRoute, true);

    // Pre-set audio route sources and MIDI route destination
    p->audioLeftRoute->source = p->audioInLeft;
    p->audioRightRoute->source = p->audioInRight;

    p->midiRoute->destFluidsynthID = p->fluidSynthInEngine;
    p->midiRoute->destIsJackPort = false;
    p->midiRoute->destPort = p->midi;

    fluidsynthPorts.append(p);

    return p;
}

void KonfytJackEngine::removeSoundfont(KfJackPluginPorts *p)
{
    pauseJackProcessing(true);

    // Delete all objects created in addSoundfont()

    fluidsynthPorts.removeAll(p);
    free(p->audioInLeft->buffer);
    free(p->audioInRight->buffer);
    removeMidiRoute(p->midiRoute);
    removeAudioRoute(p->audioLeftRoute);
    removeAudioRoute(p->audioRightRoute);
    delete p->audioInLeft;
    delete p->audioInRight;
    delete p->midi;
    delete p;

    pauseJackProcessing(false);
}

/* For the specified ports spec, create a new MIDI output port and left and
 * right audio input ports, combined in a plugin ports struct. The strings in the
 * spec are used to add to the created ports auto-connect lists and the MIDI
 * filter is applied to the MIDI port.
 * A unique ID is returned. */
KfJackPluginPorts* KonfytJackEngine::addPluginPortsAndConnect(const KonfytJackPortsSpec &spec)
{
    KfJackMidiPort* midiPort = new KfJackMidiPort();
    KfJackAudioPort* alPort = new KfJackAudioPort();
    KfJackAudioPort* arPort = new KfJackAudioPort();

    // Add a new midi output port which will be connected to the plugin midi input
    midiPort->jackPointer = registerJackMidiPort(spec.name, false);
    if (midiPort->jackPointer == nullptr) {
        print("Failed to create JACK MIDI output port '" + spec.name + "' for plugin.");
    }
    midiPort->connectionList.append( spec.midiOutConnectTo );

    // Add left audio input port where we will receive plugin audio
    QString nameL = spec.name + "_in_L";
    alPort->jackPointer = registerJackAudioPort(nameL, true);
    if (alPort->jackPointer == nullptr) {
        print("Failed to create left audio input port '" + nameL + "' for plugin.");
    }
    alPort->connectionList.append( spec.audioInLeftConnectTo );

    // Add right audio input port
    QString nameR = spec.name + "_in_R";
    arPort->jackPointer = registerJackAudioPort(nameR, true);
    if (arPort->jackPointer == nullptr) {
        print("Failed to create right audio input port '" + nameR + "' for plugin.");
    }
    arPort->connectionList.append( spec.audioInRightConnectTo );

    KfJackPluginPorts* p = new KfJackPluginPorts();
    p->midi = midiPort;
    p->audioInLeft = alPort;
    p->audioInRight = arPort;

    // Add routes
    p->midiRoute = addMidiRoute();
    p->audioLeftRoute = addAudioRoute();
    p->audioRightRoute = addAudioRoute();

    // Only MIDI route is used to set plugin active/inactive. Audio ports always
    // active.
    setAudioRouteActive(p->audioLeftRoute, true);
    setAudioRouteActive(p->audioRightRoute, true);

    // Pre-set route sources/dests
    p->midiRoute->destIsJackPort = true;
    p->midiRoute->destPort = midiPort;
    p->midiRoute->filter = spec.midiFilter;

    p->audioLeftRoute->source = p->audioInLeft;
    p->audioRightRoute->source = p->audioInRight;

    pauseJackProcessing(true);
    pluginPorts.append(p);
    pauseJackProcessing(false);

    return p;
}

void KonfytJackEngine::removePlugin(KfJackPluginPorts *p)
{
    pauseJackProcessing(true);

    // Remove everything created in addPluginPortsAndConnect()

    // Remove routes
    removeMidiRoute(p->midiRoute);
    removeAudioRoute(p->audioLeftRoute);
    removeAudioRoute(p->audioRightRoute);

    pluginPorts.removeAll(p);

    jack_port_unregister(mJackClient, p->midi->jackPointer);
    delete p->midi;

    jack_port_unregister(mJackClient, p->audioInLeft->jackPointer);
    delete p->audioInLeft;

    jack_port_unregister(mJackClient, p->audioInRight->jackPointer);
    delete p->audioInRight;

    delete p;

    pauseJackProcessing(false);
}

void KonfytJackEngine::setSoundfontMidiFilter(KfJackPluginPorts *p, KonfytMidiFilter filter)
{
    p->midiRoute->filter = filter;
}

void KonfytJackEngine::setSoundfontActive(KfJackPluginPorts *p, bool active)
{
    setMidiRouteActive(p->midiRoute, active);
}

void KonfytJackEngine::setPluginMidiFilter(KfJackPluginPorts *p, KonfytMidiFilter filter)
{
    p->midiRoute->filter = filter;
}

void KonfytJackEngine::setPluginActive(KfJackPluginPorts *p, bool active)
{
    setMidiRouteActive(p->midiRoute, active);
}

void KonfytJackEngine::setPluginGain(KfJackPluginPorts *p, float gain)
{
    p->audioLeftRoute->gain = gain;
    p->audioRightRoute->gain = gain;
}

void KonfytJackEngine::setSoundfontRouting(KfJackPluginPorts *p, KfJackMidiPort *midiInPort, KfJackAudioPort *leftPort, KfJackAudioPort *rightPort)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    // MIDI route destination has already been set in addSoundfont().
    p->midiRoute->source = midiInPort;

    // Audio route sources have already been set in addSoundfont().
    // Set left audio route destination
    p->audioLeftRoute->dest = leftPort;
    // Right audio route destination
    p->audioRightRoute->dest = rightPort;

    pauseJackProcessing(false);
}

void KonfytJackEngine::setPluginRouting(KfJackPluginPorts *p, KfJackMidiPort *midiInPort, KfJackAudioPort *leftPort, KfJackAudioPort *rightPort)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    p->midiRoute->source = midiInPort;
    p->audioLeftRoute->dest = leftPort;
    p->audioRightRoute->dest = rightPort;

    pauseJackProcessing(false);
}

KfJackMidiRoute *KonfytJackEngine::getPluginMidiRoute(KfJackPluginPorts *p)
{
    KfJackMidiRoute* ret = nullptr;
    if (p) { ret = p->midiRoute; }
    return ret;
}

QList<KfJackAudioRoute *> KonfytJackEngine::getPluginAudioRoutes(KfJackPluginPorts *p)
{
    QList<KfJackAudioRoute*> ret;
    if (p) {
        ret.append(p->audioLeftRoute);
        ret.append(p->audioRightRoute);
    }
    return ret;
}

void KonfytJackEngine::removeAllAudioInAndOutPorts()
{
    pauseJackProcessing(true);

    foreach (KfJackAudioPort* port, audioOutPorts) {
        removeAudioPort(port);
    }
    foreach (KfJackAudioPort* port, audioInPorts) {
        removeAudioPort(port);
    }

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAllMidiInAndOutPorts()
{
    pauseJackProcessing(true);

    foreach (KfJackMidiPort* port, midiInPorts) {
        removeMidiPort(port);
    }
    foreach (KfJackMidiPort* port, midiOutPorts) {
        removeMidiPort(port);
    }

    pauseJackProcessing(false);
}

void KonfytJackEngine::clearPortClients(KfJackMidiPort *port)
{
    if (port) {
        QStringList clients = port->connectionList;
        foreach (QString client, clients) {
            removeAndDisconnectPortClient(port, client);
        }
    } else {
        error_abort("clearPortClients: MIDI port null.");
    }
}

void KonfytJackEngine::clearPortClients(KfJackAudioPort *port)
{
    if (port) {
        foreach (QString client, port->connectionList) {
            removeAndDisconnectPortClient(port, client);
        }
    } else {
        error_abort("clearPortClients: Audio port null.");
    }
}

/* Set the source/destination port to NULL for all routes that use the specified
 * port. */
void KonfytJackEngine::removePortFromAllRoutes(KfJackMidiPort *port)
{
    pauseJackProcessing(true);

    for (int i=0; i < midiRoutes.count(); i++) {
        if (midiRoutes[i]->source == port) { midiRoutes[i]->source = NULL; }
        if (midiRoutes[i]->destPort == port) { midiRoutes[i]->destPort = NULL; }
    }

    pauseJackProcessing(false);
}

void KonfytJackEngine::removePortFromAllRoutes(KfJackAudioPort *port)
{
    pauseJackProcessing(true);

    for (int i=0; i < audioRoutes.count(); i++) {
        if (audioRoutes[i]->source == port) { audioRoutes[i]->source = NULL; }
        if (audioRoutes[i]->dest == port) { audioRoutes[i]->dest = NULL; }
    }

    pauseJackProcessing(false);
}

void KonfytJackEngine::addPortClient(KfJackMidiPort *port, QString newClient)
{
    if (!clientIsActive()) { return; }

    if (port) {
        port->connectionList.append(newClient);
        refreshAllPortsConnections();
    } else {
        error_abort("addPortClient: MIDI port null.");
    }
}

void KonfytJackEngine::addPortClient(KfJackAudioPort *port, QString newClient)
{
    if (!clientIsActive()) { return; }

    if (port) {
        port->connectionList.append(newClient);
        refreshAllPortsConnections();
    } else {
        error_abort("addPortClient: Audio port null.");
    }
}

void KonfytJackEngine::removeAndDisconnectPortClient(KfJackMidiPort *port, QString client)
{
    if (!clientIsActive()) { return; }

    if (!port) {
        error_abort("removeAndDisconnectPortClient: MIDI port null.");
        return;
    }

    if (!port->connectionList.contains(client)) { return; }

    bool portIsInput = midiInPorts.contains(port);
    const char* portName = jack_port_name(port->jackPointer);

    pauseJackProcessing(true);

    // Disconnect client from port in JACK
    int err = 0;
    if (portIsInput) {
        err = jack_disconnect(mJackClient, client.toLocal8Bit().constData(), portName);
    } else {
        err = jack_disconnect(mJackClient, portName, client.toLocal8Bit().constData());
    }
    // Remove client from port's list
    port->connectionList.removeAll(client);

    pauseJackProcessing(false);

    if (err) {
        print("Failed to disconnect JACK MIDI port client.");
    }
    refreshAllPortsConnections();
}

void KonfytJackEngine::removeAndDisconnectPortClient(KfJackAudioPort *port, QString client)
{
    if (!clientIsActive()) { return; }

    if (!port) {
        error_abort("removeAndDisconnectPortClient: Audio port null.");
        return;
    }

    if (!port->connectionList.contains(client)) { return; }

    bool portIsInput = audioInPorts.contains(port);
    const char* portName = jack_port_name(port->jackPointer);

    pauseJackProcessing(true);

    // Disconnect client from port in JACK
    int err = 0;
    if (portIsInput) {
        err = jack_disconnect(mJackClient, client.toLocal8Bit().constData(), portName);
    } else {
        err = jack_disconnect(mJackClient, portName, client.toLocal8Bit().constData());
    }
    // Remove client from port's list
    port->connectionList.removeAll(client);

    pauseJackProcessing(false);

    if (err) {
        print("Failed to disconnect JACK audio port client.");
    }
    refreshAllPortsConnections();
}

void KonfytJackEngine::setPortFilter(KfJackMidiPort *port, KonfytMidiFilter filter)
{
    if (!clientIsActive()) { return; }

    if (port) {
        port->filter = filter;
    } else {
        error_abort("setPortFilter: Invalid port.");
    }
}

KfJackAudioRoute *KonfytJackEngine::addAudioRoute(KfJackAudioPort *sourcePort, KfJackAudioPort *destPort)
{
    KfJackAudioRoute* route = addAudioRoute();
    setAudioRoute(route, sourcePort, destPort);
    return route;
}

KfJackAudioRoute* KonfytJackEngine::addAudioRoute()
{
    if (!clientIsActive()) { return nullptr; }

    pauseJackProcessing(true);

    KfJackAudioRoute* route = new KfJackAudioRoute();

    audioRoutes.append(route);

    pauseJackProcessing(false);

    return route;
}

void KonfytJackEngine::setAudioRoute(KfJackAudioRoute *route, KfJackAudioPort *sourcePort, KfJackAudioPort *destPort)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    route->source = sourcePort;
    route->dest = destPort;

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAudioRoute(KfJackAudioRoute* route)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    audioRoutes.removeAll(route);
    delete route;

    pauseJackProcessing(false);
}

void KonfytJackEngine::setAudioRouteActive(KfJackAudioRoute *route, bool active)
{
    if (!clientIsActive()) { return; }

    route->active = active;
}

void KonfytJackEngine::setAudioRouteGain(KfJackAudioRoute *route, float gain)
{
    if (!clientIsActive()) { return; }

    route->gain = gain;
}

KfJackMidiRoute *KonfytJackEngine::addMidiRoute(KfJackMidiPort *sourcePort, KfJackMidiPort *destPort)
{
    KfJackMidiRoute* route = addMidiRoute();
    setMidiRoute(route, sourcePort, destPort);
    return route;
}

KfJackMidiRoute* KonfytJackEngine::addMidiRoute()
{
    if (!clientIsActive()) { return nullptr; }

    pauseJackProcessing(true);

    KfJackMidiRoute* route = new KfJackMidiRoute();

    midiRoutes.append(route);

    pauseJackProcessing(false);

    return route;
}

void KonfytJackEngine::setMidiRoute(KfJackMidiRoute *route, KfJackMidiPort *sourcePort, KfJackMidiPort *destPort)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    route->source = sourcePort;
    route->destPort = destPort;

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeMidiRoute(KfJackMidiRoute *route)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    midiRoutes.removeAll(route);
    delete route;

    pauseJackProcessing(false);
}

void KonfytJackEngine::setMidiRouteActive(KfJackMidiRoute *route, bool active)
{
    if (!clientIsActive()) { return; }

    route->active = active;
}

void KonfytJackEngine::setRouteMidiFilter(KfJackMidiRoute *route, KonfytMidiFilter filter)
{
    route->filter = filter;
}

bool KonfytJackEngine::sendMidiEventsOnRoute(KfJackMidiRoute *route, QList<KonfytMidiEvent> events)
{
    bool success = true;
    foreach (KonfytMidiEvent event, events) {
        success = route->eventsTxBuffer.stash(event);
        if (!success) { break; }
    }
    route->eventsTxBuffer.commit();
    if (!success) {
        print("KonfytJackEngine::sendMidiEventsOnRoute event TX buffer full.");
    }
    return success;
}

/* This indicates whether we are connected to JACK or failed to create/activate
 * a client. */
bool KonfytJackEngine::clientIsActive()
{
    return mClientActive;
}

QString KonfytJackEngine::clientName()
{
    return mJackClientName;
}

QString KonfytJackEngine::clientBaseName()
{
    return mJackClientBaseName;
}

/* Pauses (pause=true) or unpauses (pause=false) the Jack process callback function.
 * When pause=true, the Jack process callback is disabled and this function blocks
 * until the current execution of the process callback (if any) competes, before
 * returning, thus ensuring that the process callback will not execute until this
 * function is called again with pause=false. */
void KonfytJackEngine::pauseJackProcessing(bool pause)
{
    if (pause) {
        if (jackProcessLocks == 0) {
            jackProcessMutex.lock(); // Blocks
        }
        jackProcessLocks++;
    } else {
        jackProcessLocks--;
        if (jackProcessLocks == 0) {
            jackProcessMutex.unlock();
        } else if (jackProcessLocks < 0) {
            error_abort("jackProcessLocks < 0");
        }
    }
}

void KonfytJackEngine::jackPortConnectCallback(jack_port_id_t /*a*/, jack_port_id_t /*b*/, int /*connect*/, void* arg)
{
    KonfytJackEngine* e = (KonfytJackEngine*)arg;
    e->jackPortConnectCallback();
}

void KonfytJackEngine::jackPortRegistrationCallback(jack_port_id_t /*port*/, int /*registered*/, void *arg)
{
    KonfytJackEngine* e = (KonfytJackEngine*)arg;
    e->jackPortRegistrationCallback();
}

/* Static callback function given to JACK, which calls the specific class instance
 * using the supplied user argument. */
int KonfytJackEngine::jackProcessCallback(jack_nframes_t nframes, void *arg)
{
    KonfytJackEngine* e = (KonfytJackEngine*)arg;
    return e->jackProcessCallback(nframes);
}

int KonfytJackEngine::jackXrunCallback(void *arg)
{
    KonfytJackEngine* e = (KonfytJackEngine*)arg;
    e->xrunOccurred();
    return 0;
}

/* Non-static class instance-specific JACK process callback. */
int KonfytJackEngine::jackProcessCallback(jack_nframes_t nframes)
{
    // Lock jack_process ----------------------
    if (!jackProcessMutex.tryLock()) { return 0; }
    // ----------------------------------------

    // panicCmd is the panic command received from the outside.
    // panicState is our internal panic state where zero=no panic.
    if (panicCmd) {
        if (panicState == 0) {
            // If the panic command is true and we are not in a panic state yet, go into one.
            panicState = 1;
        }
    } else {
        // If the panic command is false, ensure we are not in a panic state.
        panicState = 0;
    }

    // ------------------------------------- Start of bus processing

    // An output port is a bus destination with its gain.
    // Other audio input ports are routed to the bus output.

    // Get all audio out ports (bus) buffers
    for (int prt = 0; prt < audioOutPorts.count(); prt++) {
        KfJackAudioPort* port = audioOutPorts.at(prt);
        port->buffer = jack_port_get_buffer( port->jackPointer, nframes); // Bus output buffer
        // Reset buffer
        memset(port->buffer, 0, sizeof(jack_default_audio_sample_t)*nframes);
    }

    // Get all audio in ports buffers
    for (int prt = 0; prt < audioInPorts.count(); prt++) {
        KfJackAudioPort* port = audioInPorts.at(prt);
        port->buffer = jack_port_get_buffer( port->jackPointer, nframes );
    }

    // Get all Fluidsynth audio in port buffers
    if (fluidsynthEngine != nullptr) {
        for (int prt = 0; prt < fluidsynthPorts.count(); prt++) {
            KfJackPluginPorts* fluidsynthPort = fluidsynthPorts.at(prt);
            KfJackAudioPort* port1 = fluidsynthPort->audioInLeft; // Left
            KfJackAudioPort* port2 = fluidsynthPort->audioInRight; // Right

            // We are not getting our audio from Jack audio ports, but from Fluidsynth.
            // The buffers have already been allocated when we added the soundfont layer to the engine.
            // Get data from Fluidsynth
            fluidsynthEngine->fluidsynthWriteFloat(
                        fluidsynthPort->fluidSynthInEngine,
                        ((jack_default_audio_sample_t*)port1->buffer),
                        ((jack_default_audio_sample_t*)port2->buffer),
                        nframes );
        }
    }

    // Get all plugin audio in port buffers
    for (int prt = 0; prt < pluginPorts.count(); prt++) {
        KfJackPluginPorts* pluginPort = pluginPorts.at(prt);
        // Left
        KfJackAudioPort* port1 = pluginPort->audioInLeft;
        port1->buffer = jack_port_get_buffer( port1->jackPointer, nframes );
        // Right
        KfJackAudioPort* port2 = pluginPort->audioInRight;
        port2->buffer = jack_port_get_buffer( port2->jackPointer, nframes );
    }

    if (panicState > 0) {
        // We are in a panic state. Write zero to all buses.
        // (Buses already zeroed above)
    } else {

        // For each audio route, if active, mix source buffer to destination buffer
        for (int r = 0; r < audioRoutes.count(); r++) {
            KfJackAudioRoute* route = audioRoutes[r];
            bool outputFlag = false;
            if (route->active) {
                route->fadingOut = false;
                outputFlag = true;
            } else {
                if (route->fadeoutCounter < (fadeOutValuesCount-1)) {
                    route->fadingOut = true;
                    outputFlag = true;
                }
            }
            if (outputFlag) {
                mixBufferToDestinationPort(route, nframes, true);
            }
        }

        // Finally, apply the gain of each active bus
        for (int prt = 0; prt < audioOutPorts.count(); prt++) {
            KfJackAudioPort* port = audioOutPorts.at(prt);
            // Do for each frame
            for (jack_nframes_t i = 0;  i < nframes; i++) {
                ( (jack_default_audio_sample_t*)( port->buffer ) )[i] *= port->gain;
            }
        }

    } // end of panicState else

    // ------------------------------------- End of bus processing

    // Get buffers for midi output ports to external apps
    for (int p = 0; p < midiOutPorts.count(); p++) {
        KfJackMidiPort* port = midiOutPorts.at(p);
        port->buffer = jack_port_get_buffer( port->jackPointer, nframes);
        jack_midi_clear_buffer( port->buffer );
    }
    // Get buffers for midi output ports to plugins
    for (int p = 0; p < pluginPorts.count(); p++) {
        KfJackMidiPort* port = pluginPorts[p]->midi;
        port->buffer = jack_port_get_buffer( port->jackPointer, nframes );
        jack_midi_clear_buffer( port->buffer );
    }


    if (panicState == 1) {
        // We just entered panic state. Send note off messages etc,
        // and proceed to state 2 where we just wait.

        // Send to fluidsynth
        for (int p = 0; p < fluidsynthPorts.count(); p++) {
            KfFluidSynth* synth = fluidsynthPorts.at(p)->fluidSynthInEngine;
            // Fluidsynthengine will force event channel to zero
            fluidsynthEngine->processJackMidi( synth, &(evAllNotesOff) );
            fluidsynthEngine->processJackMidi( synth, &(evSustainZero) );
            fluidsynthEngine->processJackMidi( synth, &(evPitchbendZero) );
        }

        // Give to all output ports to external apps
        for (int p = 0; p < midiOutPorts.count(); p++) {
            KfJackMidiPort* port = midiOutPorts.at(p);
            sendMidiClosureEvents_allChannels( port ); // Send to all MIDI channels
        }

        // Also give to all plugin ports
        for (int p = 0; p < pluginPorts.count(); p++) {
            KfJackMidiPort* port = pluginPorts[p]->midi;
            sendMidiClosureEvents_chanZeroOnly( port ); // Only on channel zero
        }

        panicState = 2; // Proceed to panicState 2 where we will just wait for panic to subside.

    }


    // For each midi input port...
    for (int p = 0; p < midiInPorts.count(); p++) {

        KfJackMidiPort* sourcePort = midiInPorts[p];
        sourcePort->buffer = jack_port_get_buffer(sourcePort->jackPointer, nframes);
        jack_nframes_t nevents = jack_midi_get_event_count(sourcePort->buffer);

        // For each midi input event...
        for (jack_nframes_t i = 0; i < nevents; i++) {

            // Get input event
            jack_midi_event_t inEvent_jack;
            jack_midi_event_get(&inEvent_jack, sourcePort->buffer, i);
            KonfytMidiEvent ev( inEvent_jack.buffer, inEvent_jack.size );

            // Apply input MIDI port filter
            if (sourcePort->filter.passFilter(&ev)) {
                ev = sourcePort->filter.modify(&ev);
            } else {
                // Event doesn't pass filter. Skip.
                continue;
            }

            // Handle bank select: modify event and store bank select
            handleBankSelect(sourcePort->bankMSB, sourcePort->bankLSB, &ev);

            // Send to GUI
            midiRxBuffer.stash({.sourcePort = sourcePort,
                                  .midiRoute = nullptr,
                                  .midiEvent = ev});

            if (panicState != 0) { continue; }

            // For each MIDI route...
            for (int iRoute = 0; iRoute < midiRoutes.count(); iRoute++) {

                KfJackMidiRoute* route = midiRoutes[iRoute];

                if (route->source != sourcePort) { continue; }
                if (route->destIsJackPort && route->destPort == nullptr) { continue; }
                if (!route->filter.passFilter(&ev)) { continue; }

                KonfytMidiEvent evToSend = route->filter.modify(&ev);

                // Handle bank select: modify event and store bank select
                handleBankSelect(route->bankMSB, route->bankLSB, &evToSend);

                bool passEvent = route->active;
                bool guiOnly = false;
                bool recordNoteon = false;
                bool recordSustain = false;
                bool recordPitchbend = false;

                if (evToSend.type() == MIDI_EVENT_TYPE_NOTEOFF) {
                    passEvent = true; // Pass even if route inactive
                    guiOnly = true;
                    handleNoteoffEvent(evToSend, route, inEvent_jack.time);
                } else if ( (evToSend.type() == MIDI_EVENT_TYPE_CC) && (evToSend.data1() == 64) ) {
                    if (evToSend.data2() <= KONFYT_JACK_SUSTAIN_THRESH) {
                        // Sustain zero
                        if ((route->sustain >> evToSend.channel) & 0x1) {
                            passEvent = true; // Pass even if route inactive
                            route->sustain ^= (1 << evToSend.channel);
                        }
                    } else {
                        recordSustain = true;
                    }
                } else if ( (evToSend.type() == MIDI_EVENT_TYPE_PITCHBEND) ) {
                    if (evToSend.pitchbendValueSigned() == 0) {
                        // Pitchbend zero
                        if ((route->pitchbend >> evToSend.channel) & 0x1) {
                            passEvent = true; // Pass even if route inactive
                            route->pitchbend ^= (1 << evToSend.channel);
                        }
                    } else {
                        recordPitchbend = true;
                    }
                } else if ( evToSend.type() == MIDI_EVENT_TYPE_NOTEON ) {
                    int note = evToSend.note() + globalTranspose;
                    evToSend.setNote(note);
                    if ( (note < 0) || (note > 127) ) {
                        passEvent = false;
                    }
                    recordNoteon = true;
                }

                if (!passEvent) { continue; }

                // Give to GUI
                midiRxBuffer.stash({.sourcePort = nullptr,
                                      .midiRoute = route,
                                      .midiEvent = evToSend});

                if (guiOnly) { continue; }

                // Write MIDI output
                writeRouteMidi(route, evToSend, inEvent_jack.time);

                // Record noteon, sustain or pitchbend for off events later.
                if (recordNoteon) {
                    KonfytJackNoteOnRecord rec;
                    rec.globalTranspose = globalTranspose;
                    rec.note = evToSend.note();
                    rec.channel = evToSend.channel;
                    route->noteOnList.add(rec);
                } else if (recordPitchbend) {
                    route->pitchbend |= 1 << evToSend.channel;
                } else if (recordSustain) {
                    route->sustain |= 1 << evToSend.channel;
                }

            } // end of for midi route

        } // end for each midi input event

    } // end for each midi input port


    // Send route MIDI tx events
    // For each MIDI route...
    for (int r = 0; r < midiRoutes.count(); r++) {

        KfJackMidiRoute* route = midiRoutes[r];
        if (!route->active) { continue; }
        if (route->destIsJackPort && route->destPort == NULL) { continue; }

        route->eventsTxBuffer.startRead();
        while (route->eventsTxBuffer.hasNext()) {
            KonfytMidiEvent event = route->eventsTxBuffer.readNext();

            // Apply only the route MIDI filter output channel (if any)
            if (route->filter.outChan >= 0) {
                event.channel = route->filter.outChan;
            }

            if (route->destIsJackPort) {
                // Destination is JACK port

                unsigned char* outBuffer = 0;

                // If bank MSB/LSB not -1, send them before the event
                if (event.bankMSB >= 0) {
                    outBuffer = jack_midi_event_reserve(
                                route->destPort->buffer, 0, 3);
                    if (outBuffer) { event.msbToBuffer(outBuffer); }
                }
                if (event.bankLSB >= 0) {
                    outBuffer = jack_midi_event_reserve(
                                route->destPort->buffer, 0, 3);
                    if (outBuffer) { event.lsbToBuffer(outBuffer); }
                }
                // Send event
                outBuffer = jack_midi_event_reserve( route->destPort->buffer,
                                                     0, event.bufferSizeRequired());
                if (outBuffer) { event.toBuffer(outBuffer); }

            } else {
                // Destination is Fluidsynth port
                fluidsynthEngine->processJackMidi(route->destFluidsynthID,
                                                  &event);
            }

        }
        route->eventsTxBuffer.endRead();
    }


    // Commit received events to buffer so they can be read in the GUI thread.
    audioRxBuffer.commit();
    midiRxBuffer.commit();


    // Unlock jack_process --------------------
    jackProcessMutex.unlock();
    // ----------------------------------------

    return 0;

    // end of jackProcessCallback
}

void KonfytJackEngine::jackPortConnectCallback()
{
    connectCallback = true;
}

void KonfytJackEngine::jackPortRegistrationCallback()
{
    registerCallback = true;
}

void KonfytJackEngine::setFluidsynthEngine(KonfytFluidsynthEngine *e)
{
    fluidsynthEngine = e;
}

/* Return list of MIDI events that were buffered during JACK process callback(s). */
QList<KfJackMidiRxEvent> KonfytJackEngine::getMidiRxEvents()
{
    QList<KfJackMidiRxEvent> ret = extractedMidiRx;
    extractedMidiRx.clear();
    return ret;
}

QList<KfJackAudioRxEvent> KonfytJackEngine::getAudioRxEvents()
{
    QList<KfJackAudioRxEvent> ret = extractedAudioRx;
    extractedAudioRx.clear();
    return ret;
}

/* Helper function for Jack process callback.
 * Send noteoffs to all corresponding recorded noteon events. */
void KonfytJackEngine::handleNoteoffEvent(const KonfytMidiEvent &ev,
                                          KfJackMidiRoute *route,
                                          jack_nframes_t time)
{
    for (int i=0; i < route->noteOnList.count(); i++) {
        KonfytJackNoteOnRecord* rec = route->noteOnList.at_ptr(i);
        if ( (rec->note == ev.note() + rec->globalTranspose)
             && (rec->channel == ev.channel) ) {
            // Match! Send noteoff
            KonfytMidiEvent toSend = ev;
            toSend.setNote(rec->note);
            writeRouteMidi(route, toSend, time);
            // Remove noteon from list
            route->noteOnList.remove(i);
            i--; // Due to removal, have to stay at same index after for loop i++
        }
    }
}

/* Helper function for JACK process callback */
void KonfytJackEngine::mixBufferToDestinationPort(KfJackAudioRoute *route,
                                                  jack_nframes_t nframes,
                                                  bool applyGain)
{
    if (route->source == NULL) { return; }
    if (route->dest == NULL) { return; }

    float gain = 1;
    if (applyGain) {
        gain = route->gain;
    }

    // For each frame: destination_buffer[frame] += source_buffer[frame]
    for (jack_nframes_t i = 0;  i < nframes; i++) {
        float frame = ((jack_default_audio_sample_t*)(route->source->buffer))[i]
                * gain * fadeOutValues[route->fadeoutCounter];
        route->rxBufferSum += qAbs(frame);
        ( (jack_default_audio_sample_t*)(route->dest->buffer) )[i] += frame;

        if (route->fadingOut) {
            if (route->fadeoutCounter < (fadeOutValuesCount-1) ) {
                route->fadeoutCounter++;
            }
        } else {
            if (route->fadeoutCounter > 0) {
                route->fadeoutCounter--;
            }
        }
    }
    route->rxCycleCount++;
    if (route->rxCycleCount >= audioBufferCycleCount) {
        KfJackAudioRxEvent ev;
        ev.audioRoute = route;
        ev.data = route->rxBufferSum / route->rxCycleCount / nframes;
        audioRxBuffer.stash(ev);
        route->rxBufferSum = 0;
        route->rxCycleCount = 0;
    }
}

/* Initialises MIDI closure events that will be sent to ports during JACK
 * process callback. */
void KonfytJackEngine::initMidiClosureEvents()
{
    evAllNotesOff.setCC(MIDI_MSG_ALL_NOTES_OFF, 0);
    evSustainZero.setCC(64, 0);
    evPitchbendZero.setPitchbend(0);
}

/* Helper function for JACK process callback. */
void KonfytJackEngine::sendMidiClosureEvents(KfJackMidiPort *port, int channel)
{
    unsigned char* out_buffer;

    // All notes off
    out_buffer = jack_midi_event_reserve( port->buffer, 0, 3);
    if (out_buffer) {
        evAllNotesOff.channel = channel;
        evAllNotesOff.toBuffer(out_buffer);
    }
    // Also send sustain off message
    out_buffer = jack_midi_event_reserve( port->buffer, 0, 3 );
    if (out_buffer) {
        evSustainZero.channel = channel;
        evSustainZero.toBuffer(out_buffer);
    }
    // And also pitchbend zero
    out_buffer = jack_midi_event_reserve( port->buffer, 0, 3 );
    if (out_buffer) {
        evPitchbendZero.channel = channel;
        evPitchbendZero.toBuffer(out_buffer);
    }
}

/* Helper function for JACK process callback. */
void KonfytJackEngine::sendMidiClosureEvents_chanZeroOnly(KfJackMidiPort *port)
{
    sendMidiClosureEvents(port, 0);
}

/* Helper function for JACK process callback. */
void KonfytJackEngine::sendMidiClosureEvents_allChannels(KfJackMidiPort *port)
{
    for (int i=0; i<15; i++) {
        sendMidiClosureEvents(port, i);
    }
}

/* Modify MIDI event with stored bank select (if any), or store bank select
 * (if applicable).
 * If the MIDI event is a PROGRAM, previously stored bank MSB and LSB with the
 * same channel (if any) are added to the event. Otherwise the MIDI event bank
 * MSB and LSB are cleared.
 * If the MIDI event is a bank MSB or LSB, it is stored. Otherwise, stored bank
 * selects are cleared. */
void KonfytJackEngine::handleBankSelect(int bankMSB[], int bankLSB[], KonfytMidiEvent *ev)
{
    // Modify MIDI event with stored bank select, or clear.
    if (ev->type() == MIDI_EVENT_TYPE_PROGRAM) {
        if ( (bankMSB[ev->channel] >= 0) &&
             (bankLSB[ev->channel] >= 0) ) {
            // MIDI event is a program and bank MSB and LSB with the same channel
            // have been stored previously. Modify the MIDI event.
            ev->bankMSB = bankMSB[ev->channel];
            ev->bankLSB = bankLSB[ev->channel];
        } else {
            ev->bankMSB = -1;
            ev->bankLSB = -1;
        }
    }

    // Save bank
    if (ev->type() == MIDI_EVENT_TYPE_CC) {
        if (ev->data1() == 0) {
            // Bank select MSB
            bankMSB[ev->channel] = ev->data2();
        } else if (ev->data1() == 32) {
            // Bank select LSB
            bankLSB[ev->channel] = ev->data2();
        } else {
            // Cancel bank select
            bankMSB[ev->channel] = -1;
            bankLSB[ev->channel] = -1;
        }
    } else {
        // Cancel bank select
        bankMSB[ev->channel] = -1;
        bankLSB[ev->channel] = -1;
    }
}

bool KonfytJackEngine::initJackClient(QString name)
{
    mJackClientBaseName = name;
    // Try to become a client of the JACK server
    if ( (mJackClient = jack_client_open(name.toLocal8Bit(), JackNullOption, NULL)) == NULL) {
        print("JACK: Error becoming client.");
        this->mClientActive = false;
        return false;
    } else {
        // jack_client_open modifies the given name if another client already
        // uses it. Get our actual client name.
        mJackClientName = jack_get_client_name(mJackClient);
        print("JACK: Client created: " + mJackClientName);
        this->mClientActive = true;
    }


    // Set up callback functions
    jack_set_port_connect_callback(mJackClient,
                KonfytJackEngine::jackPortConnectCallback, this);
    jack_set_port_registration_callback(mJackClient,
                KonfytJackEngine::jackPortRegistrationCallback, this);
    jack_set_process_callback (mJackClient,
                KonfytJackEngine::jackProcessCallback, this);
    jack_set_xrun_callback(mJackClient,
                KonfytJackEngine::jackXrunCallback, this);

    nframes = jack_get_buffer_size(mJackClient);

    // Activate the client
    if (jack_activate(mJackClient)) {
        print("JACK: Cannot activate client.");
        jack_free(mJackClient);
        this->mClientActive = false;
        return false;
    } else {
        print("JACK: Activated client.");
        this->mClientActive = true;
    }

    // Get sample rate
    this->samplerate = jack_get_sample_rate(mJackClient);
    print("JACK: Samplerate " + n2s(samplerate));

    audioBufferCycleCount = samplerate/nframes/10;

    fadeOutValuesCount = samplerate*fadeOutSecs;
    // Linear fadeout
    fadeOutValues = (float*)malloc(sizeof(float)*fadeOutValuesCount);
    for (unsigned int i=0; i<fadeOutValuesCount; i++) {
        fadeOutValues[i] = 1 - ((float)i/(float)fadeOutValuesCount);
    }

    // Timer that will take care of communicating JACK process data to rest of
    // app, as well as restoring JACK port connections.
    startTimer();

    return true;
}

void KonfytJackEngine::stopJackClient()
{
    if (clientIsActive()) {
        pauseJackProcessing(true);
        jack_client_close(mJackClient);
        mJackClient = nullptr;
        mClientActive = false;
        pauseJackProcessing(false);
    }
}

/* Get a string list of JACK ports, based on type pattern (e.g. "midi" or "audio", etc.)
 * and flags (e.g. JackPortIsInput or JackPortIsOutput). */
QStringList KonfytJackEngine::getJackPorts(QString typePattern, unsigned long flags)
{
    QStringList pl;
    char** ports;

    ports = (char**)jack_get_ports(mJackClient,NULL,typePattern.toLocal8Bit(), flags);

    int i=0;
    if (ports != NULL) {
        while (ports[i] != NULL) {

            QString portname = QString::fromLocal8Bit(ports[i]);

            pl.append(portname);

            i++;
        }

        jack_free(ports);
    }

    return pl;
}

void KonfytJackEngine::writeRouteMidi(KfJackMidiRoute *route,
                                      KonfytMidiEvent &ev, jack_nframes_t time)
{
    if (route->destIsJackPort) {
        // Destination is JACK port
        unsigned char* outBuffer = jack_midi_event_reserve(
                    route->destPort->buffer, time, ev.bufferSizeRequired());

        if (outBuffer == 0) { return; } // JACK error

        // Copy event to output buffer
        ev.toBuffer(outBuffer);
    } else {
        // Destination is Fluidsynth port
        fluidsynthEngine->processJackMidi(route->destFluidsynthID, &ev);
    }
}

/* Returns list of JACK midi input ports from the JACK server. */
QStringList KonfytJackEngine::getMidiInputPortsList()
{
    return getJackPorts("midi", JackPortIsInput);
}

/* Returns list of JACK midi output ports from the JACK server. */
QStringList KonfytJackEngine::getMidiOutputPortsList()
{
    return getJackPorts("midi", JackPortIsOutput);
}

/* Returns list of JACK audio input ports from the JACK server. */
QStringList KonfytJackEngine::getAudioInputPortsList()
{
    return getJackPorts("audio", JackPortIsInput);
}

/* Returns list of JACK audio output ports from the JACK server. */
QStringList KonfytJackEngine::getAudioOutputPortsList()
{
    return getJackPorts("audio", JackPortIsOutput);
}

QSet<QString> KonfytJackEngine::getJackClientsList()
{
    QSet<QString> clients;

    QStringList ports;
    ports.append(getMidiInputPortsList());
    ports.append(getMidiOutputPortsList());
    ports.append(getAudioInputPortsList());
    ports.append(getAudioOutputPortsList());

    foreach (const QString& port, ports) {
        clients.insert(port.split(":").value(0));
    }

    return clients;
}

KfJackMidiPort *KonfytJackEngine::addMidiPort(QString name, bool isInput)
{
    if (!clientIsActive()) { return nullptr; }
    pauseJackProcessing(true);

    KfJackMidiPort* port = new KfJackMidiPort();
    port->jackPointer = registerJackMidiPort(name, isInput);
    if (isInput) {
        midiInPorts.append(port);
    } else {
        midiOutPorts.append(port);
    }

    pauseJackProcessing(false);
    return port;
}

KfJackAudioPort *KonfytJackEngine::addAudioPort(QString name, bool isInput)
{
    if (!clientIsActive()) { return nullptr; }
    pauseJackProcessing(true);

    KfJackAudioPort* port = new KfJackAudioPort();
    port->jackPointer = registerJackAudioPort(name, isInput);
    if (isInput) {
        audioInPorts.append(port);
    } else {
        audioOutPorts.append(port);
    }

    pauseJackProcessing(false);
    return port;
}

void KonfytJackEngine::removeMidiPort(KfJackMidiPort *port)
{
    if (!port) {
        error_abort("removeMidiPort: port null.");
    }

    pauseJackProcessing(true);

    midiInPorts.removeAll(port);
    midiOutPorts.removeAll(port);
    jack_port_unregister(mJackClient, port->jackPointer);
    delete port;

    // Remove this port from any routes.
    removePortFromAllRoutes(port);

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAudioPort(KfJackAudioPort *port)
{
    if (!port) {
        error_abort("removeAudioPort: Audio port null.");
    }

    pauseJackProcessing(true);

    audioInPorts.removeAll(port);
    audioOutPorts.removeAll(port);
    jack_port_unregister(mJackClient, port->jackPointer);
    delete port;

    // Remove this port from any routes.
    removePortFromAllRoutes(port);

    pauseJackProcessing(false);
}

uint32_t KonfytJackEngine::getSampleRate()
{
    return this->samplerate;
}

/* Returns the JACK nframes size (passed to process callback). */
uint32_t KonfytJackEngine::getBufferSize()
{
    return this->nframes;
}

void KonfytJackEngine::addOtherJackConPair(KonfytJackConPair p)
{
    // Ignore if already in the list
    for (int i=0; i<otherConsList.count(); i++) {
        if (p.equals(otherConsList[i])) {
            return;
        }
    }

    otherConsList.append(p);

    refreshAllPortsConnections();
}

void KonfytJackEngine::removeOtherJackConPair(KonfytJackConPair p)
{
    pauseJackProcessing(true);

    for (int i=0; i<otherConsList.count(); i++) {
        if (p.equals(otherConsList[i])) {
            otherConsList.removeAt(i);

            // Disconnect JACK ports
            if (clientIsActive()) {
                jack_disconnect(mJackClient, p.srcPort.toLocal8Bit(), p.destPort.toLocal8Bit());
            }

            break;
        }
    }

    pauseJackProcessing(false);
    refreshAllPortsConnections();
}

void KonfytJackEngine::clearOtherJackConPair()
{
    pauseJackProcessing(true);

    otherConsList.clear();

    pauseJackProcessing(false);
}

void KonfytJackEngine::setGlobalTranspose(int transpose)
{
    this->globalTranspose = transpose;
}

jack_port_t *KonfytJackEngine::registerJackMidiPort(QString name, bool input)
{
    jack_port_t* port = jack_port_register( mJackClient,
                                            name.toLocal8Bit().constData(),
                                            JACK_DEFAULT_MIDI_TYPE,
                                    input ? JackPortIsInput : JackPortIsOutput,
                                            0);
    if (!port) {
        print(QString("Failed to register JACK MIDI %1 port: %2")
                .arg(input ? "input" : "output")
                .arg(name));
    }
    return port;
}

jack_port_t *KonfytJackEngine::registerJackAudioPort(QString name, bool input)
{
    jack_port_t* port = jack_port_register( mJackClient,
                                            name.toLocal8Bit().constData(),
                                            JACK_DEFAULT_AUDIO_TYPE,
                                    input ? JackPortIsInput : JackPortIsOutput,
                                            0);
    if (!port) {
        print(QString("Failed to register JACK MIDI %1 port: %2")
                .arg(input ? "input" : "output")
                .arg(name));
    }
    return port;
}


/* Print error message to stdout, and abort app. */
void KonfytJackEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: konfytJackClient:" << msg.toLocal8Bit().constData();
    abort();
}

