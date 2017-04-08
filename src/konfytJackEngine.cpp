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


konfytJackEngine::konfytJackEngine(QObject *parent) :
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
    globalTranspose = 0;
}

// Set panicCmd. The Jack process callback will behave accordingly.
void konfytJackEngine::panic(bool p)
{
    panicCmd = p;
}

void konfytJackEngine::setOurMidiInputPortName(QString newName)
{
    ourMidiInputPortName = newName;
}

void konfytJackEngine::timerEvent(QTimerEvent *event)
{
    timer_busy = true;
    if (timer_disabled) {
        timer_busy = false;
        return;
    }

    if (event->timerId() == this->timer.timerId() ) {
        if (registerCallback) {
            JackPortsChanged();
        }
        if (connectCallback || registerCallback) {
            connectCallback = false;
            registerCallback = false;
            refreshPortConnections();
        }
    }

    timer_busy = false;
}

void konfytJackEngine::addPortToAutoConnectList(QString port)
{
    if (midiConnectList.contains(port) == false) {
        midiConnectList.append(port);
        refreshPortConnections();
    }
}

void konfytJackEngine::addAutoConnectList(QStringList ports)
{
    if (!clientIsActive()) { return; }

    this->midiConnectList.append(ports);
    refreshPortConnections();
}

void konfytJackEngine::clearAutoConnectList()
{
    this->midiConnectList.clear();
}

void konfytJackEngine::clearAutoConnectList_andDisconnect()
{
    if (!clientIsActive()) { return; }

    timer.stop();
    for (int i=0; i<midiConnectList.count(); i++) {
        jack_disconnect(client, midiConnectList.at(i).toLocal8Bit(), ourMidiInputPortName.toLocal8Bit());
    }
    clearAutoConnectList();
    startPortTimer();
}

// TODO: MAYBE MERGE WITH removePortClient_andDisconnect FUNCTION
void konfytJackEngine::removePortFromAutoConnectList_andDisconnect(QString port)
{
    if (!clientIsActive()) { return; }

    timer.stop();
    if (this->midiConnectList.contains(port)) {
        // Disconnect
        jack_disconnect(client, port.toLocal8Bit(), ourMidiInputPortName.toLocal8Bit());
        midiConnectList.removeOne(port);
    }
    startPortTimer();
}

void konfytJackEngine::startPortTimer()
{
    this->timer.start(100, this);
}

void konfytJackEngine::refreshPortConnections()
{
    if (!clientIsActive()) { return; }

    // Refresh connections to our midi input port
    for (int i=0; i<midiConnectList.count(); i++) {
        QString port = midiConnectList.at(i);

        // Get our port
        jack_port_t* p = jack_port_by_name(client, ourMidiInputPortName.toLocal8Bit());
        if (p == NULL) { return; }

        if (!jack_port_connected_to( p, port.toLocal8Bit() ) ) {
            jack_connect(client, port.toLocal8Bit(), ourMidiInputPortName.toLocal8Bit());
        }
    }

    // For each midi output port to external apps, refresh connections to all their clients
    for (int i=0; i<midi_out_ports.count(); i++) {

        konfytJackPort* port = midi_out_ports.at(i);
        const char* portname = jack_port_name( port->jack_pointer );

        for (int j=0; j<port->connectionList.count(); j++) {
            QString clientname = port->connectionList.at(j);

            if (!jack_port_connected_to( port->jack_pointer, clientname.toLocal8Bit())) {
                int success = jack_connect(client, portname, clientname.toLocal8Bit()); // NB order of ports important; First source, then dest.
            }
        }
    }

    // Refresh connections for plugin midi and audio ports
    for (int i=0; i<plugin_ports.count(); i++) {

        // Midi out
        konfytJackPort* m = plugin_ports.at(i)->midi_out;
        const char* m_src = jack_port_name( m->jack_pointer );
        for (int j=0; j<m->connectionList.count(); j++) {

            const char* m_dest = m->connectionList.at(j).toLocal8Bit();
            jack_connect(client, m_src, m_dest);
        }

        // Audio in left
        konfytJackPort* al = plugin_ports.at(i)->audio_in_l;
        const char* al_dest = jack_port_name( al->jack_pointer );
        for (int j=0; j<al->connectionList.count(); j++) {

            const char* al_src = al->connectionList.at(j).toLocal8Bit();
            jack_connect(client, al_src, al_dest);
        }

        // Audio in right
        konfytJackPort* ar = plugin_ports.at(i)->audio_in_r;
        const char* ar_dest = jack_port_name( ar->jack_pointer );
        for (int j=0; j<ar->connectionList.count(); j++) {

            const char* ar_src = ar->connectionList.at(j).toLocal8Bit();
            jack_connect(client, ar_src, ar_dest);
        }

    }

    // Refresh connections for audio input ports
    for (int i=0; i<audio_in_ports.count(); i++) {

        konfytJackPort* p = audio_in_ports[i];
        const char* dest = jack_port_name(p->jack_pointer);
        for (int j=0; j<p->connectionList.count(); j++) {
            const char* src = p->connectionList[j].toLocal8Bit();
            jack_connect(client, src, dest);
        }
    }

    // Refresh connections for audio output ports (aka busses)
    for (int i=0; i<audio_out_ports.count(); i++) {

        konfytJackPort* p = audio_out_ports[i];
        const char* src = jack_port_name(p->jack_pointer);
        for (int j=0; j<p->connectionList.count(); j++) {
            const char* dest = p->connectionList[j].toLocal8Bit();
            jack_connect(client, src, dest);
        }

    }

}

// Add new soundfont ports. Also assigns midi filter.
void konfytJackEngine::addSoundfont(layerSoundfontStruct sf)
{
    /* Soundfonts use the same structures as Carla plugins for now, but are much simpler
     * as midi is given to the fluidsynth engine and audio is recieved from it without
     * external Jack connections. */

    konfytJackPluginPorts* p = new konfytJackPluginPorts();
    p->audio_in_l = new konfytJackPort();
    p->audio_in_r = new konfytJackPort();

    // Because our audio is not received from jack audio ports, we have to allocate
    // the buffers now so fluidsynth can write to them in the jack process callback.
    p->audio_in_l->buffer = malloc(sizeof(jack_default_audio_sample_t)*nframes);
    p->audio_in_r->buffer = malloc(sizeof(jack_default_audio_sample_t)*nframes);

    p->midi_out = new konfytJackPort();
    p->midi_out->filter = sf.filter;
    p->plugin_id = sf.indexInEngine;

    soundfont_ports.append(p);
    soundfontPortsMap.insert(sf.indexInEngine, p);
}

void konfytJackEngine::removeSoundfont(int indexInEngine)
{
    pauseJackProcessing(true);

    // Delete all objects created in addSoundfont()

    konfytJackPluginPorts* p = soundfontPortsMap.value(indexInEngine);
    soundfontPortsMap.remove(p);

    // Remove all recorded noteon, sustain and pitchbend events related to this Fluidsynth ID.
    for (int i=0; i<noteOnList.count(); i++) {
        konfytJackNoteOnRecord *rec = noteOnList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == indexInEngine) {
                noteOnList.remove(i);
                i--; // Due to removal, have to stay at same index after for loop i++
            }
        }
    }
    for (int i=0; i<sustainList.count(); i++) {
        konfytJackNoteOnRecord *rec = sustainList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == indexInEngine) {
                sustainList.remove(i);
                i--;
            }
        }
    }
    for (int i=0; i<pitchBendList.count(); i++) {
        konfytJackNoteOnRecord *rec = pitchBendList.at_ptr(i);
        if (rec->jackPortNotFluidsynth == false) {
            if (rec->fluidsynthID == indexInEngine) {
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
    delete p->midi_out;
    delete p;

    pauseJackProcessing(false);
}


/* For the given Carla plugin, add a midi output port,
 * add audio left and right input ports, add the plugin ports to
 * the newly created ports auto-connect lists and combine the
 * newly created midi and audio ports into a plugin port struct,
 * add it to the plugin_ports list and the pluginPortsMap.
 * Also assigns midi filter. */
void konfytJackEngine::addPluginPortsAndConnect(layerCarlaPluginStruct plugin)
{
    QString pname = plugin.name;
    konfytJackPort* midiPort = new konfytJackPort();
    konfytJackPort* alPort = new konfytJackPort();
    konfytJackPort* arPort = new konfytJackPort();

    // Add a new midi output port which will be connected to the plugin midi input
    jack_port_t* newPortPointer = jack_port_register(client, pname.toLocal8Bit(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    if (newPortPointer == NULL) {
        userMessage("Failed to create Jack MIDI output port for plugin.");
        error_abort("Failed to create Jack MIDI output port for plugin " + pname); // TODO: handle this more gracefully
    } else {
        midiPort->active = false;
        midiPort->connectionList.append( plugin.midi_in_port );
        midiPort->jack_pointer = newPortPointer;
        midiPort->mute = false;
        midiPort->prev_active = false;
        midiPort->solo = false;
        midiPort->filter = plugin.midiFilter;
    }



    // Add left audio input port where we will receive plugin audio
    QString nameL = plugin.name + "_in_L";
    jack_port_t* newL = jack_port_register(client, nameL.toLocal8Bit(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (newL == NULL) {
        userMessage("Failed to create left audio input port.");
        error_abort("Failed to create left Jack audio input port for plugin, " + nameL); // TODO: handle this more gracefully
    } else {
        alPort->active = false;
        alPort->connectionList.append( plugin.audio_out_port_left );
        alPort->destinationPort = NULL;
        alPort->gain = 1;
        alPort->jack_pointer = newL;
        alPort->mute = false;
        alPort->prev_active = false;
        alPort->solo = false;
    }

    // Add right audio input port
    QString nameR = plugin.name + "_in_R";
    jack_port_t* newR = jack_port_register(client, nameR.toLocal8Bit(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (newR < 0) {
        userMessage("Failed to create right audio input port.");
        error_abort("Failed to create right Jack audio input port for plugin, " + nameR); // TODO: handle this more gracefully
    } else {
        arPort->active = false;
        arPort->connectionList.append( plugin.audio_out_port_right );
        arPort->destinationPort = NULL;
        arPort->gain = 1;
        arPort->jack_pointer = newR;
        arPort->mute = false;
        arPort->prev_active = false;
        arPort->solo = false;
    }

    konfytJackPluginPorts* p = new konfytJackPluginPorts();
    p->midi_out = midiPort;
    p->audio_in_l = alPort;
    p->audio_in_r = arPort;
    p->plugin_id = plugin.indexInEngine;

    plugin_ports.append(p);
    pluginsPortsMap.insert(plugin.indexInEngine, p);

}

void konfytJackEngine::removePlugin(int indexInEngine)
{
    pauseJackProcessing(true);

    // Remove everything created in addPluginPortsAndConnect()

    konfytJackPluginPorts* p = pluginsPortsMap.value(indexInEngine);
    pluginsPortsMap.remove(p);

    // Remove all recorded noteon, sustain and pitchbend events related to this midi out port
    for (int i=0; i<noteOnList.count(); i++) {
        konfytJackNoteOnRecord *rec = noteOnList.at_ptr(i);
        if (rec->jackPortNotFluidsynth) {
            if (rec->port == p->midi_out) {
                noteOnList.remove(i);
                i--; // Due to removal, have to stay at same index after for loop i++
            }
        }
    }
    for (int i=0; i<sustainList.count(); i++) {
        konfytJackNoteOnRecord *rec = sustainList.at_ptr(i);
        if (rec->jackPortNotFluidsynth) {
            if (rec->port == p->midi_out) {
                sustainList.remove(i);
                i--;
            }
        }
    }
    for (int i=0; i<pitchBendList.count(); i++) {
        konfytJackNoteOnRecord *rec = pitchBendList.at_ptr(i);
        if (rec->jackPortNotFluidsynth) {
            if (rec->port == p->midi_out) {
                pitchBendList.remove(i);
                i--;
            }
        }
    }

    plugin_ports.removeAll(p);

    jack_port_unregister(client, p->midi_out->jack_pointer);
    delete p->midi_out;

    jack_port_unregister(client, p->audio_in_l->jack_pointer);
    delete p->audio_in_l;

    jack_port_unregister(client, p->audio_in_r->jack_pointer);
    delete p->audio_in_r;

    delete p;

    pauseJackProcessing(false);
}

void konfytJackEngine::setSoundfontMidiFilter(int indexInEngine, konfytMidiFilter filter)
{
    if (soundfontPortsMap.contains(indexInEngine)) {
        soundfontPortsMap.value(indexInEngine)->midi_out->filter = filter;
    } else {
        error_abort("setSoundfontMidiFilter: indexInEngine out of range.");
    }
}

void konfytJackEngine::setPluginMidiFilter(int indexInEngine, konfytMidiFilter filter)
{
    if (pluginsPortsMap.contains(indexInEngine)) {
        // Map plugin's indexInEngine to port number
        pluginsPortsMap.value(indexInEngine)->midi_out->filter = filter;
    } else {
        error_abort("setPluginMidiFilter: indexInEngine out of range.");
    }
}

void konfytJackEngine::setSoundfontSolo(int indexInEngine, bool solo)
{
    if (soundfontPortsMap.contains(indexInEngine)) {
        soundfontPortsMap.value(indexInEngine)->audio_in_l->solo = solo;
        soundfontPortsMap.value(indexInEngine)->audio_in_r->solo = solo;
        soundfontPortsMap.value(indexInEngine)->midi_out->solo = solo;
        refreshSoloFlag();
    } else {
        error_abort("setSoundfontSolo: indexInEngine out of range.");
    }
}

void konfytJackEngine::setPluginSolo(int indexInEngine, bool solo)
{
    if (pluginsPortsMap.contains(indexInEngine)) {
        // Map plugin's indexInEngine to port number
        pluginsPortsMap.value(indexInEngine)->audio_in_l->solo = solo;
        pluginsPortsMap.value(indexInEngine)->audio_in_r->solo = solo;
        pluginsPortsMap.value(indexInEngine)->midi_out->solo = solo;
        refreshSoloFlag();
    } else {
        error_abort("setPluginSolo: indexInEngine out of range.");
    }
}

void konfytJackEngine::setSoundfontMute(int indexInEngine, bool mute)
{
    if (soundfontPortsMap.contains(indexInEngine)) {
        soundfontPortsMap.value(indexInEngine)->audio_in_l->mute = mute;
        soundfontPortsMap.value(indexInEngine)->audio_in_r->mute = mute;
        soundfontPortsMap.value(indexInEngine)->midi_out->mute = mute;
    } else {
        error_abort("setSoundfontMute: indexInEngine out of range.");
    }
}

void konfytJackEngine::setPluginMute(int indexInEngine, bool mute)
{
    if (pluginsPortsMap.contains(indexInEngine)) {
        // Map plugin's indexInEngine to port number
        pluginsPortsMap.value(indexInEngine)->audio_in_l->mute = mute;
        pluginsPortsMap.value(indexInEngine)->audio_in_r->mute = mute;
        pluginsPortsMap.value(indexInEngine)->midi_out->mute = mute;
    } else {
        error_abort("setPluginMute: indexInEngine out of range.");
    }
}

void konfytJackEngine::setSoundfontRouting(int indexInEngine, konfytJackPort *port_left, konfytJackPort *port_right)
{
    pauseJackProcessing(true);

    if (!audio_out_ports.contains(port_left)) {
        error_abort("setSoundfontRouting: invalid port_left.");
    }
    if (!audio_out_ports.contains(port_right)) {
        error_abort("setSoundfontRouting: invalid port_right.");
    }

    if (soundfontPortsMap.contains(indexInEngine)) {
        konfytJackPluginPorts* p = soundfontPortsMap.value(indexInEngine);
        p->audio_in_l->destinationPort = port_left;
        p->audio_in_r->destinationPort = port_right;
    } else {
        error_abort("setSoundfontRouting: indexInEngine out of range.");
    }

    pauseJackProcessing(false);
}

void konfytJackEngine::setPluginRouting(int indexInEngine, konfytJackPort* port_left, konfytJackPort* port_right)
{
    pauseJackProcessing(true);

    if (!audio_out_ports.contains(port_left)) {
        error_abort("setPluginRouting: invalid port_left.");
    }
    if (!audio_out_ports.contains(port_right)) {
        error_abort("setPluginRouting: invalid port_right.");
    }

    if (pluginsPortsMap.contains(indexInEngine)) {
        konfytJackPluginPorts* p = pluginsPortsMap.value(indexInEngine);
        p->audio_in_l->destinationPort = port_left;
        p->audio_in_r->destinationPort = port_right;
    } else {
        error_abort("setPluginRouting: indexInEngine out of range.");
    }

    pauseJackProcessing(false);
}


void konfytJackEngine::refreshSoloFlag()
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
bool konfytJackEngine::getSoloFlagInternal()
{
    // Check if a plugin is solo
    for (int i=0; i<plugin_ports.count(); i++) {
        if (plugin_ports[i]->midi_out->active) {
            if (plugin_ports[i]->midi_out->solo == true) {
                return true;
            }
        }
    }
    // Check if a soundfont is solo
    for (int i=0; i<soundfont_ports.count(); i++) {
        if (soundfont_ports[i]->midi_out->active) {
            if (soundfont_ports[i]->midi_out->solo == true) {
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
void konfytJackEngine::setSoloFlagExternal(bool newSolo)
{
    this->soloFlag_external = newSolo;
    this->refreshSoloFlag();
}

void konfytJackEngine::removeAllAudioInAndOutPorts()
{
    // Disable audio port processing in Jack process callback function,
    // and wait for processing to finish before removing ports.
    pauseJackProcessing(true);

    for (int i=0; i<audio_in_ports.count(); i++) {
        konfytJackPort* p = audio_in_ports[i];
        jack_port_unregister(client, p->jack_pointer);
        delete p;
    }
    audio_in_ports.clear();

    for (int i=0; i<audio_out_ports.count(); i++) {
        konfytJackPort* p = audio_out_ports[i];
        jack_port_unregister(client, p->jack_pointer);
        delete p;
    }
    audio_out_ports.clear();

    // Ensure all the destinationPort member of ports are NULL, since there
    // are now no more output ports (busses) left.
    nullDestinationPorts_all();

    // Enable audio port processing again.
    pauseJackProcessing(false);
}

void konfytJackEngine::removeAllMidiOutPorts()
{
    pauseJackProcessing(true);

    for (int i=0; i<midi_out_ports.count(); i++) {
        konfytJackPort* p = midi_out_ports[i];
        removePort(KonfytJackPortType_MidiOut, p);
    }

    pauseJackProcessing(false);
}

/* Set the destinationPort to NULL for all ports that make use of this
 * (i.e. for audio input ports and plugin audio ports).
 * NULL is used by the Jack process callback to determine that the
 * destination port is not set/valid. */
void konfytJackEngine::nullDestinationPorts_all()
{
    pauseJackProcessing(true);

    // Audio input ports
    for (int i=0; i<audio_in_ports.count(); i++) {
        audio_in_ports[i]->destinationPort = NULL;
    }

    // Plugin audio ports
    for (int i=0; i<plugin_ports.count(); i++) {
        plugin_ports[i]->audio_in_l->destinationPort = NULL;
        plugin_ports[i]->audio_in_r->destinationPort = NULL;
    }

    // Soundfont audio ports
    for (int i=0; i<soundfont_ports.count(); i++) {
        soundfont_ports[i]->audio_in_l->destinationPort = NULL;
        soundfont_ports[i]->audio_in_r->destinationPort = NULL;
    }

    pauseJackProcessing(false);
}

/* Set the destinationPort to NULL for all ports where it points to the specified
 * port. */
void konfytJackEngine::nullDestinationPorts_pointingTo(konfytJackPort *port)
{
    pauseJackProcessing(true);

    // Audio input ports
    for (int i=0; i<audio_in_ports.count(); i++) {
        if (audio_in_ports[i]->destinationPort == port) {
            audio_in_ports[i]->destinationPort = NULL;
        }
    }

    // Plugin audio ports
    for (int i=0; i<plugin_ports.count(); i++) {
        if (plugin_ports[i]->audio_in_l->destinationPort == port) {
            plugin_ports[i]->audio_in_l->destinationPort = NULL;
        }
        if (plugin_ports[i]->audio_in_r->destinationPort == port) {
            plugin_ports[i]->audio_in_r->destinationPort = NULL;
        }
    }

    // Soundfont audio ports
    for (int i=0; i<soundfont_ports.count(); i++) {
        if (soundfont_ports[i]->audio_in_l->destinationPort == port) {
            soundfont_ports[i]->audio_in_l->destinationPort = NULL;
        }
        if (soundfont_ports[i]->audio_in_r->destinationPort == port) {
            soundfont_ports[i]->audio_in_r->destinationPort = NULL;
        }
    }

    pauseJackProcessing(false);
}

void konfytJackEngine::removePort(konfytJackPortType type, konfytJackPort *port)
{
    // Disable Jack process callback and block until it is not executing anymore
    pauseJackProcessing(true);

    QList<konfytJackPort*> *l;
    switch (type) {
    case KonfytJackPortType_AudioIn:
        l = &audio_in_ports;
        break;
    case KonfytJackPortType_AudioOut:
        l = &audio_out_ports;
        break;
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        break;
    default:
        error_abort("removePort: invalid port type");
        break;
    }

    if (l->contains(port)) {
        l->removeAll(port);
        jack_port_unregister(client, port->jack_pointer);
        delete port;

        // If this is a midi out port, remove all recorded noteon, sustain and pitchbend
        // events related to this port.
        if (type == KonfytJackPortType_MidiOut) {
            for (int i=0; i<noteOnList.count(); i++) {
                konfytJackNoteOnRecord *rec = noteOnList.at_ptr(i);
                if (rec->jackPortNotFluidsynth) {
                    if (rec->port == port) {
                        noteOnList.remove(i);
                        i--; // Due to removal, have to stay at same index after for loop i++
                    }
                }
            }
            for (int i=0; i<sustainList.count(); i++) {
                konfytJackNoteOnRecord *rec = sustainList.at_ptr(i);
                if (rec->jackPortNotFluidsynth) {
                    if (rec->port == port) {
                        sustainList.remove(i);
                        i--;
                    }
                }
            }
            for (int i=0; i<pitchBendList.count(); i++) {
                konfytJackNoteOnRecord *rec = pitchBendList.at_ptr(i);
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
        if (type == KonfytJackPortType_AudioOut) {
            nullDestinationPorts_pointingTo(port);
        }

    } else {
        error_abort("removePort: invalid port");
    }

    refreshSoloFlag();

    // Enable Jack process callback again
    pauseJackProcessing(false);
}


/* Dummy function to copy structure from. */
void konfytJackEngine::dummyFunction(konfytJackPortType type)
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


konfytJackPort* konfytJackEngine::addPort(konfytJackPortType type, QString port_name)
{
    if (!clientIsActive()) { return NULL; }

    pauseJackProcessing(true);

    jack_port_t* newPort;
    QList<konfytJackPort*> *listToAddTo;
    QString pname;
    konfytJackPort* ret = NULL;

    switch (type) {
    case KonfytJackPortType_MidiOut:

        newPort = jack_port_register (client, port_name.toLocal8Bit(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
        listToAddTo = &midi_out_ports;

        break;
    case KonfytJackPortType_AudioOut:

        pname = port_name;
        newPort = jack_port_register (client, pname.toLocal8Bit(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        listToAddTo = &audio_out_ports;

        break;
    case KonfytJackPortType_AudioIn:

        pname = port_name;
        newPort = jack_port_register (client, pname.toLocal8Bit(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        listToAddTo = &audio_in_ports;

        break;
    }

    if (newPort == NULL) {
        error_abort("konfytJackEngine::addPort: could not add jack port. Have to handle this better!");
        ret = NULL;
    } else {

        konfytJackPort* p = new konfytJackPort();
        p->jack_pointer = newPort;
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
        p->destinationPort = NULL;

        listToAddTo->append(p);

        // Return pointer to port, which acts as the port's unique ID
        ret = p;
    }

    pauseJackProcessing(false);
    return ret;
}




void konfytJackEngine::setPortClients(konfytJackPortType type, konfytJackPort *port, QStringList newClientList)
{
    for (int i=0; i<newClientList.count(); i++) {
        this->addPortClient( type, port, newClientList[i] );
    }
}

void konfytJackEngine::clearPortClients(konfytJackPortType type, konfytJackPort *port)
{
    QList<konfytJackPort*> *l;

    switch (type) {
    case KonfytJackPortType_MidiIn:
        error_abort( "clearPortClients: Not yet implemented." );
        break;
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        break;
    case KonfytJackPortType_AudioIn:
        l = &audio_in_ports;
        break;
    case KonfytJackPortType_AudioOut:
        l = &audio_out_ports;
        break;
    default:
        error_abort( "clearPortClients: Unknown Jack Port Type." );
    }

    if (l->contains(port)) {
        port->connectionList.clear();
    } else {
        error_abort("clearPortClients: invalid port");
    }

}

void konfytJackEngine::clearPortClients_andDisconnect(konfytJackPortType type, konfytJackPort *port)
{
    QList<konfytJackPort*> *l;

    switch (type) {
    case KonfytJackPortType_MidiIn:
        error_abort("not implemented yet");
        break;
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        break;
    case KonfytJackPortType_AudioIn:
        l = &audio_in_ports;
        break;
    case KonfytJackPortType_AudioOut:
        l = &audio_out_ports;
        break;
    default:
        error_abort( "Unknown Jack Port Type." );
    }

    if (l->contains(port)) {
        while (port->connectionList.count()) {
            removePortClient_andDisconnect(type, port, port->connectionList[0]);
        }
    } else {
        error_abort("Invalid port.");
    }

}

void konfytJackEngine::addPortClient(konfytJackPortType type, konfytJackPort* port, QString newClient)
{
    if (!clientIsActive()) { return; }

    QList<konfytJackPort*> *l;

    switch (type) {
    case KonfytJackPortType_MidiIn:
        error_abort("not implemented yet");
        break;
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        break;
    case KonfytJackPortType_AudioIn:
        l = &audio_in_ports;
        break;
    case KonfytJackPortType_AudioOut:
        l = &audio_out_ports;
        break;
    default:
        error_abort("addPortClient: invalid Jack Port Type");
    }

    if (l->contains(port)) {
        port->connectionList.append(newClient);
    } else { error_abort("Invalid port." ); }

    refreshPortConnections();
}



/* Remove the given client:port (cname) from the specified port auto-connect list. */
void konfytJackEngine::removePortClient_andDisconnect(konfytJackPortType type, konfytJackPort *port, QString cname)
{
    if (!clientIsActive()) { return; }

    pauseJackProcessing(true);

    QList<konfytJackPort*> *l;
    int order;

    switch (type) {
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        order = 0;
        break;
    case KonfytJackPortType_AudioIn:
        l = &audio_in_ports;
        order = 1;
        break;
    case KonfytJackPortType_AudioOut:
        l = &audio_out_ports;
        order = 0;
        break;
    default:
        error_abort("removePortClient_andDisconnect: invalid Jack Port Type");
        break;
    }

    if (l->contains(port)) {

        if (port->connectionList.contains( cname )) {
            // Disconnect in Jack
            char* ourPort = jack_port_name( port->jack_pointer );
            // NB the order of ports in the jack_disconnect function matters:
            if (order == 0) {
                jack_disconnect(client, ourPort, cname.toLocal8Bit() );
            } else {
                jack_disconnect(client, cname.toLocal8Bit(), ourPort );
            }
            // Remove client from port's list
            port->connectionList.removeAll(cname);
        }
    } else { error_abort("removePortClient_andDisconnect: invalid port."); }

    pauseJackProcessing(false);
    refreshPortConnections();
}

QStringList konfytJackEngine::getPortClients(konfytJackPortType type, konfytJackPort *port)
{
    QList<konfytJackPort*> *l;

    switch (type) {
    case KonfytJackPortType_MidiIn:
        error_abort("not implementd yet");
        break;
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        break;
    case KonfytJackPortType_AudioIn:
        l = &audio_in_ports;
        break;
    case KonfytJackPortType_AudioOut:
        l = &audio_out_ports;
        break;
    default:
        error_abort( "Unknown Jack Port Type." );
    }

    if (l->contains(port)) {
        return port->connectionList;
    } else {
        error_abort("Invalid port.");
    }

}

int konfytJackEngine::getPortCount(konfytJackPortType type)
{
    switch (type) {
    case KonfytJackPortType_MidiIn:
        error_abort("not implemented yet.");
        break;
    case KonfytJackPortType_MidiOut:
        return midi_out_ports.count();
        break;
    case KonfytJackPortType_AudioIn:
        return audio_in_ports.count();
        break;
    case KonfytJackPortType_AudioOut:
        return audio_out_ports.count();
        break;
    default:
        error_abort("getPortCount: invalid JackPortType");
    }
}

void konfytJackEngine::setPortFilter(konfytJackPortType type, konfytJackPort *port, konfytMidiFilter filter)
{
    QList<konfytJackPort*> *l;

    switch (type) {
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        break;
    default:
        error_abort( "Unknown Jack Port Type." );
    }

    if (l->contains(port)) {
        port->filter = filter;
    } else {
        error_abort("Invalid port.");
    }
}

void konfytJackEngine::setPortSolo(konfytJackPortType type, konfytJackPort *port, bool solo)
{
    switch (type) {
    case KonfytJackPortType_MidiOut:
        if (midi_out_ports.contains(port)) {
            port->solo = solo;
        } else {
            // Logic error somewhere else.
            error_abort( "invalid port" );
        }
        break;
    case KonfytJackPortType_AudioIn:
        if (audio_in_ports.contains(port)) {
            port->solo = solo;
        } else {
            // Logic error somewhere else.
            error_abort( "invalid port" );
        }
        break;
    default:
        error_abort("setPortSolo: invalid Jack Port Type");
    }
}

void konfytJackEngine::setPortMute(konfytJackPortType type, konfytJackPort *port, bool mute)
{
    switch (type) {
    case KonfytJackPortType_MidiOut:
        if (midi_out_ports.contains(port)) {
            port->mute = mute;
        } else {
            // Logic error somewhere else.
            error_abort( "invalid port" );
        }
        break;
    case KonfytJackPortType_AudioIn:
        if (audio_in_ports.contains(port)) {
            port->mute = mute;
        } else {
            // Logic error somewhere else.
            error_abort( "invalid port" );
        }
        break;
    default:
        error_abort("setPortMute: invalid Jack Port Type");
    }
}

void konfytJackEngine::setPortGain(konfytJackPortType type, konfytJackPort *port, float gain)
{
    switch (type) {
    case KonfytJackPortType_AudioOut:
        if (audio_out_ports.contains(port)) {
            port->gain = gain;
        } else {
            // Logic error somewhere else.
            error_abort( "invalid port" );
        }
        break;
    case KonfytJackPortType_AudioIn:
        if (audio_in_ports.contains(port)) {
            port->gain = gain;
        } else {
            // Logic error somewhere else.
            error_abort( "invalid port" );
        }
        break;
    default:
        error_abort("setPortGain: invalid Jack Port Type");
    }
}

/* Route the src port to the dest port, i.e. set src destinationPort = dest. This routes audio from
 * the src port to the dest port (bus). */
void konfytJackEngine::setPortRouting(konfytJackPortType type, konfytJackPort *src, konfytJackPort *dest)
{
    pauseJackProcessing(true);

    switch (type) {
    case KonfytJackPortType_AudioIn:
        if (audio_in_ports.contains(src)) {
            if (audio_out_ports.contains(dest)) {
                src->destinationPort = dest;
            } else { error_abort("invalid dest port"); }
        } else { error_abort("invalid src port"); }
        break;
    default:
        error_abort("setPortRouting: invalid Jack Port Type");
    }

    pauseJackProcessing(false);
}

// This indicates whether we are connected to Jack or failed to create/activate a client.
bool konfytJackEngine::clientIsActive()
{
    return this->clientActive;
}

QString konfytJackEngine::clientName()
{
    return ourJackClientName;
}

/* Pauses (pause=true) or unpauses (pause=false) the Jack process callback function.
 * When pause=true, the Jack process callback is disabled and this function blocks
 * until the current execution of the process callback (if any) competes, before
 * returning, thus ensuring that the process callback will not execute until this
 * function is called again with pause=false. */
void konfytJackEngine::pauseJackProcessing(bool pause)
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

void konfytJackEngine::jackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void *arg)
{
    konfytJackEngine* e = (konfytJackEngine*)arg;

    e->connectCallback = true;
}

void konfytJackEngine::jackPortRegistrationCallback(jack_port_id_t port, int registered, void *arg)
{
    konfytJackEngine* e = (konfytJackEngine*)arg;

    e->registerCallback = true;
}

int konfytJackEngine::jackProcessCallback(jack_nframes_t nframes, void *arg)
{
    konfytJackEngine* e = (konfytJackEngine*)arg;

    // Lock jack_process ----------------------
    e->jack_process_busy = true;
    if (e->jack_process_disable) {
        e->jack_process_busy = false;
        //e->userMessage("Jack process callback locked out.");
        return 0;
    }
    // ----------------------------------------

    int n, i, id;
    konfytJackPort* tempPort;
    konfytJackPort* tempPort2;

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
            if (e->passMuteSoloActiveCriteria( tempPort )) {
                tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes );
                e->mixBufferToDestinationPort( tempPort, nframes, true );
            }
        }

        // For each soundfont audio in port, mix to destination bus
        if (e->fluidsynthEngine != NULL) {
            for (n=0; n<e->soundfont_ports.count(); n++) {
                tempPort = e->soundfont_ports[n]->audio_in_l; // Left

                if ( e->passMuteSoloActiveCriteria(tempPort) ) {
                    tempPort2 = e->soundfont_ports[n]->audio_in_r;
                    // We are not getting our audio from Jack audio ports, but from fluidsynth.
                    // The buffers have already been allocated when we added the soundfont layer to the engine.
                    // Get data from fluidsynth

                    /* Same as below, just manually one at a time.*/
                    for (i=0; i<nframes; i++) {
                        e->fluidsynthEngine->fluidsynthWriteFloat( e->soundfont_ports[n]->plugin_id,
                                                                   &( ((jack_default_audio_sample_t*)tempPort->buffer)[i] ),
                                                                   &( ((jack_default_audio_sample_t*)tempPort2->buffer)[i] ), 1 );
                    }
                    /*
                    e->fluidsynthEngine->fluidsynthWriteFloat( e->soundfont_ports[n]->plugin_id,
                                                               ((jack_default_audio_sample_t*)tempPort->buffer),
                                                               ((jack_default_audio_sample_t*)tempPort2->buffer), nframes );
                    */

                    e->mixBufferToDestinationPort( tempPort, nframes, false );
                    e->mixBufferToDestinationPort( tempPort2, nframes, false );
                }
            }
        }


        // For each plugin audio in port, mix to destination bus
        for (n=0; n<e->plugin_ports.count(); n++) {
            tempPort = e->plugin_ports[n]->audio_in_l; // Left
            if ( e->passMuteSoloActiveCriteria( tempPort ) ) {
                // Left
                tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes );
                e->mixBufferToDestinationPort( tempPort, nframes, false );
                // Right
                tempPort = e->plugin_ports[n]->audio_in_r;
                tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes );
                e->mixBufferToDestinationPort( tempPort, nframes, false );
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
        tempPort = e->plugin_ports[n]->midi_out;
        tempPort->buffer = jack_port_get_buffer( tempPort->jack_pointer, nframes );
        jack_midi_clear_buffer( tempPort->buffer );
    }


    if (e->panicState == 1) {
        // We just entered panic state. Send note off messages etc, and proceed to state 2 where we just wait.

        // Send to fluidsynth
        for (n=0; n<e->soundfont_ports.count(); n++) {
            tempPort = e->soundfont_ports.at(n)->midi_out;
            id = e->soundfont_ports.at(n)->plugin_id;
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
            tempPort = e->plugin_ports[n]->midi_out;
            e->sendMidiClosureEvents_chanZeroOnly( tempPort ); // Only on channel zero
        }

        e->panicState = 2; // Proceed to panicState 2 where we will just wait for panic to subside.

    }


    // Get our midi input
    void* input_port_buf = jack_port_get_buffer(e->midi_input_port, nframes);
    jack_nframes_t input_event_count = jack_midi_get_event_count(input_port_buf);

    unsigned char* out_buffer;
    jack_midi_event_t inEvent_jack;
    konfytMidiEvent evToSend;

    // For each midi input event...
    for (i=0; i<input_event_count; i++) {

        // Get input event
        jack_midi_event_get(&inEvent_jack, input_port_buf, i);

        QByteArray inEvent_jack_tempBuffer(inEvent_jack.buffer, (int)inEvent_jack.size);
        konfytMidiEvent ev( inEvent_jack_tempBuffer );

        // Send to GUI
        if (ev.type != MIDI_EVENT_TYPE_SYSTEM) {
            emit e->midiEventSignal( ev );
        }



        bool passEvent = true;
        bool recordNoteon = false;
        bool recordSustain = false;
        bool recordPitchbend = false;


        if (ev.type == MIDI_EVENT_TYPE_NOTEOFF) {
            passEvent = false;
            for (int i=0; i<e->noteOnList.count(); i++) {
                konfytJackNoteOnRecord* rec = e->noteOnList.at_ptr(i);
                // Apply global transpose that was active at time of noteon
                ev.data1 += rec->globalTranspose;
                // Check if event passes noteon filter
                if ( rec->filter.passFilter(&ev) ) {
                    // Apply noteon filter modification
                    konfytMidiEvent newEv = rec->filter.modify( &ev );
                    if (newEv.data1 == rec->note) {
                        // Match! Send noteoff and remove noteon from list.
                        if (rec->jackPortNotFluidsynth) {
                            // Get output buffer, based on size and time of input event
                            out_buffer = jack_midi_event_reserve( rec->port->buffer, inEvent_jack.time, inEvent_jack.size);
                            // Copy input event to output buffer
                            newEv.toBuffer( out_buffer );
                        } else {
                            e->fluidsynthEngine->processJackMidi( rec->fluidsynthID, &newEv );
                        }
                        rec->port->noteOns--;
                        if (rec->relatedPort!=NULL) { rec->relatedPort->noteOns--; }

                        e->noteOnList.remove(i);
                        i--; // Due to removal, have to stay at same index after for loop i++
                    }
                }
                ev.data1 -= rec->globalTranspose;
            }
        } else if ( (ev.type == MIDI_EVENT_TYPE_CC) && (ev.data1 == 64) ) {
            if (ev.data2 == 0) {
                // Sustain zero
                passEvent = false;
                for (int i=0; i<e->sustainList.count(); i++) {
                    konfytJackNoteOnRecord* rec = e->sustainList.at_ptr(i);
                    // Check if event passes filter
                    if ( rec->filter.passFilter(&ev) ) {
                        // Apply filter modification
                        konfytMidiEvent newEv = rec->filter.modify( &ev );
                        // Send sustain zero and remove from list
                        if (rec->jackPortNotFluidsynth) {
                            // Get output buffer, based on size and time of input event
                            out_buffer = jack_midi_event_reserve( rec->port->buffer, inEvent_jack.time, inEvent_jack.size);
                            // Copy input event to output buffer
                            newEv.toBuffer( out_buffer );
                        } else {
                            e->fluidsynthEngine->processJackMidi( rec->fluidsynthID, &newEv );
                        }
                        rec->port->sustainNonZero = false;
                        if (rec->relatedPort!=NULL) { rec->relatedPort->sustainNonZero = false; }

                        e->sustainList.remove(i);
                        i--; // Due to removal, have to stay at same index after for loop i++
                    }
                }
            } else {
                recordSustain = true;
            }
        } else if ( (ev.type == MIDI_EVENT_TYPE_PITCHBEND) ) {
            if (ev.pitchbendValue_signed() == 0) {
                // Pitchbend zero
                passEvent = false;
                for (int i=0; i<e->pitchBendList.count(); i++) {
                    konfytJackNoteOnRecord* rec = e->pitchBendList.at_ptr(i);
                    // Check if event passes filter
                    if ( rec->filter.passFilter(&ev) ) {
                        // Apply filter modification
                        konfytMidiEvent newEv = rec->filter.modify( &ev );
                        // Send pitchbend zero and remove from list
                        if (rec->jackPortNotFluidsynth) {
                            // Get output buffer, based on size and time of input event
                            out_buffer = jack_midi_event_reserve( rec->port->buffer, inEvent_jack.time, inEvent_jack.size);
                            // Copy input event to output buffer
                            newEv.toBuffer( out_buffer );
                        } else {
                            e->fluidsynthEngine->processJackMidi( rec->fluidsynthID, &newEv );
                        }
                        rec->port->pitchbendNonZero = false;
                        if (rec->relatedPort!=NULL) { rec->relatedPort->pitchbendNonZero = false; }

                        e->pitchBendList.remove(i);
                        i--; // Due to removal, have to stay at same index after for loop i++
                    }
                }
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

                // Send to fluidsynth
                for (n=0; n<e->soundfont_ports.count(); n++) {
                    tempPort = e->soundfont_ports.at(n)->midi_out;
                    tempPort2 = e->soundfont_ports.at(n)->audio_in_l; // Related audio port
                    id = e->soundfont_ports.at(n)->plugin_id;
                    if ( e->passMidiMuteSoloActiveFilterAndModify(tempPort, &ev, &evToSend) ) {

                        e->fluidsynthEngine->processJackMidi( id, &evToSend );

                        // Record noteon, sustain or pitchbend for off events later.
                        if (recordNoteon) {
                            konfytJackNoteOnRecord rec;
                            rec.filter = tempPort->filter;
                            rec.fluidsynthID = id;
                            rec.globalTranspose = e->globalTranspose;
                            rec.jackPortNotFluidsynth = false;
                            rec.port = tempPort;
                            rec.relatedPort = tempPort2;
                            rec.note = evToSend.data1;
                            e->noteOnList.add(rec);
                            tempPort->noteOns++;
                            tempPort2->noteOns++;
                        } else if (recordPitchbend) {
                            // If this port hasn't saved a pitchbend yet
                            if ( !tempPort->pitchbendNonZero ) {
                                konfytJackNoteOnRecord rec;
                                rec.filter = tempPort->filter;
                                rec.fluidsynthID = id;
                                rec.jackPortNotFluidsynth = false;
                                rec.port = tempPort;
                                rec.relatedPort = tempPort2;
                                e->pitchBendList.add(rec);
                                tempPort->pitchbendNonZero = true;
                                tempPort2->pitchbendNonZero = true;
                            }
                        } else if (recordSustain) {
                            // If this port hasn't saved a sustain yet
                            if ( !tempPort->sustainNonZero ) {
                                konfytJackNoteOnRecord rec;
                                rec.filter = tempPort->filter;
                                rec.fluidsynthID = id;
                                rec.jackPortNotFluidsynth = false;
                                rec.port = tempPort;
                                rec.relatedPort = tempPort2;
                                e->sustainList.add(rec);
                                tempPort->sustainNonZero = true;
                                tempPort2->sustainNonZero = true;
                            }
                        }

                    } else if ( (tempPort->prev_active) && !(tempPort->active) ) {
                        // Was previously active, but not anymore.
                    }
                    tempPort->prev_active = tempPort->active;
                }

                // Give to all active output ports to external apps
                for (n=0; n<e->midi_out_ports.count(); n++) {
                    tempPort = e->midi_out_ports.at(n);
                    if ( e->passMidiMuteSoloActiveFilterAndModify(tempPort, &ev, &evToSend) ) {

                        // Get output buffer, based on size and time of input event
                        out_buffer = jack_midi_event_reserve( tempPort->buffer, inEvent_jack.time, inEvent_jack.size);

                        // Copy input event to output buffer
                        evToSend.toBuffer( out_buffer );

                        // Record noteon, sustain or pitchbend for off events later.
                        if (recordNoteon) {
                            konfytJackNoteOnRecord rec;
                            rec.filter = tempPort->filter;
                            rec.globalTranspose = e->globalTranspose;
                            rec.jackPortNotFluidsynth = true;
                            rec.note = evToSend.data1;
                            rec.port = tempPort;
                            rec.relatedPort = NULL;
                            e->noteOnList.add(rec);
                            tempPort->noteOns++;
                        } else if (recordPitchbend) {
                            // If this port hasn't saved a pitchbend yet
                            if ( !tempPort->pitchbendNonZero ) {
                                konfytJackNoteOnRecord rec;
                                rec.filter = tempPort->filter;
                                rec.jackPortNotFluidsynth = true;
                                rec.port = tempPort;
                                rec.relatedPort = NULL;
                                e->pitchBendList.add(rec);
                                tempPort->pitchbendNonZero = true;
                            }
                        } else if (recordSustain) {
                            // If this port hasn't saved a pitchbend yet
                            if ( !tempPort->sustainNonZero ) {
                                konfytJackNoteOnRecord rec;
                                rec.filter = tempPort->filter;
                                rec.jackPortNotFluidsynth = true;
                                rec.port = tempPort;
                                rec.relatedPort = NULL;
                                e->sustainList.add(rec);
                                tempPort->sustainNonZero = true;
                            }
                        }

                    } else if ( (tempPort->prev_active) && !(tempPort->active) ) {
                        // Was previously active, but not anymore.
                    }
                    tempPort->prev_active = tempPort->active;
                }

                // Also give to all active plugin ports
                for (n=0; n<e->plugin_ports.count(); n++) {
                    tempPort = e->plugin_ports[n]->midi_out;
                    tempPort2 = e->plugin_ports[n]->audio_in_l; // related audio port
                    if ( e->passMidiMuteSoloActiveFilterAndModify(tempPort, &ev, &evToSend) ) {

                        // Get output buffer, based on size and time of input event
                        out_buffer = jack_midi_event_reserve( tempPort->buffer, inEvent_jack.time, inEvent_jack.size);

                        // Force MIDI channel 0
                        evToSend.channel = 0;

                        // Copy input event to output buffer
                        evToSend.toBuffer( out_buffer );

                        // Record noteon, sustain or pitchbend for off events later.
                        if (recordNoteon) {
                            konfytJackNoteOnRecord rec;
                            rec.filter = tempPort->filter;
                            rec.globalTranspose = e->globalTranspose;
                            rec.jackPortNotFluidsynth = true;
                            rec.note = evToSend.data1;
                            rec.port = tempPort;
                            rec.relatedPort = tempPort2;
                            e->noteOnList.add(rec);
                            tempPort->noteOns++;
                            tempPort2->noteOns++;
                        } else if (recordPitchbend) {
                            // If this port hasn't saved a pitchbend yet
                            if ( !tempPort->pitchbendNonZero ) {
                                konfytJackNoteOnRecord rec;
                                rec.filter = tempPort->filter;
                                rec.jackPortNotFluidsynth = true;
                                rec.port = tempPort;
                                rec.relatedPort = tempPort2;
                                e->pitchBendList.add(rec);
                                tempPort->pitchbendNonZero = true;
                                tempPort2->pitchbendNonZero = true;
                            }
                        } else if (recordSustain) {
                            // If this port hasn't saved a pitchbend yet
                            if ( !tempPort->sustainNonZero ) {
                                konfytJackNoteOnRecord rec;
                                rec.filter = tempPort->filter;
                                rec.jackPortNotFluidsynth = true;
                                rec.port = tempPort;
                                rec.relatedPort = tempPort2;
                                e->sustainList.add(rec);
                                tempPort->sustainNonZero = true;
                                tempPort2->sustainNonZero = true;
                            }
                        }

                    } else if ( (tempPort->prev_active) && !(tempPort->active) ) {
                        // Was previoiusly active, but not anymore.
                    }
                    tempPort->prev_active = tempPort->active;
                }

            } // end if passEvent

        } // end if panicState == 0

    }


    // Unlock jack_process --------------------
    e->jack_process_busy = false;
    // ----------------------------------------

    return 0;

} // end of jackProcessCallback


int konfytJackEngine::jackXrunCallback(void *arg)
{   
    konfytJackEngine* e = (konfytJackEngine*)arg;
    e->xrunSignal();
    return 0;
}



// Helper function for Jack process callback
bool konfytJackEngine::passMuteSoloActiveCriteria(konfytJackPort* port)
{
    if ( (port->active) || (port->noteOns) || (port->sustainNonZero) || (port->pitchbendNonZero) ) {
        if (port->mute == false) {
            if ( ( soloFlag && port->solo ) || (soloFlag==false) ) {
                if (port->destinationPort != NULL) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Helper function for Jack process callback
void konfytJackEngine::mixBufferToDestinationPort(konfytJackPort* port, jack_nframes_t nframes, bool applyGain)
{
    float gain = 1;
    if (applyGain) {
        gain = port->gain;
    }

    // For each frame: bus_buffer[frame] += port_buffer[frame]
    for (int i=0; i<nframes; i++) {
        ( (jack_default_audio_sample_t*)(port->destinationPort->buffer) )[i] +=
                ((jack_default_audio_sample_t*)(port->buffer))[i] * gain;
    }
}

// Initialises MIDI closure events that will be sent to ports during Jack process callback.
void konfytJackEngine::initMidiClosureEvents()
{
    evAllNotesOff = konfytMidiEvent();
    evAllNotesOff.type = MIDI_EVENT_TYPE_CC;
    evAllNotesOff.data1 = MIDI_MSG_ALL_NOTES_OFF;
    evAllNotesOff.data2 = 0;

    evSustainZero = konfytMidiEvent();
    evSustainZero.type = MIDI_EVENT_TYPE_CC;
    evSustainZero.data1 = 64;
    evSustainZero.data2 = 0;

    evPitchbendZero = konfytMidiEvent();
    evPitchbendZero.type = MIDI_EVENT_TYPE_PITCHBEND;
    evPitchbendZero.setPitchbendData(0);
}

// Helper function for Jack process callback
void konfytJackEngine::sendMidiClosureEvents(konfytJackPort *port, int channel)
{
    unsigned char* out_buffer;

    // All notes off
    out_buffer = jack_midi_event_reserve( port->buffer, 0, 3);
    evAllNotesOff.channel = channel;
    evAllNotesOff.toBuffer(out_buffer);
    // Also send sustain off message
    out_buffer = jack_midi_event_reserve( port->buffer, 0, 3 );
    evSustainZero.channel = channel;
    evSustainZero.toBuffer(out_buffer);
    // And also pitchbend zero
    out_buffer = jack_midi_event_reserve( port->buffer, 0, 3 );
    evPitchbendZero.channel = channel;
    evPitchbendZero.toBuffer(out_buffer);
}

// Helper function for Jack process callback
void konfytJackEngine::sendMidiClosureEvents_chanZeroOnly(konfytJackPort *port)
{
    sendMidiClosureEvents(port, 0);
}

// Helper function for Jack process callback
void konfytJackEngine::sendMidiClosureEvents_allChannels(konfytJackPort *port)
{
    for (int i=0; i<15; i++) {
        sendMidiClosureEvents(port, i);
    }
}

// Helper function for Jack process callback
bool konfytJackEngine::passMidiMuteSoloActiveFilterAndModify(konfytJackPort* port, const konfytMidiEvent* ev, konfytMidiEvent* evToSend)
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

bool konfytJackEngine::InitJackClient(QString name)
{
    // Try to become a client of the jack server
    if ( (client = jack_client_open(name.toLocal8Bit(), 0, NULL)) == NULL) {
        userMessage("JACK: Error becoming client.");
        this->clientActive = false;
        return false;
    } else {
        ourJackClientName = jack_get_client_name(client); // jack_client_open modifies the given name if another client already uses it.
        userMessage("JACK: Client created: " + ourJackClientName);
        this->clientActive = true;
    }


    // Set up callback functions
    jack_set_port_connect_callback(client, konfytJackEngine::jackPortConnectCallback, this);
    jack_set_port_registration_callback(client, konfytJackEngine::jackPortRegistrationCallback, this);
    jack_set_process_callback (client, konfytJackEngine::jackProcessCallback, this);
    jack_set_xrun_callback(client, konfytJackEngine::jackXrunCallback, this);

    // Set up midi input port
    setOurMidiInputPortName( ourJackClientName + ":" +
                            QString::fromLocal8Bit(KONFYT_JACK_MIDI_IN_PORT_NAME));
    midi_input_port = jack_port_register ( client,
                                           KONFYT_JACK_MIDI_IN_PORT_NAME,
                                           JACK_DEFAULT_MIDI_TYPE,
                                           JackPortIsInput, 0);

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

    startPortTimer(); // Timer that will take care of automatically restoring port connections

    return true;
}

void konfytJackEngine::stopJackClient()
{
    if (clientIsActive()) {
        jack_client_close(client);
        this->clientActive = false;
    }
}

// Get a string list of jack ports, based on type pattern (e.g. "midi" or "audio", etc.)
// and flags (e.g. JackPortIsInput or JackPortIsOutput).
QStringList konfytJackEngine::getPortsList(QString typePattern, unsigned long flags)
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
QStringList konfytJackEngine::getMidiInputPortsList()
{
    return getPortsList("midi", JackPortIsInput);
}

/* Returns list of JACK midi output ports from the JACK server. */
QStringList konfytJackEngine::getMidiOutputPortsList()
{
    return getPortsList("midi", JackPortIsOutput);
}

/* Returns list of JACK audio input ports from the JACK server. */
QStringList konfytJackEngine::getAudioInputPortsList()
{
    return getPortsList("audio", JackPortIsInput);
}

/* Returns list of JACK audio output ports from the JACK server. */
QStringList konfytJackEngine::getAudioOutputPortsList()
{
    return getPortsList("audio", JackPortIsOutput);
}

double konfytJackEngine::getSampleRate()
{
    return this->samplerate;
}


void konfytJackEngine::setPortActive(konfytJackPortType type, konfytJackPort* port, bool active)
{
    if (!clientIsActive()) { return; }

    QList<konfytJackPort*> *l;

    switch (type) {
    case KonfytJackPortType_MidiIn:
        error_abort("Not implemented yet.");
        break;
    case KonfytJackPortType_MidiOut:
        l = &midi_out_ports;
        break;
    case KonfytJackPortType_AudioIn:
        l = &audio_in_ports;
        break;
    case KonfytJackPortType_AudioOut:
        error_abort("Not implemented for this type.");
        break;
    default:
        error_abort( "Unknown Jack Port Type." );
    }

    if (l->contains(port)) {
        port->active = active;
    } else {
        error_abort("Invalid port.");
    }
}

/* Set all audio in or midi out ports active based on specified bool. */
void konfytJackEngine::setAllPortsActive(konfytJackPortType type, bool active)
{
    if (!clientIsActive()) { return; }

    QList<konfytJackPort*> *l;

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


void konfytJackEngine::setAllSoundfontPortsActive(bool active)
{
    if (!clientIsActive()) { return; }

    for (int i=0; i<soundfont_ports.count(); i++) {
        soundfont_ports[i]->midi_out->active = active;
        soundfont_ports[i]->audio_in_l->active = active;
        soundfont_ports[i]->audio_in_r->active = active;
    }
}

void konfytJackEngine::setAllPluginPortsActive(bool active)
{
    if (!clientIsActive()) { return; }

    for (int i=0; i<plugin_ports.count(); i++) {
        plugin_ports[i]->midi_out->active = active;
        plugin_ports[i]->audio_in_l->active = active;
        plugin_ports[i]->audio_in_r->active = active;
    }
}

/* Activate all the necessary Jack ports for the given patch.
 * Project is required for some of the patch ports that refer to ports set up in the project. */
void konfytJackEngine::activatePortsForPatch(const konfytPatch* patch, const konfytProject* project)
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
            prjMidiOutPort projectPort = project->midiOutPort_getPort(id);
            // The project port contains the reference to the Jack port, which we can now use.
            setPortActive(KonfytJackPortType_MidiOut, projectPort.jackPort, true);
        } else {
            // We choose to ignore this gracefully. The user should have been notified in the GUI
            // that the midi out port layer is not valid.
        }
    }


    // Midi output and audio input ports for plugins

    setAllPluginPortsActive(false);

    // For each plugin in the patch, activate midi and audio ports for that plugin
    QList<konfytPatchLayer> pluginLayers = patch->getPluginLayerList();
    for (int i=0; i<pluginLayers.count(); i++) {
        if (pluginLayers[i].hasError()) { continue; }
        layerCarlaPluginStruct plugin = pluginLayers.at(i).carlaPluginData;
        if ( pluginsPortsMap.contains( plugin.indexInEngine ) ) {
            konfytJackPluginPorts* p = pluginsPortsMap.value(plugin.indexInEngine);
            p->audio_in_l->active = true;
            p->audio_in_r->active = true;
            p->midi_out->active = true;
        } else {
            error_abort("activatePortsForPatch: Plugin indexInEngine " + n2s(plugin.indexInEngine) +
                        " not in pluginsPortsMap.");
        }
    }

    // Soundfont ports

    setAllSoundfontPortsActive(false);

    QList<konfytPatchLayer> sflayers = patch->getSfLayerList();
    for (int i=0; i<sflayers.count(); i++) {
        if (sflayers[i].hasError()) { continue; }
        layerSoundfontStruct sf = sflayers[i].sfData;
        if ( soundfontPortsMap.contains( sf.indexInEngine ) ) {
            konfytJackPluginPorts* p = soundfontPortsMap.value(sf.indexInEngine);
            p->audio_in_l->active = true;
            p->audio_in_r->active = true;
            p->midi_out->active = true;
        } else {
            error_abort("activatePortsForPatch: Plugin indexInEngine " + n2s(sf.indexInEngine) +
                        " not in soundfontPortsMap.");
        }
    }

    // General audio input ports

    setAllPortsActive(KonfytJackPortType_AudioIn, false);
    QList<int> audioInIds = patch->getAudioInPortList_ids();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        if (project->audioInPort_exists(id)) {
            prjAudioInPort portPair = project->audioInPort_getPort(id);
            setPortActive(KonfytJackPortType_AudioIn, portPair.leftJackPort, true);
            setPortActive(KonfytJackPortType_AudioIn, portPair.rightJackPort, true);
        } else {
            // Gracefully ignore this. User should have been notified by the GUI of the
            // invalid port layer.
        }
    }
}

void konfytJackEngine::setGlobalTranspose(int transpose)
{
    this->globalTranspose = transpose;
}


// Print error message to stdout, and abort app.
void konfytJackEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: konfytJackClient:" << msg.toLocal8Bit().constData();
    abort();
}

