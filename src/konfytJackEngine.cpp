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

#include "konfytJackEngine.h"
#include <iostream>


KonfytJackEngine::KonfytJackEngine(QObject *parent) :
    QObject(parent)
{
    this->clientActive = false;
    connectCallback = false;
    registerCallback = false;
    jack_process_busy = false;
    jack_process_disable = 0; // Non-zero value = disabled
    timer_busy = false;
    timer_disabled = false;
    fluidsynthEngine = NULL;
    initMidiClosureEvents();
    panicCmd = false;
    panicState = 0;
    idCounter = 150;
    globalTranspose = 0;

    fadeOutSecs = 0;
    fadeOutValuesCount = 0;
}

// Set panicCmd. The Jack process callback will behave accordingly.
void KonfytJackEngine::panic(bool p)
{
    panicCmd = p;
}


void KonfytJackEngine::timerEvent(QTimerEvent *event)
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

    // Refresh connections for audio output ports (aka busses)
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

// Add new soundfont ports. Also assigns midi filter.
int KonfytJackEngine::addSoundfont(LayerSoundfontStruct sf)
{
    /* Soundfonts use the same structures as Carla plugins for now, but are much simpler
     * as midi is given to the fluidsynth engine and audio is recieved from it without
     * external Jack connections. */

    KonfytJackPluginPorts* p = new KonfytJackPluginPorts();
    p->audio_in_l = new KonfytJackPort();
    p->audio_in_r = new KonfytJackPort();

    // Because our audio is not received from jack audio ports, we have to allocate
    // the buffers now so fluidsynth can write to them in the jack process callback.
    p->audio_in_l->buffer = malloc(sizeof(jack_default_audio_sample_t)*nframes);
    p->audio_in_r->buffer = malloc(sizeof(jack_default_audio_sample_t)*nframes);

    p->midi = new KonfytJackPort();
    p->midi->filter = sf.filter;
    p->id = idCounter++;
    p->idInPluginEngine = sf.indexInEngine;

    soundfont_ports.append(p);
    soundfontPortsMap.insert(p->id, p);

    return p->id;
}

void KonfytJackEngine::removeSoundfont(int id)
{
    if (!soundfontPortsMap.contains(id)) {
        error_abort("removeSoundfont: id out of range: " + n2s(id));
    }

    pauseJackProcessing(true);

    // Delete all objects created in addSoundfont()

    KonfytJackPluginPorts* p = soundfontPortsMap.value(id);
    soundfontPortsMap.remove( soundfontPortsMap.key(p) );

    // Remove all recorded noteon, sustain and pitchbend events related to this Fluidsynth ID.
    for (int i=0; i<noteOnList.count(); i++) {
        KonfytJackNoteOnRecord *rec = noteOnList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == id) {
                noteOnList.remove(i);
                i--; // Due to removal, have to stay at same index after for loop i++
            }
        }
    }
    for (int i=0; i<sustainList.count(); i++) {
        KonfytJackNoteOnRecord *rec = sustainList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == id) {
                sustainList.remove(i);
                i--;
            }
        }
    }
    for (int i=0; i<pitchBendList.count(); i++) {
        KonfytJackNoteOnRecord *rec = pitchBendList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == id) {
                pitchBendList.remove(i);
                i--;
            }
        }
    }

    soundfont_ports.removeAll(p);
    free(p->audio_in_l->buffer);
    free(p->audio_in_r->buffer);
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
    midiPort->active = false;
    midiPort->connectionList.append( spec.midiOutConnectTo );
    midiPort->mute = false;
    midiPort->prev_active = false;
    midiPort->solo = false;
    midiPort->filter = spec.midiFilter;

    // Add left audio input port where we will receive plugin audio
    QString nameL = spec.name + "_in_L";
    jack_port_t* newL = registerJackPort(alPort, client, nameL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (newL == NULL) {
        userMessage("Failed to create left audio input port '" + nameL + "' for plugin.");
    }
    alPort->active = false;
    alPort->connectionList.append( spec.audioInLeftConnectTo );
    alPort->destOrSrcPort = NULL;
    alPort->gain = 1;
    alPort->mute = false;
    alPort->prev_active = false;
    alPort->solo = false;

    // Add right audio input port
    QString nameR = spec.name + "_in_R";
    jack_port_t* newR = registerJackPort(arPort, client, nameR, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (newR == NULL) {
        userMessage("Failed to create right audio input port '" + nameR + "' for plugin.");
    }
    arPort->active = false;
    arPort->connectionList.append( spec.audioInRightConnectTo );
    arPort->destOrSrcPort = NULL;
    arPort->gain = 1;
    arPort->mute = false;
    arPort->prev_active = false;
    arPort->solo = false;

    KonfytJackPluginPorts* p = new KonfytJackPluginPorts();
    p->midi = midiPort;
    p->audio_in_l = alPort;
    p->audio_in_r = arPort;

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

    KonfytJackPluginPorts* p = pluginsPortsMap.value(id);
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
    if (soundfontPortsMap.contains(id)) {
        soundfontPortsMap.value(id)->midi->filter = filter;
    } else {
        error_abort("setSoundfontMidiFilter: id out of range: " + n2s(id));
    }
}

void KonfytJackEngine::setPluginMidiFilter(int id, KonfytMidiFilter filter)
{
    if (pluginsPortsMap.contains(id)) {
        pluginsPortsMap.value(id)->midi->filter = filter;
    } else {
        error_abort("setPluginMidiFilter: id " + n2s(id) + " out of range.");
    }
}

void KonfytJackEngine::setSoundfontSolo(int id, bool solo)
{
    if (soundfontPortsMap.contains(id)) {
        soundfontPortsMap.value(id)->audio_in_l->solo = solo;
        soundfontPortsMap.value(id)->audio_in_r->solo = solo;
        soundfontPortsMap.value(id)->midi->solo = solo;
        refreshSoloFlag();
    } else {
        error_abort("setSoundfontSolo: id out of range: " + n2s(id));
    }
}

void KonfytJackEngine::setPluginSolo(int id, bool solo)
{
    if (pluginsPortsMap.contains(id)) {
        pluginsPortsMap.value(id)->audio_in_l->solo = solo;
        pluginsPortsMap.value(id)->audio_in_r->solo = solo;
        pluginsPortsMap.value(id)->midi->solo = solo;
        refreshSoloFlag();
    } else {
        error_abort("setPluginSolo: id " + n2s(id) + " out of range.");
    }
}

void KonfytJackEngine::setSoundfontMute(int id, bool mute)
{
    if (soundfontPortsMap.contains(id)) {
        soundfontPortsMap.value(id)->audio_in_l->mute = mute;
        soundfontPortsMap.value(id)->audio_in_r->mute = mute;
        soundfontPortsMap.value(id)->midi->mute = mute;
    } else {
        error_abort("setSoundfontMute: id out of range: " + n2s(id));
    }
}

void KonfytJackEngine::setPluginMute(int id, bool mute)
{
    if (pluginsPortsMap.contains(id)) {
        pluginsPortsMap.value(id)->audio_in_l->mute = mute;
        pluginsPortsMap.value(id)->audio_in_r->mute = mute;
        pluginsPortsMap.value(id)->midi->mute = mute;
    } else {
        error_abort("setPluginMute: id " + n2s(id) + " out of range.");
    }
}

void KonfytJackEngine::setPluginGain(int id, float gain)
{
    if (pluginsPortsMap.contains(id)) {
        KonfytJackPluginPorts* p = pluginsPortsMap.value(id);
        p->audio_in_l->gain = gain;
        p->audio_in_r->gain = gain;
    } else {
        error_abort("setPluginGain: id " + n2s(id) + " out of range.");
    }
}

void KonfytJackEngine::setSoundfontRouting(int id, int midiInPortId, int leftPortId, int rightPortId)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* midiPort = portIdMap.value(midiInPortId, NULL);
    if (midiPort == NULL) {
        error_abort("Invalid MIDI-In port " + n2s(midiInPortId));
        return;
    }

    KonfytJackPort* leftPort = portIdMap.value(leftPortId, NULL);
    if (leftPort == NULL) {
        error_abort("Invalid left port " + n2s(leftPortId));
        return;
    }

    KonfytJackPort* rightPort = portIdMap.value(rightPortId, NULL);
    if (rightPort == NULL) {
        error_abort("Invalid right port " + n2s(rightPortId));
        return;
    }

    if (soundfontPortsMap.contains(id)) {
        KonfytJackPluginPorts* p = soundfontPortsMap.value(id);

        pauseJackProcessing(true);
        p->midi->destOrSrcPort = midiPort;
        p->audio_in_l->destOrSrcPort = leftPort;
        p->audio_in_r->destOrSrcPort = rightPort;
        pauseJackProcessing(false);

    } else {
        error_abort("setSoundfontRouting: id out of range: " + n2s(id));
    }
}

void KonfytJackEngine::setPluginRouting(int pluginId, int midiInPortId, int leftPortId, int rightPortId)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* midiPort = portIdMap.value(midiInPortId, NULL);
    if (midiPort == NULL) {
        error_abort("Invalid MIDI-In port " + n2s(midiInPortId));
        return;
    }

    KonfytJackPort* leftPort = portIdMap.value(leftPortId, NULL);
    if (leftPort == NULL) {
        error_abort("Invalid left port " + n2s(leftPortId));
        return;
    }

    KonfytJackPort* rightPort = portIdMap.value(rightPortId, NULL);
    if (rightPort == NULL) {
        error_abort("Invalid right port " + n2s(rightPortId));
        return;
    }

    if (pluginsPortsMap.contains(pluginId)) {
        KonfytJackPluginPorts* p = pluginsPortsMap.value(pluginId);

        pauseJackProcessing(true);
        p->midi->destOrSrcPort = midiPort;
        p->audio_in_l->destOrSrcPort = leftPort;
        p->audio_in_r->destOrSrcPort = rightPort;
        pauseJackProcessing(false);

    } else {
        error_abort("setPluginRouting: plugin id " + n2s(pluginId) + " out of range.");
    }
}


void KonfytJackEngine::refreshSoloFlag()
{
    // First check external solo flag
    if (this->soloFlag_external) {
        soloFlag = true;
        return;
    }
    // Else, check internal
    soloFlag = this->getSoloFlagInternal();
}

/* Returns whether any of the internal layers are set so solo. */
bool KonfytJackEngine::getSoloFlagInternal()
{
    // Check if a plugin is solo
    for (int i=0; i<plugin_ports.count(); i++) {
        if (plugin_ports[i]->midi->active) {
            if (plugin_ports[i]->midi->solo == true) {
                return true;
            }
        }
    }
    // Check if a soundfont is solo
    for (int i=0; i<soundfont_ports.count(); i++) {
        if (soundfont_ports[i]->midi->active) {
            if (soundfont_ports[i]->midi->solo == true) {
                return true;
            }
        }
    }
    // Check if a midi output port is set solo.
    for (int i=0; i<midi_out_ports.count(); i++) {
        if (midi_out_ports[i]->active) {
            if ( midi_out_ports.at(i)->solo == true) {
                return true;
            }
        }
    }
    // Check if an audio input port is set solo.
    for (int i=0; i<audio_in_ports.count(); i++) {
        if (audio_in_ports[i]->active) {
            if ( audio_in_ports.at(i)->solo == true) {
                return true;
            }
        }
    }
    // Else, no solo is set.
    return false;
}

/* Sets the soloFlag_external, i.e. for use if an external engine as a solo set. */
void KonfytJackEngine::setSoloFlagExternal(bool newSolo)
{
    this->soloFlag_external = newSolo;
    this->refreshSoloFlag();
}

void KonfytJackEngine::removeAllAudioInAndOutPorts()
{
    // Disable audio port processing in Jack process callback function,
    // and wait for processing to finish before removing ports.
    pauseJackProcessing(true);

    for (int i=0; i<audio_in_ports.count(); i++) {
        KonfytJackPort* p = audio_in_ports[i];
        jack_port_unregister(client, p->jack_pointer);
        portIdMap.remove( portIdMap.key(p) );
        delete p;
    }
    audio_in_ports.clear();

    for (int i=0; i<audio_out_ports.count(); i++) {
        KonfytJackPort* p = audio_out_ports[i];
        jack_port_unregister(client, p->jack_pointer);
        portIdMap.remove( portIdMap.key(p) );
        delete p;
    }
    audio_out_ports.clear();

    // Ensure all the destinationPort member of ports are NULL, since there
    // are now no more output ports (busses) left.
    nullDestinationPorts_all();

    // Enable audio port processing again.
    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAllMidiInPorts()
{
    pauseJackProcessing(true);

    while (midi_in_ports.count()) {
        KonfytJackPort* p = midi_in_ports[0];
        int id = portIdMap.key(p);
        removePort(id);
    }

    pauseJackProcessing(false);
}

void KonfytJackEngine::removeAllMidiOutPorts()
{
    pauseJackProcessing(true);

    while (midi_out_ports.count()) {
        KonfytJackPort* p = midi_out_ports[0];
        int id = portIdMap.key(p);
        removePort(id);
    }

    pauseJackProcessing(false);
}

/* Set the destinationPort to NULL for all ports that make use of this
 * (i.e. for audio input ports and plugin audio ports).
 * NULL is used by the Jack process callback to determine that the
 * destination port is not set/valid. */
void KonfytJackEngine::nullDestinationPorts_all()
{
    pauseJackProcessing(true);

    // Audio input ports
    for (int i=0; i<audio_in_ports.count(); i++) {
        audio_in_ports[i]->destOrSrcPort = NULL;
    }

    // Plugin audio ports
    for (int i=0; i<plugin_ports.count(); i++) {
        plugin_ports[i]->audio_in_l->destOrSrcPort = NULL;
        plugin_ports[i]->audio_in_r->destOrSrcPort = NULL;
    }

    // Soundfont audio ports
    for (int i=0; i<soundfont_ports.count(); i++) {
        soundfont_ports[i]->audio_in_l->destOrSrcPort = NULL;
        soundfont_ports[i]->audio_in_r->destOrSrcPort = NULL;
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

/* Set the destinationPort to NULL for all ports where it points to the specified
 * port. */
void KonfytJackEngine::nullDestinationPorts_pointingTo(KonfytJackPort *port)
{
    pauseJackProcessing(true);

    // Audio input ports
    for (int i=0; i<audio_in_ports.count(); i++) {
        if (audio_in_ports[i]->destOrSrcPort == port) {
            audio_in_ports[i]->destOrSrcPort = NULL;
        }
    }

    // Plugin audio ports
    for (int i=0; i<plugin_ports.count(); i++) {
        if (plugin_ports[i]->audio_in_l->destOrSrcPort == port) {
            plugin_ports[i]->audio_in_l->destOrSrcPort = NULL;
        }
        if (plugin_ports[i]->audio_in_r->destOrSrcPort == port) {
            plugin_ports[i]->audio_in_r->destOrSrcPort = NULL;
        }
    }

    // Soundfont audio ports
    for (int i=0; i<soundfont_ports.count(); i++) {
        if (soundfont_ports[i]->audio_in_l->destOrSrcPort == port) {
            soundfont_ports[i]->audio_in_l->destOrSrcPort = NULL;
        }
        if (soundfont_ports[i]->audio_in_r->destOrSrcPort == port) {
            soundfont_ports[i]->audio_in_r->destOrSrcPort = NULL;
        }
    }

    pauseJackProcessing(false);
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

    // If this is an audio out port (i.e. bus), ensure that all destinationPorts
    // that pointed to this is set to NULL to ensure no errors in the Jack process
    // callback.
    if (l == &audio_out_ports) {
        nullDestinationPorts_pointingTo(port);
    }

    refreshSoloFlag();

    // Enable Jack process callback again
    pauseJackProcessing(false);
}


/* Dummy function to copy structure from. */
void KonfytJackEngine::dummyFunction(KonfytJackPortType type)
{
    switch (type) {
    case KonfytJackPortType_MidiIn:

        break;
    case KonfytJackPortType_MidiOut:

        break;
    case KonfytJackPortType_AudioIn:

        break;
    case KonfytJackPortType_AudioOut:

        break;
    default:
        error_abort( "Unknown Jack Port Type." );
    }
}

/* Adds a new port to JACK with the specified type and name. Returns a unique
 * port ID. */
int KonfytJackEngine::addPort(KonfytJackPortType type, QString port_name)
{
    if (!clientIsActive()) { return KONFYT_JACK_PORT_ERROR; }

    pauseJackProcessing(true);

    jack_port_t* newPort;
    QList<KonfytJackPort*> *listToAddTo;

    KonfytJackPort* p = new KonfytJackPort();

    switch (type) {
    case KonfytJackPortType_MidiIn:

        newPort = registerJackPort (p, client, port_name, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
        listToAddTo = &midi_in_ports;

        break;
    case KonfytJackPortType_MidiOut:

        newPort = registerJackPort (p, client, port_name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
        listToAddTo = &midi_out_ports;

        break;
    case KonfytJackPortType_AudioOut:

        newPort = registerJackPort (p, client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        listToAddTo = &audio_out_ports;

        break;
    case KonfytJackPortType_AudioIn:

        newPort = registerJackPort (p, client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        listToAddTo = &audio_in_ports;

        break;
    }

    if (newPort == NULL) {
        userMessage("Failed to add JACK port " + port_name);
    }

    if (type == KonfytJackPortType_AudioOut) {
        p->active = true;
        p->prev_active = true;
    } else {
        p->active = false;
        p->prev_active = false;
    }
    p->solo = false;
    p->mute = false;
    p->gain = 1;
    p->destOrSrcPort = NULL;

    // Create unique ID for port
    p->id = idCounter++;

    listToAddTo->append(p);
    portIdMap.insert(p->id, p);

    pauseJackProcessing(false);
    return p->id;
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

    int order;
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

void KonfytJackEngine::setPortSolo(int portId, bool solo)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
        return;
    }

    port->solo = solo;
}

void KonfytJackEngine::setPortMute(int portId, bool mute)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
        return;
    }

    port->mute = mute;
}

void KonfytJackEngine::setPortGain(int portId, float gain)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
        return;
    }

    port->gain = gain;
}

/* Route the port specified by basePortId to the port specified by routePortId.
 * The destOrSrcPort field of basePort is set to routePort.
 * Thus, for an AudioIn type, this means that audio will be routed to the
 * destination port (bus) specified by routePort.
 * For a MidiOut type, this means that MIDI will be routed from the source port
 * specified by routePort to the MidiOut port. */
void KonfytJackEngine::setPortRouting(int basePortId, int routePortId)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* srcPort = portIdMap.value(basePortId, NULL);
    if (srcPort == NULL) {
        error_abort("Invalid source port " + n2s(basePortId));
        return;
    }

    KonfytJackPort* destPort = portIdMap.value(routePortId, NULL);
    if (destPort == NULL) {
        error_abort("Invalid destination port " + n2s(routePortId));
        return;
    }

    QList<KonfytJackPort*> *lsrc = getListContainingPort(basePortId);
    if (lsrc == NULL) {
        error_abort("Source port not in any list.");
        return;
    }

    QList<KonfytJackPort*> *ldest = getListContainingPort(routePortId);
    if (ldest == NULL) {
        error_abort("Destination port not in any list.");
        return;
    }

    pauseJackProcessing(true);

    if (lsrc == &audio_in_ports) {

        if (ldest == &audio_out_ports) {
            srcPort->destOrSrcPort = destPort;
        } else {
            error_abort("setPortRouting: Source is audio in but dest is not audio out.");
        }

    } else if (lsrc == &midi_out_ports) {

        if (ldest == &midi_in_ports) {
            srcPort->destOrSrcPort = destPort;
        } else {
            error_abort("setPortRouting: Source is MIDI out but dest is not MIDI in.");
        }

    } else {
        error_abort("setPortRouting: Invalid source port type.");
    }

    pauseJackProcessing(false);
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

void KonfytJackEngine::jackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void *arg)
{
    KonfytJackEngine* e = (KonfytJackEngine*)arg;

    e->connectCallback = true;
}

void KonfytJackEngine::jackPortRegistrationCallback(jack_port_id_t port, int registered, void *arg)
{
    KonfytJackEngine* e = (KonfytJackEngine*)arg;

    e->registerCallback = true;
}

int KonfytJackEngine::jackProcessCallback(jack_nframes_t nframes, void *arg)
{
    KonfytJackEngine* e = (KonfytJackEngine*)arg;

    // Lock jack_process ----------------------
    e->jack_process_busy = true;
    if (e->jack_process_disable) {
        e->jack_process_busy = false;
        //e->userMessage("Jack process callback locked out.");
        return 0;
    }
    // ----------------------------------------

    int n, id;
    uint32_t i;
    KonfytJackPort* tempPort;
    KonfytJackPort* tempPort2;

    // panicCmd is the panic command received from the outside.
    // panicState is our internal panic state where zero=no panic.
    if (e->panicCmd) {
        if (e->panicState == 0) {
            // If the panic command is true and we are not in a panic state yet, go into one.
            e->panicState = 1;
        }
    } else {
        // If the panic command is false, ensure we are not in a panic state.
        e->panicState = 0;
    }

    // ------------------------------------- Start of bus processing

    // An output port is a bus destination with its gain.
    // Other audio input ports are routed to the bus output.

    // Get all out ports (bus) buffers
    for (n=0; n<e->audio_out_ports.count(); n++) {
        tempPort = e->audio_out_ports.at(n);
        tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes); // Bus output buffer
        for (i=0; i<nframes; i++) { ((jack_default_audio_sample_t*)(tempPort->buffer))[i] = 0; } // Reset buffer
    }

    if (e->panicState > 0) {
        // We are in a panic state. Write zero to all busses.
        for (n=0; n<e->audio_out_ports.count(); n++) {
            tempPort = e->audio_out_ports.at(n);
            // Write zero to output for each frame
            for (i=0; i<nframes; i++) {
                ( (jack_default_audio_sample_t*)(tempPort->buffer) )[i] = 0;
            }
        }
    } else {

        // For each audio in port, if active, mix input buffer to destination bus (out port)
        for (n=0; n<e->audio_in_ports.count(); n++) {
            tempPort = e->audio_in_ports.at(n);
            // If active:
            //   If counter >0:
            //     Count down; Multiply with output
            //   outputFlag = 1
            // Else:
            //   If counter < max:
            //     Count up; Multiply with output
            //     OutputFlag = 1
            //   Else:
            //     OutputFlag = 0
            // If outputFlag:
            //   Do Output
            bool outputFlag = false;
            if (e->passMuteSoloActiveCriteria( tempPort )) {
                tempPort->fadingOut = false;
                outputFlag = true;
            } else {
                if (tempPort->fadeoutCounter < (e->fadeOutValuesCount-1) ) {
                    tempPort->fadingOut = true;
                    outputFlag = true;
                }
            }
            if (outputFlag) {
                tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes );
                e->mixBufferToDestinationPort( tempPort, nframes, true );
            }

        }

        // For each soundfont audio in port, mix to destination bus
        if (e->fluidsynthEngine != NULL) {
            for (n=0; n<e->soundfont_ports.count(); n++) {
                tempPort = e->soundfont_ports[n]->audio_in_l; // Left
                if ( e->passMuteSoloCriteria(tempPort) ) { // Don't check if port is active, only solo and mute
                    tempPort2 = e->soundfont_ports[n]->audio_in_r;
                    // We are not getting our audio from Jack audio ports, but from fluidsynth.
                    // The buffers have already been allocated when we added the soundfont layer to the engine.
                    // Get data from fluidsynth

                    e->fluidsynthEngine->fluidsynthWriteFloat( e->soundfont_ports[n]->idInPluginEngine,
                                                               ((jack_default_audio_sample_t*)tempPort->buffer),
                                                               ((jack_default_audio_sample_t*)tempPort2->buffer), nframes );


                    e->mixBufferToDestinationPort( tempPort, nframes, false );
                    e->mixBufferToDestinationPort( tempPort2, nframes, false );
                }
            }
        }


        // For each plugin audio in port, mix to destination bus
        for (n=0; n<e->plugin_ports.count(); n++) {
            tempPort = e->plugin_ports[n]->audio_in_l; // Left
            if (tempPort->jack_pointer == NULL) { continue; } // debug
            if ( e->passMuteSoloCriteria( tempPort ) ) { // Don't check if port is active, only solo and mute
                // Left
                tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes );
                e->mixBufferToDestinationPort( tempPort, nframes, true );
                // Right
                tempPort = e->plugin_ports[n]->audio_in_r;
                tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes );
                e->mixBufferToDestinationPort( tempPort, nframes, true );
            }
        }


        // Finally, apply the gain of each active bus, or write zero if not active
        for (n=0; n<e->audio_out_ports.count(); n++) {
            tempPort = e->audio_out_ports.at(n);
            if (tempPort->active) {
                // Do for each frame
                for (i=0; i<nframes; i++) {
                    ( (jack_default_audio_sample_t*)( tempPort->buffer ) )[i] *= tempPort->gain;
                }
            } else {
                // Write zero to output for each frame
                for (i=0; i<nframes; i++) {
                    ( (jack_default_audio_sample_t*)(tempPort->buffer) )[i] = 0;
                }
            }
        }

    } // end of panicState else

    // ------------------------------------- End of bus processing

    // Get buffers for midi output ports to external apps
    for (n=0; n<e->midi_out_ports.count(); n++) {
        tempPort = e->midi_out_ports.at(n);
        tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes);
        jack_midi_clear_buffer( tempPort->buffer );
    }
    // Get buffers for midi output ports to plugins
    for (n=0; n<e->plugin_ports.count(); n++) {
        tempPort = e->plugin_ports[n]->midi;
        tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes );
        jack_midi_clear_buffer( tempPort->buffer );
    }


    if (e->panicState == 1) {
        // We just entered panic state. Send note off messages etc, and proceed to state 2 where we just wait.

        // Send to fluidsynth
        for (n=0; n<e->soundfont_ports.count(); n++) {
            tempPort = e->soundfont_ports.at(n)->midi;
            id = e->soundfont_ports.at(n)->idInPluginEngine;
            // Fluidsynthengine will force event channel to zero
            e->fluidsynthEngine->processJackMidi( id, &(e->evAllNotesOff) );
            e->fluidsynthEngine->processJackMidi( id, &(e->evSustainZero) );
            e->fluidsynthEngine->processJackMidi( id, &(e->evPitchbendZero) );
        }

        // Give to all active output ports to external apps
        for (n=0; n<e->midi_out_ports.count(); n++) {
            tempPort = e->midi_out_ports.at(n);
            e->sendMidiClosureEvents_allChannels( tempPort ); // Send to all MIDI channels
        }

        // Also give to all active plugin ports
        for (n=0; n<e->plugin_ports.count(); n++) {
            tempPort = e->plugin_ports[n]->midi;
            e->sendMidiClosureEvents_chanZeroOnly( tempPort ); // Only on channel zero
        }

        e->panicState = 2; // Proceed to panicState 2 where we will just wait for panic to subside.

    }


    // For each midi input port...
    for (int prt=0; prt < e->midi_in_ports.count(); prt++) {

        KonfytJackPort* sourcePort = e->midi_in_ports[prt];
        void* input_port_buf = jack_port_get_buffer(sourcePort->jack_pointer, nframes);
        jack_nframes_t input_event_count = jack_midi_get_event_count(input_port_buf);
        jack_midi_event_t inEvent_jack;

        // For each midi input event...
        for (i=0; i<input_event_count; i++) {

            // Get input event
            jack_midi_event_get(&inEvent_jack, input_port_buf, i);

            QByteArray inEvent_jack_tempBuffer((const char*)inEvent_jack.buffer, (int)inEvent_jack.size);
            KonfytMidiEvent ev( inEvent_jack_tempBuffer );

            // Apply input MIDI port filter
            if (sourcePort->filter.passFilter(&ev)) {
                ev = sourcePort->filter.modify(&ev);
            } else {
                // Event doesn't pass filter. Skip.
                continue;
            }

            // Send to GUI
            if (ev.type != MIDI_EVENT_TYPE_SYSTEM) {
                emit e->midiEventSignal( ev, sourcePort->id );
            }

            bool passEvent = true;
            bool recordNoteon = false;
            bool recordSustain = false;
            bool recordPitchbend = false;


            if (ev.type == MIDI_EVENT_TYPE_NOTEOFF) {
                passEvent = false;
                e->handleNoteoffEvent(ev, inEvent_jack, sourcePort);
            } else if ( (ev.type == MIDI_EVENT_TYPE_CC) && (ev.data1 == 64) ) {
                if (ev.data2 <= KONFYT_JACK_SUSTAIN_THRESH) {
                    // Sustain zero
                    passEvent = false;
                    e->handleSustainoffEvent(ev, inEvent_jack, sourcePort);
                } else {
                    recordSustain = true;
                }
            } else if ( (ev.type == MIDI_EVENT_TYPE_PITCHBEND) ) {
                if (ev.pitchbendValue_signed() == 0) {
                    // Pitchbend zero
                    passEvent = false;
                    e->handlePitchbendZeroEvent(ev, inEvent_jack, sourcePort);
                } else {
                    recordPitchbend = true;
                }
            } else if ( ev.type == MIDI_EVENT_TYPE_NOTEON ) {
                ev.data1 += e->globalTranspose;
                if ( (ev.data1 < 0) || (ev.data1 > 127) ) {
                    passEvent = false;
                }
                recordNoteon = true;
            }


            if (e->panicState == 0) {
                if ( passEvent ) {
                    // Give to Fluidsynth
                    e->processMidiEventForFluidsynth(sourcePort, ev, recordNoteon, recordSustain, recordPitchbend);
                    // Give to all active output ports to external apps
                    e->processMidiEventForMidiOutPorts(sourcePort, inEvent_jack, ev, recordNoteon, recordSustain, recordPitchbend);
                    // Also give to all active plugin ports
                    e->processMidiEventForPlugins(sourcePort, inEvent_jack, ev, recordNoteon, recordSustain, recordPitchbend);
                }
            }

        } // end for each midi input event

    } // end for each midi input port


    // Unlock jack_process --------------------
    e->jack_process_busy = false;
    // ----------------------------------------

    return 0;

} // end of jackProcessCallback


int KonfytJackEngine::jackXrunCallback(void *arg)
{   
    KonfytJackEngine* e = (KonfytJackEngine*)arg;
    e->xrunSignal();
    return 0;
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
                if (rec->relatedPort!=NULL) { rec->relatedPort->noteOns--; } // TODO: Related ports possibly not needed anymore.

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
            if (rec->relatedPort!=NULL) { rec->relatedPort->sustainNonZero = false; }

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
            if (rec->relatedPort!=NULL) { rec->relatedPort->pitchbendNonZero = false; }

            pitchBendList.remove(i);
            i--; // Due to removal, have to stay at same index after for loop i++
        }
    }
}

/* Helper function for Jack process callback.
 * Sends Midi event to the Fluidsynth engine if it passes the necessary filters,
 * and do additional processing like recording noteon, sustain and pitchbend
 * events. */
void KonfytJackEngine::processMidiEventForFluidsynth(KonfytJackPort* sourcePort,
                                                     KonfytMidiEvent &ev,
                                                     bool recordNoteon,
                                                     bool recordSustain,
                                                     bool recordPitchbend)
{
    int n, id;
    KonfytJackPort* tempPort;
    KonfytJackPort* tempPort2;
    KonfytMidiEvent evToSend;

    // Send to fluidsynth
    for (n=0; n < soundfont_ports.count(); n++) {
        tempPort = soundfont_ports.at(n)->midi;
        if (tempPort->destOrSrcPort != sourcePort) { continue; }
        tempPort2 = soundfont_ports.at(n)->audio_in_l; // Related audio port
        id = soundfont_ports.at(n)->idInPluginEngine;
        if ( passMidiMuteSoloActiveFilterAndModify(tempPort, &ev, &evToSend) ) {

            fluidsynthEngine->processJackMidi( id, &evToSend );

            // Record noteon, sustain or pitchbend for off events later.
            if (recordNoteon) {
                KonfytJackNoteOnRecord rec;
                rec.filter = tempPort->filter;
                rec.fluidsynthID = id;
                rec.globalTranspose = globalTranspose;
                rec.jackPortNotFluidsynth = false;
                rec.port = tempPort;
                rec.relatedPort = tempPort2;
                rec.note = evToSend.data1;
                rec.sourcePort = sourcePort;
                noteOnList.add(rec);
                tempPort->noteOns++;
                tempPort2->noteOns++;
            } else if (recordPitchbend) {
                // If this port hasn't saved a pitchbend yet
                if ( !tempPort->pitchbendNonZero ) {
                    KonfytJackNoteOnRecord rec;
                    rec.filter = tempPort->filter;
                    rec.fluidsynthID = id;
                    rec.jackPortNotFluidsynth = false;
                    rec.port = tempPort;
                    rec.relatedPort = tempPort2;
                    rec.sourcePort = sourcePort;
                    pitchBendList.add(rec);
                    tempPort->pitchbendNonZero = true;
                    tempPort2->pitchbendNonZero = true;
                }
            } else if (recordSustain) {
                // If this port hasn't saved a sustain yet
                if ( !tempPort->sustainNonZero ) {
                    KonfytJackNoteOnRecord rec;
                    rec.filter = tempPort->filter;
                    rec.fluidsynthID = id;
                    rec.jackPortNotFluidsynth = false;
                    rec.port = tempPort;
                    rec.relatedPort = tempPort2;
                    rec.sourcePort = sourcePort;
                    sustainList.add(rec);
                    tempPort->sustainNonZero = true;
                    tempPort2->sustainNonZero = true;
                }
            }

        } else if ( (tempPort->prev_active) && !(tempPort->active) ) {
            // Was previously active, but not anymore.
        }
        tempPort->prev_active = tempPort->active;
    }
}

/* Helper function for Jack process callback.
 * Sends Midi event to Midi output ports if it passes the necessary filters,
 * and do additional processing like recording noteon, sustain and pitchbend
 * events. */
void KonfytJackEngine::processMidiEventForMidiOutPorts(KonfytJackPort *sourcePort,
                                                       jack_midi_event_t inEvent_jack,
                                                       KonfytMidiEvent &ev,
                                                       bool recordNoteon,
                                                       bool recordSustain,
                                                       bool recordPitchbend)
{
    int n;
    KonfytJackPort* tempPort;
    unsigned char* out_buffer;
    KonfytMidiEvent evToSend;

    for (n=0; n < midi_out_ports.count(); n++) {
        tempPort = midi_out_ports.at(n);
        if (tempPort->destOrSrcPort != sourcePort) { continue; }
        if ( passMidiMuteSoloActiveFilterAndModify(tempPort, &ev, &evToSend) ) {

            // Get output buffer, based on size and time of input event
            out_buffer = jack_midi_event_reserve( tempPort->buffer, inEvent_jack.time, inEvent_jack.size);
            if (out_buffer == 0) {
                // JACK error
                continue;
            }

            // Copy input event to output buffer
            evToSend.toBuffer( out_buffer );

            // Record noteon, sustain or pitchbend for off events later.
            if (recordNoteon) {
                KonfytJackNoteOnRecord rec;
                rec.filter = tempPort->filter;
                rec.globalTranspose = globalTranspose;
                rec.jackPortNotFluidsynth = true;
                rec.note = evToSend.data1;
                rec.port = tempPort;
                rec.relatedPort = NULL;
                rec.sourcePort = sourcePort;
                noteOnList.add(rec);
                tempPort->noteOns++;
            } else if (recordPitchbend) {
                // If this port hasn't saved a pitchbend yet
                if ( !tempPort->pitchbendNonZero ) {
                    KonfytJackNoteOnRecord rec;
                    rec.filter = tempPort->filter;
                    rec.jackPortNotFluidsynth = true;
                    rec.port = tempPort;
                    rec.relatedPort = NULL;
                    rec.sourcePort = sourcePort;
                    pitchBendList.add(rec);
                    tempPort->pitchbendNonZero = true;
                }
            } else if (recordSustain) {
                // If this port hasn't saved a pitchbend yet
                if ( !tempPort->sustainNonZero ) {
                    KonfytJackNoteOnRecord rec;
                    rec.filter = tempPort->filter;
                    rec.jackPortNotFluidsynth = true;
                    rec.port = tempPort;
                    rec.relatedPort = NULL;
                    rec.sourcePort = sourcePort;
                    sustainList.add(rec);
                    tempPort->sustainNonZero = true;
                }
            }

        } else if ( (tempPort->prev_active) && !(tempPort->active) ) {
            // Was previously active, but not anymore.
        }
        tempPort->prev_active = tempPort->active;
    }
}

/* Helper function for Jack process callback.
 * Sends Midi event to plugin Midi ports if it passes the necessary filters,
 * and do additional processing like recording noteon, sustain and pitchbend
 * events. */
void KonfytJackEngine::processMidiEventForPlugins(KonfytJackPort *sourcePort,
                                                  jack_midi_event_t inEvent_jack,
                                                  KonfytMidiEvent &ev,
                                                  bool recordNoteon,
                                                  bool recordSustain,
                                                  bool recordPitchbend)
{
    int n;
    KonfytJackPort* tempPort;
    KonfytJackPort* tempPort2;
    unsigned char* out_buffer;
    KonfytMidiEvent evToSend;

    for (n=0; n < plugin_ports.count(); n++) {
        tempPort = plugin_ports[n]->midi;
        if (tempPort->destOrSrcPort != sourcePort) { continue; }
        tempPort2 = plugin_ports[n]->audio_in_l; // related audio port
        if ( passMidiMuteSoloActiveFilterAndModify(tempPort, &ev, &evToSend) ) {

            // Get output buffer, based on size and time of input event
            out_buffer = jack_midi_event_reserve( tempPort->buffer, inEvent_jack.time, inEvent_jack.size);
            if (out_buffer == 0) {
                // JACK error
                continue;
            }

            // Force MIDI channel 0
            evToSend.channel = 0;

            // Copy input event to output buffer
            evToSend.toBuffer( out_buffer );

            // Record noteon, sustain or pitchbend for off events later.
            if (recordNoteon) {
                KonfytJackNoteOnRecord rec;
                rec.filter = tempPort->filter;
                rec.globalTranspose = globalTranspose;
                rec.jackPortNotFluidsynth = true;
                rec.note = evToSend.data1;
                rec.port = tempPort;
                rec.relatedPort = tempPort2;
                rec.sourcePort = sourcePort;
                noteOnList.add(rec);
                tempPort->noteOns++;
                tempPort2->noteOns++;
            } else if (recordPitchbend) {
                // If this port hasn't saved a pitchbend yet
                if ( !tempPort->pitchbendNonZero ) {
                    KonfytJackNoteOnRecord rec;
                    rec.filter = tempPort->filter;
                    rec.jackPortNotFluidsynth = true;
                    rec.port = tempPort;
                    rec.relatedPort = tempPort2;
                    rec.sourcePort = sourcePort;
                    pitchBendList.add(rec);
                    tempPort->pitchbendNonZero = true;
                    tempPort2->pitchbendNonZero = true;
                }
            } else if (recordSustain) {
                // If this port hasn't saved a pitchbend yet
                if ( !tempPort->sustainNonZero ) {
                    KonfytJackNoteOnRecord rec;
                    rec.filter = tempPort->filter;
                    rec.jackPortNotFluidsynth = true;
                    rec.port = tempPort;
                    rec.relatedPort = tempPort2;
                    rec.sourcePort = sourcePort;
                    sustainList.add(rec);
                    tempPort->sustainNonZero = true;
                    tempPort2->sustainNonZero = true;
                }
            }

        } else if ( (tempPort->prev_active) && !(tempPort->active) ) {
            // Was previoiusly active, but not anymore.
        }
        tempPort->prev_active = tempPort->active;
    }
}



// Helper function for Jack process callback
bool KonfytJackEngine::passMuteSoloActiveCriteria(KonfytJackPort* port)
{
    if ( port->active ) {
        return passMuteSoloCriteria(port);
    }
    return false;
}

bool KonfytJackEngine::passMuteSoloCriteria(KonfytJackPort *port)
{
    if (port->mute == false) {
        if ( ( soloFlag && port->solo ) || (soloFlag==false) ) {
            return true;
        }
    }
    return false;
}

// Helper function for Jack process callback
void KonfytJackEngine::mixBufferToDestinationPort(KonfytJackPort* port, jack_nframes_t nframes, bool applyGain)
{
    if (port->destOrSrcPort == NULL) { return; }

    float gain = 1;
    if (applyGain) {
        gain = port->gain;
    }

    // For each frame: bus_buffer[frame] += port_buffer[frame]
    for (uint32_t i=0; i<nframes; i++) {

        ( (jack_default_audio_sample_t*)(port->destOrSrcPort->buffer) )[i] +=
                ((jack_default_audio_sample_t*)(port->buffer))[i] * gain * fadeOutValues[port->fadeoutCounter];

        if (port->fadingOut) {
            if (port->fadeoutCounter < (fadeOutValuesCount-1) ) {
                port->fadeoutCounter++;
            }
        } else {
            if (port->fadeoutCounter>0) {
                port->fadeoutCounter--;
            }
        }
    }
}

// Initialises MIDI closure events that will be sent to ports during Jack process callback.
void KonfytJackEngine::initMidiClosureEvents()
{
    evAllNotesOff = KonfytMidiEvent();
    evAllNotesOff.type = MIDI_EVENT_TYPE_CC;
    evAllNotesOff.data1 = MIDI_MSG_ALL_NOTES_OFF;
    evAllNotesOff.data2 = 0;

    evSustainZero = KonfytMidiEvent();
    evSustainZero.type = MIDI_EVENT_TYPE_CC;
    evSustainZero.data1 = 64;
    evSustainZero.data2 = 0;

    evPitchbendZero = KonfytMidiEvent();
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

// Helper function for Jack process callback
bool KonfytJackEngine::passMidiMuteSoloActiveFilterAndModify(KonfytJackPort* port, const KonfytMidiEvent* ev, KonfytMidiEvent* evToSend)
{
    // If not muted, pass.
    // If soloFlag is true and port is solo, or soloFlag is false, pass.
    // Finally, pass based on midi filter.
    if (port->active) {
        if (port->mute == false) {
            if ( ( soloFlag && ( port->solo ) ) || (soloFlag==false) ) {
                if ( port->filter.passFilter(ev) ) {
                    // Modify based on filter
                    *evToSend = port->filter.modify(ev);
                    return true;
                }
            }
        }
    }
    return false;
}

bool KonfytJackEngine::InitJackClient(QString name)
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

// Get a string list of jack ports, based on type pattern (e.g. "midi" or "audio", etc.)
// and flags (e.g. JackPortIsInput or JackPortIsOutput).
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


void KonfytJackEngine::setPortActive(int portId, bool active)
{
    if (!clientIsActive()) { return; }

    KonfytJackPort* port = portIdMap.value(portId, NULL);
    if (port == NULL) {
        error_abort("Invalid port " + n2s(portId));
        return;
    }

    port->active = active;
}

/* Set all audio in or midi out ports active based on specified bool. */
void KonfytJackEngine::setAllPortsActive(KonfytJackPortType type, bool active)
{
    if (!clientIsActive()) { return; }

    QList<KonfytJackPort*> *l;

    switch (type) {
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        break;
    case KonfytJackPortType_AudioIn:
        l = &audio_in_ports;
        break;
    default:
        error_abort( "Invalid Jack Port Type." );
    }

    for (int i=0; i<l->count(); i++) {
        l->at(i)->active = active;
    }
}


void KonfytJackEngine::setAllSoundfontPortsActive(bool active)
{
    if (!clientIsActive()) { return; }

    for (int i=0; i<soundfont_ports.count(); i++) {
        soundfont_ports[i]->midi->active = active;
        soundfont_ports[i]->audio_in_l->active = active;
        soundfont_ports[i]->audio_in_r->active = active;
    }
}

void KonfytJackEngine::setAllPluginPortsActive(bool active)
{
    if (!clientIsActive()) { return; }

    for (int i=0; i<plugin_ports.count(); i++) {
        plugin_ports[i]->midi->active = active;
        plugin_ports[i]->audio_in_l->active = active;
        plugin_ports[i]->audio_in_r->active = active;
    }
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


/* Activate all the necessary Jack ports for the given patch.
 * Project is required for some of the patch ports that refer to ports set up in the project. */
void KonfytJackEngine::activatePortsForPatch(const konfytPatch* patch, const KonfytProject* project)
{
    if (!clientIsActive()) { return; }

    // Midi output ports to external apps

    setAllPortsActive(KonfytJackPortType_MidiOut, false);

    QList<int> l = patch->getMidiOutputPortList_ids(); // Get list of midi output ports from patch

    // For each port in list, set active.
    for (int i=0; i<l.count(); i++) {
        int id = l[i];
        if (project->midiOutPort_exists(id)) {
            // Using port id in patch, get project port
            PrjMidiPort projectPort = project->midiOutPort_getPort(id);
            // The project port contains the reference to the Jack port, which we can now use.
            setPortActive(projectPort.jackPortId, true);
        } else {
            // We choose to ignore this gracefully. The user should have been notified in the GUI
            // that the midi out port layer is not valid.
        }
    }


    // Midi output and audio input ports for plugins

    setAllPluginPortsActive(false);

    // For each plugin in the patch, activate midi and audio ports for that plugin
    QList<KonfytPatchLayer> pluginLayers = patch->getPluginLayerList();
    for (int i=0; i<pluginLayers.count(); i++) {
        if (pluginLayers[i].hasError()) { continue; }
        LayerCarlaPluginStruct plugin = pluginLayers.at(i).carlaPluginData;
        if ( pluginsPortsMap.contains( plugin.portsIdInJackEngine ) ) {
            KonfytJackPluginPorts* p = pluginsPortsMap.value(plugin.portsIdInJackEngine);
            p->audio_in_l->active = true;
            p->audio_in_r->active = true;
            p->midi->active = true;
        } else {
            error_abort("activatePortsForPatch: Plugin id " + n2s(plugin.portsIdInJackEngine) +
                        " not in pluginsPortsMap.");
        }
    }

    // Soundfont ports

    setAllSoundfontPortsActive(false);

    QList<KonfytPatchLayer> sflayers = patch->getSfLayerList();
    for (int i=0; i<sflayers.count(); i++) {
        if (sflayers[i].hasError()) { continue; }
        LayerSoundfontStruct sf = sflayers[i].sfData;
        if ( soundfontPortsMap.contains( sf.idInJackEngine ) ) {
            KonfytJackPluginPorts* p = soundfontPortsMap.value(sf.idInJackEngine);
            p->audio_in_l->active = true;
            p->audio_in_r->active = true;
            p->midi->active = true;
        } else {
            error_abort("activatePortsForPatch: Soundfont id " + n2s(sf.idInJackEngine) +
                        " not in soundfontPortsMap.");
        }
    }

    // General audio input ports

    setAllPortsActive(KonfytJackPortType_AudioIn, false);
    QList<int> audioInIds = patch->getAudioInPortList_ids();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        if (project->audioInPort_exists(id)) {
            PrjAudioInPort portPair = project->audioInPort_getPort(id);
            setPortActive(portPair.leftJackPortId, true);
            setPortActive(portPair.rightJackPortId, true);
        } else {
            // Gracefully ignore this. User should have been notified by the GUI of the
            // invalid port layer.
        }
    }
}

void KonfytJackEngine::setGlobalTranspose(int transpose)
{
    this->globalTranspose = transpose;
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

