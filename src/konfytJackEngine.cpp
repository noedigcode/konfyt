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

// Set panicCmd. The Jack process callback will behave accordingly.
void KonfytJackEngine::panic(bool p)
{
    panicCmd = p;
}


void KonfytJackEngine::timerEvent(QTimerEvent* /*event*/)
{
    timer_busy = true;
    if (timer_disabled) {
        timer_busy = false;
        return;
    }

    if (connectCallback || registerCallback) {
        connectCallback = false;
        registerCallback = false;
        refreshPortConnections();
        jackPortRegisterOrConnectCallback();
    }

    timer_busy = false;
}

void KonfytJackEngine::startPortTimer()
{
    this->timer.start(100, this);
}

KonfytJackPluginPorts *KonfytJackEngine::pluginPortsFromId(int portId)
{
    KonfytJackPluginPorts* p = pluginsPortsMap.value(portId, NULL);
    if (p == NULL) {
        error_abort("Invalid pluginports " + n2s(portId));
    }
    return p;
}

KonfytJackPluginPorts* KonfytJackEngine::fluidsynthPortFromId(int portId)
{
    KonfytJackPluginPorts* p = soundfontPortsMap.value(portId, NULL);
    if (p == NULL) {
        error_abort("Invalid Fluidsynth port " + n2s(portId));
    }
    return p;
}

void KonfytJackEngine::refreshPortConnections()
{
    if (!clientIsActive()) { return; }

    // Refresh connections to each midi input port
    for (int i=0; i < midi_in_ports.count(); i++) {

        KonfytJackPort* port = midi_in_ports.at(i);
        const char* portname = jack_port_name( port->jack_pointer );

        for (int j=0; j < port->connectionList.count(); j++) {
            QString clientname = port->connectionList.at(j);

            if (!jack_port_connected_to( port->jack_pointer, clientname.toLocal8Bit() )) {
                jack_connect(client, clientname.toLocal8Bit(), portname); // NB order of ports important; First source, then dest.
            }
        }
    }

    // For each midi output port to external apps, refresh connections to all their clients
    for (int i=0; i<midi_out_ports.count(); i++) {

        KonfytJackPort* port = midi_out_ports.at(i);
        const char* portname = jack_port_name( port->jack_pointer );

        for (int j=0; j<port->connectionList.count(); j++) {
            QString clientname = port->connectionList.at(j);

            if (!jack_port_connected_to( port->jack_pointer, clientname.toLocal8Bit())) {
                jack_connect(client, portname, clientname.toLocal8Bit()); // NB order of ports important; First source, then dest.
            }
        }
    }

    // Refresh connections for plugin midi and audio ports
    for (int i=0; i<plugin_ports.count(); i++) {

        // Midi out
        KonfytJackPort* m = plugin_ports.at(i)->midi;
        const char* m_src = jack_port_name( m->jack_pointer );
        for (int j=0; j<m->connectionList.count(); j++) {

            const char* m_dest = m->connectionList.at(j).toLocal8Bit();
            jack_connect(client, m_src, m_dest);
        }

        // Audio in left
        KonfytJackPort* al = plugin_ports.at(i)->audio_in_l;
        const char* al_dest = jack_port_name( al->jack_pointer );
        for (int j=0; j<al->connectionList.count(); j++) {

            const char* al_src = al->connectionList.at(j).toLocal8Bit();
            jack_connect(client, al_src, al_dest);
        }

        // Audio in right
        KonfytJackPort* ar = plugin_ports.at(i)->audio_in_r;
        const char* ar_dest = jack_port_name( ar->jack_pointer );
        for (int j=0; j<ar->connectionList.count(); j++) {

            const char* ar_src = ar->connectionList.at(j).toLocal8Bit();
            jack_connect(client, ar_src, ar_dest);
        }

    }

    // Refresh connections for audio input ports
    for (int i=0; i<audio_in_ports.count(); i++) {

        KonfytJackPort* p = audio_in_ports[i];
        const char* dest = jack_port_name(p->jack_pointer);
        for (int j=0; j<p->connectionList.count(); j++) {
            const char* src = p->connectionList[j].toLocal8Bit();
            jack_connect(client, src, dest);
        }
    }

    // Refresh connections for audio output ports (aka buses)
    for (int i=0; i<audio_out_ports.count(); i++) {

        KonfytJackPort* p = audio_out_ports[i];
        const char* src = jack_port_name(p->jack_pointer);
        for (int j=0; j<p->connectionList.count(); j++) {
            const char* dest = p->connectionList[j].toLocal8Bit();
            jack_connect(client, src, dest);
        }

    }

    // Refresh other JACK connections
    for (int i=0; i<otherConsList.count(); i++) {
        KonfytJackConPair p = otherConsList[i];
        jack_connect(client, p.srcPort.toLocal8Bit(), p.destPort.toLocal8Bit());
    }

}

/* Add new soundfont ports. Also assigns MIDI filter. */
int KonfytJackEngine::addSoundfont(const LayerSoundfontStruct &sf)
{
    /* Soundfonts use the same structures as SFZ plugins for now, but are much simpler
     * as midi is given to the fluidsynth engine and audio is recieved from it without
     * external Jack connections. */

    KonfytJackPluginPorts* p = new KonfytJackPluginPorts();
    p->audio_in_l = new KonfytJackPort();
    p->audio_in_r = new KonfytJackPort();

    // Because our audio is not received from jack audio ports, we have to allocate
    // the buffers now so fluidsynth can write to them in the jack process callback.
    p->audio_in_l->buffer = malloc(sizeof(jack_default_audio_sample_t)*nframes);
    p->audio_in_r->buffer = malloc(sizeof(jack_default_audio_sample_t)*nframes);

    p->midi = new KonfytJackPort(); // Dummy port for note records, etc.
    p->id = idCounter++;
    p->idInPluginEngine = sf.indexInEngine;

    p->midiRouteId = addMidiRoute();
    p->audioLeftRouteId = addAudioRoute();
    p->audioRightRouteId = addAudioRoute();

    // Only MIDI route is used to activate/deactivate the sound. Audio routes are
    // always active.
    setAudioRouteActive(p->audioLeftRouteId, true);
    setAudioRouteActive(p->audioRightRouteId, true);

    // Pre-set audio route sources and MIDI route destination
    KonfytJackAudioRoute* route = audioRouteFromId(p->audioLeftRouteId);
    route->source = p->audio_in_l;

    route = audioRouteFromId(p->audioRightRouteId);
    route->source = p->audio_in_r;

    KonfytJackMidiRoute* midiRoute = midiRouteFromId(p->midiRouteId);
    midiRoute->destFluidsynthID = p->idInPluginEngine;
    midiRoute->destIsJackPort = false;
    midiRoute->destPort = p->midi;
    midiRoute->filter = sf.filter;

    fluidsynth_ports.append(p);
    soundfontPortsMap.insert(p->id, p);

    return p->id;
}

void KonfytJackEngine::removeSoundfont(int id)
{
    pauseJackProcessing(true);

    // Delete all objects created in addSoundfont()

    KonfytJackPluginPorts* p = fluidsynthPortFromId(id);

    // Remove all recorded noteon, sustain and pitchbend events related to this Fluidsynth ID.
    for (int i=0; i<noteOnList.count(); i++) {
        KonfytJackNoteOnRecord *rec = noteOnList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == p->idInPluginEngine) {
                noteOnList.remove(i);
                i--; // Due to removal, have to stay at same index after for loop i++
            }
        }
    }
    for (int i=0; i<sustainList.count(); i++) {
        KonfytJackNoteOnRecord *rec = sustainList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == p->idInPluginEngine) {
                sustainList.remove(i);
                i--;
            }
        }
    }
    for (int i=0; i<pitchBendList.count(); i++) {
        KonfytJackNoteOnRecord *rec = pitchBendList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == p->idInPluginEngine) {
                pitchBendList.remove(i);
                i--;
            }
        }
    }

    soundfontPortsMap.remove(id);
    fluidsynth_ports.removeAll(p);
    free(p->audio_in_l->buffer);
    free(p->audio_in_r->buffer);
    removeMidiRoute(p->midiRouteId);
    removeAudioRoute(p->audioLeftRouteId);
    removeAudioRoute(p->audioRightRouteId);
    delete p->audio_in_l;
    delete p->audio_in_r;
    delete p->midi;
    delete p;

    pauseJackProcessing(false);
}


/* For the specified ports spec, create a new MIDI output port and left and
 * right audio input ports, combined in a plugin ports struct. The strings in the
 * spec are used to add to the created ports auto-connect lists and the MIDI
 * filter is applied to the MIDI port.
 * A unique ID is returned. */
int KonfytJackEngine::addPluginPortsAndConnect(const KonfytJackPortsSpec &spec)
{
    KonfytJackPort* midiPort = new KonfytJackPort();
    KonfytJackPort* alPort = new KonfytJackPort();
    KonfytJackPort* arPort = new KonfytJackPort();

    // Add a new midi output port which will be connected to the plugin midi input
    jack_port_t* newPortPointer = registerJackPort(midiPort, client, spec.name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    if (newPortPointer == NULL) {
        userMessage("Failed to create Jack MIDI output port '" + spec.name + "' for plugin.");
    }
    midiPort->connectionList.append( spec.midiOutConnectTo );

    // Add left audio input port where we will receive plugin audio
    QString nameL = spec.name + "_in_L";
    jack_port_t* newL = registerJackPort(alPort, client, nameL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (newL == NULL) {
        userMessage("Failed to create left audio input port '" + nameL + "' for plugin.");
    }
    alPort->connectionList.append( spec.audioInLeftConnectTo );

    // Add right audio input port
    QString nameR = spec.name + "_in_R";
    jack_port_t* newR = registerJackPort(arPort, client, nameR, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (newR == NULL) {
        userMessage("Failed to create right audio input port '" + nameR + "' for plugin.");
    }
    arPort->connectionList.append( spec.audioInRightConnectTo );

    KonfytJackPluginPorts* p = new KonfytJackPluginPorts();
    p->midi = midiPort;
    p->audio_in_l = alPort;
    p->audio_in_r = arPort;

    // Add routes
    p->midiRouteId = addMidiRoute();
    p->audioLeftRouteId = addAudioRoute();
    p->audioRightRouteId = addAudioRoute();

    // Only MIDI route is used to set plugin active/inactive. Audio ports always
    // active.
    setAudioRouteActive(p->audioLeftRouteId, true);
    setAudioRouteActive(p->audioRightRouteId, true);

    // Pre-set route sources/dests
    KonfytJackMidiRoute* midiRoute = midiRouteFromId(p->midiRouteId);
    midiRoute->destIsJackPort = true;
    midiRoute->destPort = midiPort;
    midiRoute->filter = spec.midiFilter;

    KonfytJackAudioRoute* audioRoute = audioRouteFromId(p->audioLeftRouteId);
    audioRoute->source = alPort;

    audioRoute = audioRouteFromId(p->audioRightRouteId);
    audioRoute->source = arPort;

    // Create new unique id
    p->id = idCounter++;

    pauseJackProcessing(true);
    plugin_ports.append(p);
    pluginsPortsMap.insert(p->id, p);
    pauseJackProcessing(false);

    return p->id;
}

void KonfytJackEngine::removePlugin(int id)
{
    pauseJackProcessing(true);

    // Remove everything created in addPluginPortsAndConnect()

    KonfytJackPluginPorts* p = pluginPortsFromId(id);
    pluginsPortsMap.remove(id);

    // Remove all recorded noteon, sustain and pitchbend events related to this midi out port
    for (int i=0; i<noteOnList.count(); i++) {
        KonfytJackNoteOnRecord *rec = noteOnList.at_ptr(i);
        if (rec->jackPortNotFluidsynth) {
            if (rec->port == p->midi) {
                noteOnList.remove(i);
                i--; // Due to removal, have to stay at same index after for loop i++
            }
        }
    }
    for (int i=0; i<sustainList.count(); i++) {
        KonfytJackNoteOnRecord *rec = sustainList.at_ptr(i);
        if (rec->jackPortNotFluidsynth) {
            if (rec->port == p->midi) {
                sustainList.remove(i);
                i--;
            }
        }
    }
    for (int i=0; i<pitchBendList.count(); i++) {
        KonfytJackNoteOnRecord *rec = pitchBendList.at_ptr(i);
        if (rec->jackPortNotFluidsynth) {
            if (rec->port == p->midi) {
                pitchBendList.remove(i);
                i--;
            }
        }
    }

    // Remove routes
    removeMidiRoute(p->midiRouteId);
    removeAudioRoute(p->audioLeftRouteId);
    removeAudioRoute(p->audioRightRouteId);

    plugin_ports.removeAll(p);

    jack_port_unregister(client, p->midi->jack_pointer);
    delete p->midi;

    jack_port_unregister(client, p->audio_in_l->jack_pointer);
    delete p->audio_in_l;

    jack_port_unregister(client, p->audio_in_r->jack_pointer);
    delete p->audio_in_r;

    delete p;

    pauseJackProcessing(false);
}

void KonfytJackEngine::setSoundfontMidiFilter(int id, KonfytMidiFilter filter)
{
    midiRouteFromId(fluidsynthPortFromId(id)->midiRouteId)->filter = filter;
}

void KonfytJackEngine::setSoundfontActive(int id, bool active)
{
    KonfytJackPluginPorts* p = fluidsynthPortFromId(id);
    setMidiRouteActive(p->midiRouteId, active);
}

void KonfytJackEngine::setPluginMidiFilter(int id, KonfytMidiFilter filter)
{
    midiRouteFromId(pluginPortsFromId(id)->midiRouteId)->filter = filter;
}

void KonfytJackEngine::setPluginActive(int id, bool active)
{
    KonfytJackPluginPorts* p = pluginPortsFromId(id);
    setMidiRouteActive(p->midiRouteId, active);
}

void KonfytJackEngine::setPluginGain(int id, float gain)
{
    KonfytJackPluginPorts* p = pluginPortsFromId(id);
    audioRouteFromId(p->audioLeftRouteId)->gain = gain;
    audioRouteFromId(p->audioRightRouteId)->gain = gain;
}

void KonfytJackEngine::setSoundfontRouting(int id, int midiInPortId, int leftPortId, int rightPortId)
{
    if (!clientIsActive()) { return; }

    KonfytJackPluginPorts* p = fluidsynthPortFromId(id);

    pauseJackProcessing(true);

    // MIDI route destination has already been set in addSoundfont().
    KonfytJackMidiRoute* midiRoute = midiRouteFromId(p->midiRouteId);
    midiRoute->source = midiInPortFromId(midiInPortId);

    // Audio route sources have already been set in addSoundfont().
    // Set left audio route destination
    KonfytJackAudioRoute* route = audioRouteFromId(p->audioLeftRouteId);
    route->dest = audioOutPortFromId(leftPortId);
    // Right audio route destination
    route = audioRouteFromId(p->audioRightRouteId);
    route->dest = audioOutPortFromId(rightPortId);

    pauseJackProcessing(false);
}

void KonfytJackEngine::setPluginRouting(int pluginId, int midiInPortId, int leftPortId, int rightPortId)
{
    if (!clientIsActive()) { return; }

    KonfytJackPluginPorts* p = pluginPortsFromId(pluginId);

    pauseJackProcessing(true);

    KonfytJackMidiRoute* midiRoute = midiRouteFromId(p->midiRouteId);
    midiRoute->source = midiInPortFromId(midiInPortId);

    KonfytJackAudioRoute* audioRoute = audioRouteFromId(p->audioLeftRouteId);
    audioRoute->dest = audioOutPortFromId(leftPortId);

    audioRoute = audioRouteFromId(p->audioRightRouteId);
    audioRoute->dest = audioOutPortFromId(rightPortId);

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAllAudioInAndOutPorts()
{
    // Disable audio port processing in Jack process callback function,
    // and wait for processing to finish before removing ports.
    pauseJackProcessing(true);

    while (audio_in_ports.count()) {
        removePort(audio_in_ports[0]->id);
    }

    while (audio_out_ports.count()) {
        removePort(audio_out_ports[0]->id);
    }

    // Enable audio port processing again.
    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAllMidiInPorts()
{
    pauseJackProcessing(true);

    while (midi_in_ports.count()) {
        removePort(midi_in_ports[0]->id);
    }

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAllMidiOutPorts()
{
    pauseJackProcessing(true);

    while (midi_out_ports.count()) {
        removePort(midi_out_ports[0]->id);
    }

    pauseJackProcessing(false);
}

QList<KonfytJackPort *> *KonfytJackEngine::getListContainingPort(int portId)
{
    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) { return NULL; }

    QList<KonfytJackPort*> *l = NULL;

    if (midi_in_ports.contains(port)) {
        l = &midi_in_ports;
    } else if (midi_out_ports.contains(port)) {
        l = &midi_out_ports;
    } else if (audio_out_ports.contains(port)) {
        l = &audio_out_ports;
    } else if (audio_in_ports.contains(port)) {
        l = &audio_in_ports;
    }

    return l;
}

/* Set the source/destination port to NULL for all routes that use the specified
 * port. */
void KonfytJackEngine::removePortFromAllRoutes(KonfytJackPort *port)
{
    pauseJackProcessing(true);

    for (int i=0; i < audio_routes.count(); i++) {
        if (audio_routes[i]->source == port) { audio_routes[i]->source = NULL; }
        if (audio_routes[i]->dest == port) { audio_routes[i]->dest = NULL; }
    }

    for (int i=0; i < midi_routes.count(); i++) {
        if (midi_routes[i]->source == port) { midi_routes[i]->source = NULL; }
        if (midi_routes[i]->destPort == port) { midi_routes[i]->destPort = NULL; }
    }

    pauseJackProcessing(false);
}

/* Adds a new port to JACK with the specified type and name. Returns a unique
 * port ID. */
int KonfytJackEngine::addPort(KonfytJackPortType type, QString portName)
{
    if (!clientIsActive()) { return KONFYT_JACK_PORT_ERROR; }

    pauseJackProcessing(true);

    jack_port_t* newPort = nullptr;
    QList<KonfytJackPort*> *listToAddTo = nullptr;

    KonfytJackPort* p = new KonfytJackPort();

    switch (type) {
    case KonfytJackPortType_MidiIn:

        newPort = registerJackPort (p, client, portName, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
        listToAddTo = &midi_in_ports;

        break;
    case KonfytJackPortType_MidiOut:

        newPort = registerJackPort (p, client, portName, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
        listToAddTo = &midi_out_ports;

        break;
    case KonfytJackPortType_AudioOut:

        newPort = registerJackPort (p, client, portName, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        listToAddTo = &audio_out_ports;

        break;
    case KonfytJackPortType_AudioIn:

        newPort = registerJackPort (p, client, portName, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        listToAddTo = &audio_in_ports;

        break;

    default:

        error_abort("addPort: Invalid unhandled KonfytJackPortType.");

    }

    if (newPort == nullptr) {
        userMessage("Failed to add JACK port " + portName);
    }

    // Create unique ID for port
    p->id = idCounter++;

    listToAddTo->append(p);
    portIdMap.insert(p->id, p);

    pauseJackProcessing(false);
    return p->id;
}

void KonfytJackEngine::removePort(int portId)
{
    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        // No such port
        error_abort("removePort: Port " + n2s(portId) + " not found in map.");
        return;
    }

    // Disable Jack process callback and block until it is not executing anymore
    pauseJackProcessing(true);

    QList<KonfytJackPort*> *l = getListContainingPort(portId);
    if (l == NULL) {
        error_abort("removePort: Port not found in any of the lists.");
    }


    l->removeAll(port);
    jack_port_unregister(client, port->jack_pointer);
    delete port;
    portIdMap.remove(portId);

    // If this is a midi out port, remove all recorded noteon, sustain and pitchbend
    // events related to this port.
    if (l == &midi_out_ports) {
        for (int i=0; i<noteOnList.count(); i++) {
            KonfytJackNoteOnRecord *rec = noteOnList.at_ptr(i);
            if (rec->jackPortNotFluidsynth) {
                if (rec->port == port) {
                    noteOnList.remove(i);
                    i--; // Due to removal, have to stay at same index after for loop i++
                }
            }
        }
        for (int i=0; i<sustainList.count(); i++) {
            KonfytJackNoteOnRecord *rec = sustainList.at_ptr(i);
            if (rec->jackPortNotFluidsynth) {
                if (rec->port == port) {
                    sustainList.remove(i);
                    i--;
                }
            }
        }
        for (int i=0; i<pitchBendList.count(); i++) {
            KonfytJackNoteOnRecord *rec = pitchBendList.at_ptr(i);
            if (rec->jackPortNotFluidsynth) {
                if (rec->port == port) {
                    pitchBendList.remove(i);
                    i--;
                }
            }
        }
    }

    // Remove this port from any routes.
    removePortFromAllRoutes(port);

    // Enable Jack process callback again
    pauseJackProcessing(false);
}

void KonfytJackEngine::setPortClients(int portId, QStringList newClientList)
{
    for (int i=0; i<newClientList.count(); i++) {
        this->addPortClient( portId, newClientList[i] );
    }
}

void KonfytJackEngine::clearPortClients(int portId)
{
    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
        return;
    }

    port->connectionList.clear();
}

void KonfytJackEngine::clearPortClients_andDisconnect(int portId)
{
    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
        return;
    }

    while (port->connectionList.count()) {
        removePortClient_andDisconnect(portId, port->connectionList[0]);
    }
}

void KonfytJackEngine::addPortClient(int portId, QString newClient)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
    }

    port->connectionList.append(newClient);

    refreshPortConnections();
}



/* Remove the given client:port (cname) from the specified port auto-connect list. */
void KonfytJackEngine::removePortClient_andDisconnect(int portId, QString cname)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
        return;
    }

    QList<KonfytJackPort*> *l = getListContainingPort(portId);
    if (l == NULL) {
        error_abort("Port " + n2s(portId) + " not in any list.");
        return;
    }

    int order = 0;
    if ( (l == &midi_in_ports) || (l == &audio_in_ports) ) {
        order = 0;
    } else if ( (l == &audio_out_ports) || (l == &midi_out_ports) ) {
        order = 1;
    }

    pauseJackProcessing(true);

    if (port->connectionList.contains( cname )) {
        // Disconnect in Jack
        const char* ourPort = jack_port_name( port->jack_pointer );
        // NB the order of ports in the jack_disconnect function matters:
        if (order) {
            jack_disconnect(client, ourPort, cname.toLocal8Bit() );
        } else {
            jack_disconnect(client, cname.toLocal8Bit(), ourPort );
        }
        // Remove client from port's list
        port->connectionList.removeAll(cname);
    }

    pauseJackProcessing(false);
    refreshPortConnections();
}

QStringList KonfytJackEngine::getPortClients(int portId)
{
    QStringList clients;

    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
    } else {
        clients = port->connectionList;
    }

    return clients;
}

int KonfytJackEngine::getPortCount(KonfytJackPortType type)
{
    int ret = 0;

    switch (type) {
    case KonfytJackPortType_MidiIn:
        ret = midi_in_ports.count();
        break;
    case KonfytJackPortType_MidiOut:
        ret = midi_out_ports.count();
        break;
    case KonfytJackPortType_AudioIn:
        ret = audio_in_ports.count();
        break;
    case KonfytJackPortType_AudioOut:
        ret = audio_out_ports.count();
        break;
    default:
        error_abort("getPortCount: invalid JackPortType");
    }

    return ret;
}

void KonfytJackEngine::setPortFilter(int portId, KonfytMidiFilter filter)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
        return;
    }

    port->filter = filter;
}



int KonfytJackEngine::addAudioRoute(int sourcePortId, int destPortId)
{
    int routeId = addAudioRoute();
    setAudioRoute(routeId, sourcePortId, destPortId);
    return routeId;
}

int KonfytJackEngine::addAudioRoute()
{
    if (!clientIsActive()) { return KONFYT_JACK_PORT_ERROR; }

    pauseJackProcessing(true);

    KonfytJackAudioRoute* route = new KonfytJackAudioRoute();
    route->id = idCounter++;

    audio_routes.append(route);
    audioRouteIdMap.insert(route->id, route);

    pauseJackProcessing(false);

    return route->id;
}

void KonfytJackEngine::setAudioRoute(int routeId, int sourcePortId, int destPortId)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    KonfytJackAudioRoute* route = audioRouteFromId(routeId);

    route->source = audioInPortFromId(sourcePortId);
    route->dest = audioOutPortFromId(destPortId);

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAudioRoute(int routeId)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    KonfytJackAudioRoute* route = audioRouteFromId(routeId);
    audio_routes.removeAll(route);
    audioRouteIdMap.remove(routeId);

    delete route;

    pauseJackProcessing(false);
}

void KonfytJackEngine::setAudioRouteActive(int routeId, bool active)
{
    if (!clientIsActive()) { return; }

    KonfytJackAudioRoute* route = audioRouteFromId(routeId);
    route->active = active;
}

void KonfytJackEngine::setAudioRouteGain(int routeId, float gain)
{
    if (!clientIsActive()) { return; }

    KonfytJackAudioRoute* route = audioRouteFromId(routeId);
    route->gain = gain;
}

int KonfytJackEngine::addMidiRoute(int sourcePortId, int destPortId)
{
    int routeId = addMidiRoute();
    setMidiRoute(routeId, sourcePortId, destPortId);
    return routeId;
}

int KonfytJackEngine::addMidiRoute()
{
    if (!clientIsActive()) { return KONFYT_JACK_PORT_ERROR; }

    pauseJackProcessing(true);

    KonfytJackMidiRoute* route = new KonfytJackMidiRoute();
    route->id = idCounter++;

    midi_routes.append(route);
    midiRouteIdMap.insert(route->id, route);

    pauseJackProcessing(false);

    return route->id;
}

void KonfytJackEngine::setMidiRoute(int routeId, int sourcePortId, int destPortId)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    KonfytJackMidiRoute* route = midiRouteFromId(routeId);

    route->source = midiInPortFromId(sourcePortId);
    route->destPort = midiOutPortFromId(destPortId);

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeMidiRoute(int routeId)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    KonfytJackMidiRoute* route = midiRouteFromId(routeId);
    midi_routes.removeAll(route);
    midiRouteIdMap.remove(routeId);

    delete route;

    pauseJackProcessing(false);
}

void KonfytJackEngine::setMidiRouteActive(int routeId, bool active)
{
    if (!clientIsActive()) { return; }

    KonfytJackMidiRoute* route = midiRouteFromId(routeId);
    route->active = active;
}

void KonfytJackEngine::setRouteMidiFilter(int routeId, KonfytMidiFilter filter)
{
    midiRouteFromId(routeId)->filter = filter;
}

bool KonfytJackEngine::sendMidiEventsOnRoute(int routeId, QList<KonfytMidiEvent> events)
{
    KonfytJackMidiRoute* route = midiRouteFromId(routeId);

    bool success = true;
    foreach (KonfytMidiEvent event, events) {
        success = route->eventsTxBuffer.stash(event);
        if (!success) { break; }
    }
    route->eventsTxBuffer.commit();
    if (!success) {
        userMessage("KonfytJackEngine::sendMidiEventsOnRoute " + n2s(routeId)
                    + " event TX buffer full.");
    }
    return success;
}

// This indicates whether we are connected to Jack or failed to create/activate a client.
bool KonfytJackEngine::clientIsActive()
{
    return this->clientActive;
}

QString KonfytJackEngine::clientName()
{
    return ourJackClientName;
}

/* Pauses (pause=true) or unpauses (pause=false) the Jack process callback function.
 * When pause=true, the Jack process callback is disabled and this function blocks
 * until the current execution of the process callback (if any) competes, before
 * returning, thus ensuring that the process callback will not execute until this
 * function is called again with pause=false. */
void KonfytJackEngine::pauseJackProcessing(bool pause)
{
    /* When jack_process_disable is non-zero, Jack process is disabled. Here it is
     * incremented or decremented, accounting for the fact that this function might
     * be called multiple times within nested functions. Only once all of the callers
     * have also called this function with pause=false, will it reach zero, enabling
     * the Jack process callback again. */

    if (pause) {
        jack_process_disable++;
        timer_disabled = true;
        timer.stop();
        while (jack_process_busy) {}
        while (timer_busy) {}
    } else {
        if (jack_process_disable) {
            jack_process_disable--;
        }
        if (jack_process_disable == 0) {
            timer_disabled = false;
            startPortTimer();
        }
    }
}

void KonfytJackEngine::jackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void* arg)
{
    KonfytJackEngine* e = (KonfytJackEngine*)arg;
    e->jackPortConnectCallback();
}

void KonfytJackEngine::jackPortRegistrationCallback(jack_port_id_t port, int registered, void *arg)
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

/* Non-static class instance specific JACK process callback. */
int KonfytJackEngine::jackProcessCallback(jack_nframes_t nframes)
{
    // Lock jack_process ----------------------
    jack_process_busy = true;
    if (jack_process_disable) {
        jack_process_busy = false;
        //userMessage("Jack process callback locked out.");
        return 0;
    }
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
    for (int prt = 0; prt < audio_out_ports.count(); prt++) {
        KonfytJackPort* port = audio_out_ports.at(prt);
        port->buffer = jack_port_get_buffer( port->jack_pointer, nframes); // Bus output buffer
        // Reset buffer
        memset(port->buffer, 0, sizeof(jack_default_audio_sample_t)*nframes);
    }

    // Get all audio in ports buffers
    for (int prt = 0; prt < audio_in_ports.count(); prt++) {
        KonfytJackPort* port = audio_in_ports.at(prt);
        port->buffer = jack_port_get_buffer( port->jack_pointer, nframes );
    }

    // Get all Fluidsynth audio in port buffers
    if (fluidsynthEngine != NULL) {
        for (int prt = 0; prt < fluidsynth_ports.count(); prt++) {
            KonfytJackPort* port1 = fluidsynth_ports[prt]->audio_in_l; // Left
            KonfytJackPort* port2 = fluidsynth_ports[prt]->audio_in_r; // Right

            // We are not getting our audio from Jack audio ports, but from Fluidsynth.
            // The buffers have already been allocated when we added the soundfont layer to the engine.
            // Get data from Fluidsynth
            fluidsynthEngine->fluidsynthWriteFloat(
                        fluidsynth_ports[prt]->idInPluginEngine,
                        ((jack_default_audio_sample_t*)port1->buffer),
                        ((jack_default_audio_sample_t*)port2->buffer),
                        nframes );
        }
    }

    // Get all plugin audio in port buffers
    for (int prt = 0; prt < plugin_ports.count(); prt++) {
        // Left
        KonfytJackPort* port1 = plugin_ports[prt]->audio_in_l;
        port1->buffer = jack_port_get_buffer( port1->jack_pointer, nframes );
        // Right
        KonfytJackPort* port2 = plugin_ports[prt]->audio_in_r;
        port2->buffer = jack_port_get_buffer( port2->jack_pointer, nframes );
    }

    if (panicState > 0) {
        // We are in a panic state. Write zero to all buses.
        // (Buses already zeroed above))))
    } else {

        // For each audio route, if active, mix source buffer to destination buffer
        for (int r = 0; r < audio_routes.count(); r++) {
            KonfytJackAudioRoute* route = audio_routes[r];
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
        for (int prt = 0; prt < audio_out_ports.count(); prt++) {
            KonfytJackPort* port = audio_out_ports.at(prt);
            // Do for each frame
            for (jack_nframes_t i = 0;  i < nframes; i++) {
                ( (jack_default_audio_sample_t*)( port->buffer ) )[i] *= port->gain;
            }
        }

    } // end of panicState else

    // ------------------------------------- End of bus processing

    // Get buffers for midi output ports to external apps
    for (int p = 0; p < midi_out_ports.count(); p++) {
        KonfytJackPort* port = midi_out_ports.at(p);
        port->buffer = jack_port_get_buffer( port->jack_pointer, nframes);
        jack_midi_clear_buffer( port->buffer );
    }
    // Get buffers for midi output ports to plugins
    for (int p = 0; p < plugin_ports.count(); p++) {
        KonfytJackPort* port = plugin_ports[p]->midi;
        port->buffer = jack_port_get_buffer( port->jack_pointer, nframes );
        jack_midi_clear_buffer( port->buffer );
    }


    if (panicState == 1) {
        // We just entered panic state. Send note off messages etc, and proceed to state 2 where we just wait.

        // Send to fluidsynth
        for (int p = 0; p < fluidsynth_ports.count(); p++) {
            int id = fluidsynth_ports.at(p)->idInPluginEngine;
            // Fluidsynthengine will force event channel to zero
            fluidsynthEngine->processJackMidi( id, &(evAllNotesOff) );
            fluidsynthEngine->processJackMidi( id, &(evSustainZero) );
            fluidsynthEngine->processJackMidi( id, &(evPitchbendZero) );
        }

        // Give to all output ports to external apps
        for (int p = 0; p < midi_out_ports.count(); p++) {
            KonfytJackPort* port = midi_out_ports.at(p);
            sendMidiClosureEvents_allChannels( port ); // Send to all MIDI channels
        }

        // Also give to all plugin ports
        for (int p = 0; p < plugin_ports.count(); p++) {
            KonfytJackPort* port = plugin_ports[p]->midi;
            sendMidiClosureEvents_chanZeroOnly( port ); // Only on channel zero
        }

        panicState = 2; // Proceed to panicState 2 where we will just wait for panic to subside.

    }


    // For each midi input port...
    for (int p = 0; p < midi_in_ports.count(); p++) {

        KonfytJackPort* sourcePort = midi_in_ports[p];
        sourcePort->buffer = jack_port_get_buffer(sourcePort->jack_pointer, nframes);
        jack_nframes_t nevents = jack_midi_get_event_count(sourcePort->buffer);

        // For each midi input event...
        for (jack_nframes_t i = 0; i < nevents; i++) {

            // Get input event
            jack_midi_event_t inEvent_jack;
            jack_midi_event_get(&inEvent_jack, sourcePort->buffer, i);
            KonfytMidiEvent ev( inEvent_jack.buffer, inEvent_jack.size );
            ev.sourceId = sourcePort->id;

            // Apply input MIDI port filter
            if (sourcePort->filter.passFilter(&ev)) {
                ev = sourcePort->filter.modify(&ev);
            } else {
                // Event doesn't pass filter. Skip.
                continue;
            }

            // Send to GUI
            if (ev.type != MIDI_EVENT_TYPE_SYSTEM) {
                eventsRxBuffer.stash(ev);
            }

            bool passEvent = true;
            bool recordNoteon = false;
            bool recordSustain = false;
            bool recordPitchbend = false;


            if (ev.type == MIDI_EVENT_TYPE_NOTEOFF) {
                passEvent = false;
                handleNoteoffEvent(ev, inEvent_jack, sourcePort);
            } else if ( (ev.type == MIDI_EVENT_TYPE_CC) && (ev.data1 == 64) ) {
                if (ev.data2 <= KONFYT_JACK_SUSTAIN_THRESH) {
                    // Sustain zero
                    passEvent = false;
                    handleSustainoffEvent(ev, inEvent_jack, sourcePort);
                } else {
                    recordSustain = true;
                }
            } else if ( (ev.type == MIDI_EVENT_TYPE_PITCHBEND) ) {
                if (ev.pitchbendValue_signed() == 0) {
                    // Pitchbend zero
                    passEvent = false;
                    handlePitchbendZeroEvent(ev, inEvent_jack, sourcePort);
                } else {
                    recordPitchbend = true;
                }
            } else if ( ev.type == MIDI_EVENT_TYPE_NOTEON ) {
                ev.data1 += globalTranspose;
                if ( (ev.data1 < 0) || (ev.data1 > 127) ) {
                    passEvent = false;
                }
                recordNoteon = true;
            }


            if ( (panicState == 0) && passEvent ) {

                // For each MIDI route...
                for (int r = 0; r < midi_routes.count(); r++) {

                    KonfytJackMidiRoute* route = midi_routes[r];
                    if (!route->active) { continue; }
                    if (route->source != sourcePort) { continue; }
                    if (route->destIsJackPort && route->destPort == NULL) { continue; }
                    if (!route->filter.passFilter(&ev)) { continue; }

                    KonfytMidiEvent evToSend = route->filter.modify(&ev);

                    if (route->destIsJackPort) {
                        // Destination is JACK port
                        unsigned char* outBuffer = jack_midi_event_reserve(
                                    route->destPort->buffer, inEvent_jack.time,
                                    inEvent_jack.size);

                        if (outBuffer == 0) { continue; } // JACK error

                        // Copy event to output buffer
                        evToSend.toBuffer( outBuffer );
                    } else {
                        // Destination is Fluidsynth port
                        fluidsynthEngine->processJackMidi(route->destFluidsynthID,
                                                          &evToSend);
                    }

                    // Record noteon, sustain or pitchbend for off events later.
                    if (recordNoteon) {
                        KonfytJackNoteOnRecord rec;
                        rec.filter = route->filter;
                        rec.globalTranspose = globalTranspose;
                        rec.jackPortNotFluidsynth = route->destIsJackPort;
                        rec.port = route->destPort;
                        rec.fluidsynthID = route->destFluidsynthID;
                        rec.sourcePort = route->source;

                        rec.note = evToSend.data1;

                        noteOnList.add(rec);
                        rec.port->noteOns++;

                    } else if (recordPitchbend) {
                        // If this port hasn't saved a pitchbend yet
                        if ( !route->destPort->pitchbendNonZero ) {
                            KonfytJackNoteOnRecord rec;
                            rec.filter = route->filter;
                            rec.globalTranspose = globalTranspose;
                            rec.jackPortNotFluidsynth = route->destIsJackPort;
                            rec.port = route->destPort;
                            rec.fluidsynthID = route->destFluidsynthID;
                            rec.sourcePort = route->source;

                            pitchBendList.add(rec);
                            rec.port->pitchbendNonZero = true;
                        }
                    } else if (recordSustain) {
                        // If this port hasn't saved a sustain yet
                        if ( !route->destPort->sustainNonZero ) {
                            KonfytJackNoteOnRecord rec;
                            rec.filter = route->filter;
                            rec.globalTranspose = globalTranspose;
                            rec.jackPortNotFluidsynth = route->destIsJackPort;
                            rec.port = route->destPort;
                            rec.fluidsynthID = route->destFluidsynthID;
                            rec.sourcePort = route->source;

                            sustainList.add(rec);
                            rec.port->sustainNonZero = true;
                        }
                    }

                }

            }

        } // end for each midi input event

    } // end for each midi input port


    // Send route MIDI tx events
    // For each MIDI route...
    for (int r = 0; r < midi_routes.count(); r++) {

        KonfytJackMidiRoute* route = midi_routes[r];
        if (!route->active) { continue; }
        if (route->destIsJackPort && route->destPort == NULL) { continue; }

        QList<KonfytMidiEvent> events = route->eventsTxBuffer.readAll();
        foreach (KonfytMidiEvent event, events) {

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

    }


    if (eventsRxBuffer.commit()) {
        // Inform GUI there are events waiting
        emit midiEventSignal();
    }


    // Unlock jack_process --------------------
    jack_process_busy = false;
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

void KonfytJackEngine::setFluidsynthEngine(konfytFluidsynthEngine *e)
{
    fluidsynthEngine = e;
}


int KonfytJackEngine::jackXrunCallback(void *arg)
{   
    KonfytJackEngine* e = (KonfytJackEngine*)arg;
    e->xrunSignal();
    return 0;
}

/* Return list of MIDI events that were buffered during JACK process callback(s). */
QList<KonfytMidiEvent> KonfytJackEngine::getEvents()
{
    return eventsRxBuffer.readAll();
}



/* Helper function for Jack process callback.
 * Send noteoffs to all ports with corresponding noteon events. */
void KonfytJackEngine::handleNoteoffEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort *sourcePort)
{
    for (int i=0; i < noteOnList.count(); i++) {
        KonfytJackNoteOnRecord* rec = noteOnList.at_ptr(i);
        if (rec->sourcePort != sourcePort) { continue; }
        // Apply global transpose that was active at time of noteon
        ev.data1 += rec->globalTranspose;
        // Check if event passes noteon filter
        if ( rec->filter.passFilter(&ev) ) {
            // Apply noteon filter modification
            KonfytMidiEvent newEv = rec->filter.modify( &ev );
            if (newEv.data1 == rec->note) {
                // Match! Send noteoff and remove noteon from list.
                if (rec->jackPortNotFluidsynth) {
                    // Get output buffer, based on size and time of input event
                    unsigned char* out_buffer = jack_midi_event_reserve( rec->port->buffer, inEvent_jack.time, inEvent_jack.size);
                    if (out_buffer) {
                        // Copy input event to output buffer
                        newEv.toBuffer( out_buffer );
                    }
                } else {
                    fluidsynthEngine->processJackMidi( rec->fluidsynthID, &newEv );
                }
                rec->port->noteOns--;

                noteOnList.remove(i);
                i--; // Due to removal, have to stay at same index after for loop i++
            }
        }
        ev.data1 -= rec->globalTranspose;
    }
}

/* Helper function for Jack process callback.
 * Send sustain zero to all Jack midi ports that have a recorded sustain event. */
void KonfytJackEngine::handleSustainoffEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort *sourcePort)
{
    for (int i=0; i < sustainList.count(); i++) {
        KonfytJackNoteOnRecord* rec = sustainList.at_ptr(i);
        if (rec->sourcePort != sourcePort) { continue; }
        // Check if event passes filter
        if ( rec->filter.passFilter(&ev) ) {
            // Apply filter modification
            KonfytMidiEvent newEv = rec->filter.modify( &ev );
            // Set sustain value to zero exactly
            newEv.data2 = 0;
            // Send sustain zero and remove from list
            if (rec->jackPortNotFluidsynth) {
                // Get output buffer, based on size and time of input event
                unsigned char* out_buffer = jack_midi_event_reserve( rec->port->buffer, inEvent_jack.time, inEvent_jack.size);
                if (out_buffer) {
                    // Copy input event to output buffer
                    newEv.toBuffer( out_buffer );
                }
            } else {
                fluidsynthEngine->processJackMidi( rec->fluidsynthID, &newEv );
            }
            rec->port->sustainNonZero = false;

            sustainList.remove(i);
            i--; // Due to removal, have to stay at same index after for loop i++
        }
    }
}

/* Helper function for Jack process callback.
 * Send pitchbend zero to all Jack midi ports that have a recorded pitchbend. */
void KonfytJackEngine::handlePitchbendZeroEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort *sourcePort)
{
    for (int i=0; i < pitchBendList.count(); i++) {
        KonfytJackNoteOnRecord* rec = pitchBendList.at_ptr(i);
        if (rec->sourcePort != sourcePort) { continue; }
        // Check if event passes filter
        if ( rec->filter.passFilter(&ev) ) {
            // Apply filter modification
            KonfytMidiEvent newEv = rec->filter.modify( &ev );
            // Send pitchbend zero and remove from list
            if (rec->jackPortNotFluidsynth) {
                // Get output buffer, based on size and time of input event
                unsigned char* out_buffer = jack_midi_event_reserve( rec->port->buffer, inEvent_jack.time, inEvent_jack.size);
                if (out_buffer) {
                    // Copy input event to output buffer
                    newEv.toBuffer( out_buffer );
                }
            } else {
                fluidsynthEngine->processJackMidi( rec->fluidsynthID, &newEv );
            }
            rec->port->pitchbendNonZero = false;

            pitchBendList.remove(i);
            i--; // Due to removal, have to stay at same index after for loop i++
        }
    }
}

// Helper function for Jack process callback
void KonfytJackEngine::mixBufferToDestinationPort(KonfytJackAudioRoute *route,
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
        ( (jack_default_audio_sample_t*)(route->dest->buffer) )[i] +=
                ((jack_default_audio_sample_t*)(route->source->buffer))[i]
                * gain * fadeOutValues[route->fadeoutCounter];

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
}

// Initialises MIDI closure events that will be sent to ports during Jack process callback.
void KonfytJackEngine::initMidiClosureEvents()
{
    evAllNotesOff.type = MIDI_EVENT_TYPE_CC;
    evAllNotesOff.data1 = MIDI_MSG_ALL_NOTES_OFF;
    evAllNotesOff.data2 = 0;

    evSustainZero.type = MIDI_EVENT_TYPE_CC;
    evSustainZero.data1 = 64;
    evSustainZero.data2 = 0;

    evPitchbendZero.type = MIDI_EVENT_TYPE_PITCHBEND;
    evPitchbendZero.setPitchbendData(0);
}

// Helper function for Jack process callback
void KonfytJackEngine::sendMidiClosureEvents(KonfytJackPort *port, int channel)
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

// Helper function for Jack process callback
void KonfytJackEngine::sendMidiClosureEvents_chanZeroOnly(KonfytJackPort *port)
{
    sendMidiClosureEvents(port, 0);
}

// Helper function for Jack process callback
void KonfytJackEngine::sendMidiClosureEvents_allChannels(KonfytJackPort *port)
{
    for (int i=0; i<15; i++) {
        sendMidiClosureEvents(port, i);
    }
}

bool KonfytJackEngine::initJackClient(QString name)
{
    // Try to become a client of the jack server
    if ( (client = jack_client_open(name.toLocal8Bit(), JackNullOption, NULL)) == NULL) {
        userMessage("JACK: Error becoming client.");
        this->clientActive = false;
        return false;
    } else {
        ourJackClientName = jack_get_client_name(client); // jack_client_open modifies the given name if another client already uses it.
        userMessage("JACK: Client created: " + ourJackClientName);
        this->clientActive = true;
    }


    // Set up callback functions
    jack_set_port_connect_callback(client, KonfytJackEngine::jackPortConnectCallback, this);
    jack_set_port_registration_callback(client, KonfytJackEngine::jackPortRegistrationCallback, this);
    jack_set_process_callback (client, KonfytJackEngine::jackProcessCallback, this);
    jack_set_xrun_callback(client, KonfytJackEngine::jackXrunCallback, this);

    nframes = jack_get_buffer_size(client);

    // Activate the client
    if (jack_activate(client)) {
        userMessage("JACK: Cannot activate client.");
        jack_free(client);
        this->clientActive = false;
        return false;
    } else {
        userMessage("JACK: Activated client.");
        this->clientActive = true;
    }

    // Get sample rate
    this->samplerate = jack_get_sample_rate(client);
    userMessage("JACK: Samplerate " + n2s(samplerate));

    fadeOutSecs = 1;
    fadeOutValuesCount = samplerate*fadeOutSecs;
    // Linear fadeout
    fadeOutValues = (float*)malloc(sizeof(float)*fadeOutValuesCount);
    for (unsigned int i=0; i<fadeOutValuesCount; i++) {
        fadeOutValues[i] = 1 - ((float)i/(float)fadeOutValuesCount);
    }

    startPortTimer(); // Timer that will take care of automatically restoring port connections

    return true;
}

void KonfytJackEngine::stopJackClient()
{
    if (clientIsActive()) {
        jack_client_close(client);
        this->clientActive = false;
    }
}

/* Get a string list of JACK ports, based on type pattern (e.g. "midi" or "audio", etc.)
 * and flags (e.g. JackPortIsInput or JackPortIsOutput). */
QStringList KonfytJackEngine::getPortsList(QString typePattern, unsigned long flags)
{
    QStringList pl;
    char** ports;

    ports = (char**)jack_get_ports(client,NULL,typePattern.toLocal8Bit(), flags);

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

/* Returns list of JACK midi input ports from the JACK server. */
QStringList KonfytJackEngine::getMidiInputPortsList()
{
    return getPortsList("midi", JackPortIsInput);
}

/* Returns list of JACK midi output ports from the JACK server. */
QStringList KonfytJackEngine::getMidiOutputPortsList()
{
    return getPortsList("midi", JackPortIsOutput);
}

/* Returns list of JACK audio input ports from the JACK server. */
QStringList KonfytJackEngine::getAudioInputPortsList()
{
    return getPortsList("audio", JackPortIsInput);
}

/* Returns list of JACK audio output ports from the JACK server. */
QStringList KonfytJackEngine::getAudioOutputPortsList()
{
    return getPortsList("audio", JackPortIsOutput);
}

double KonfytJackEngine::getSampleRate()
{
    return this->samplerate;
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

    refreshPortConnections();
}

void KonfytJackEngine::removeOtherJackConPair(KonfytJackConPair p)
{
    pauseJackProcessing(true);

    for (int i=0; i<otherConsList.count(); i++) {
        if (p.equals(otherConsList[i])) {
            otherConsList.removeAt(i);

            // Disconnect JACK ports
            if (clientIsActive()) {
                jack_disconnect(client, p.srcPort.toLocal8Bit(), p.destPort.toLocal8Bit());
            }

            break;
        }
    }

    pauseJackProcessing(false);
    refreshPortConnections();
}

void KonfytJackEngine::clearOtherJackConPair()
{
    pauseJackProcessing(true);

    otherConsList.clear();

    pauseJackProcessing(false);
}


/* Activate all the necessary routes for the given patch. */
void KonfytJackEngine::activateRoutesForPatch(const KonfytPatch* patch, bool active)
{
    if (!clientIsActive()) { return; }

    // MIDI output ports to external apps

    QList<KonfytPatchLayer> midiOutLayers = patch->getMidiOutputLayerList();
    foreach (KonfytPatchLayer layer, midiOutLayers) {
        if (layer.hasError()) { continue; }
        setMidiRouteActive(layer.midiOutputPortData.jackRouteId, active);
    }

    // Plugins

    QList<KonfytPatchLayer> pluginLayers = patch->getPluginLayerList();
    for (int i=0; i<pluginLayers.count(); i++) {
        KonfytPatchLayer layer = pluginLayers[i];
        if (layer.hasError()) { continue; }
        setPluginActive(layer.sfzData.portsIdInJackEngine, active);
    }

    // Soundfont ports

    QList<KonfytPatchLayer> sfLayers = patch->getSfLayerList();
    for (int i=0; i<sfLayers.count(); i++) {
        KonfytPatchLayer layer = sfLayers[i];
        if (layer.hasError()) { continue; }
        setSoundfontActive(layer.soundfontData.idInJackEngine, active);
    }

    // General audio input ports

    QList<KonfytPatchLayer> audioInLayers = patch->getAudioInLayerList();
    foreach (KonfytPatchLayer layer, audioInLayers) {
        if (layer.hasError()) { continue; }
        setAudioRouteActive(layer.audioInPortData.jackRouteIdLeft, active);
        setAudioRouteActive(layer.audioInPortData.jackRouteIdRight, active);
    }

}

void KonfytJackEngine::setGlobalTranspose(int transpose)
{
    this->globalTranspose = transpose;
}

KonfytJackAudioRoute *KonfytJackEngine::audioRouteFromId(int routeId)
{
    KonfytJackAudioRoute* route = audioRouteIdMap.value(routeId, NULL);
    if (route == NULL) {
        error_abort("Invalid audio route " + n2s(routeId));
    }
    return route;
}

KonfytJackMidiRoute *KonfytJackEngine::midiRouteFromId(int routeId)
{
    KonfytJackMidiRoute* route = midiRouteIdMap.value(routeId, NULL);
    if (route == NULL) {
        error_abort("Invalid MIDI route " + n2s(routeId));
    }
    return route;
}

KonfytJackPort *KonfytJackEngine::audioInPortFromId(int portId)
{
    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (!audio_in_ports.contains(port)) {
        error_abort("Invalid audio in port " + n2s(portId));
    }
    return port;
}

KonfytJackPort *KonfytJackEngine::audioOutPortFromId(int portId)
{
    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (!audio_out_ports.contains(port)) {
        error_abort("Invalid audio out port " + n2s(portId));
    }
    return port;
}

KonfytJackPort *KonfytJackEngine::midiInPortFromId(int portId)
{
    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (!midi_in_ports.contains(port)) {
        error_abort("Invalid MIDI in port " + n2s(portId));
    }
    return port;
}

KonfytJackPort *KonfytJackEngine::midiOutPortFromId(int portId)
{
    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (!midi_out_ports.contains(port)) {
        error_abort("Invalid MIDI out port " + n2s(portId));
    }
    return port;
}

jack_port_t *KonfytJackEngine::registerJackPort(KonfytJackPort* portStruct, jack_client_t *client, QString port_name, QString port_type, unsigned long flags, unsigned long buffer_size)
{
    jack_port_t* port = NULL;

    port = jack_port_by_name(client, port_name.toLocal8Bit());
    if (port != NULL) {
        userMessage("Got port by name!");
    } else {
        port = jack_port_register(client, port_name.toLocal8Bit(), port_type.toLocal8Bit(), flags, buffer_size);
    }

    if (port == NULL) {
        userMessage("Failed to create JACK port " + port_name);
    }

    portStruct->jack_pointer = port;

    return port;
}


// Print error message to stdout, and abort app.
void KonfytJackEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: konfytJackClient:" << msg.toLocal8Bit().constData();
    abort();
}

