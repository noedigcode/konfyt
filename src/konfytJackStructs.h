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

#ifndef KONFYTJACKSTRUCTS_H
#define KONFYTJACKSTRUCTS_H

#include "konfytMidiFilter.h"

enum KonfytJackPortType {
    KonfytJackPortType_AudioIn  = 0,
    KonfytJackPortType_AudioOut = 1,
    KonfytJackPortType_MidiIn   = 2,
    KonfytJackPortType_MidiOut  = 3,
};

struct KonfytJackPort {
    jack_port_t* jack_pointer;
    bool active;
    bool prev_active;
    void* buffer;
    konfytMidiFilter filter;
    bool solo;
    bool mute;
    float gain;
    QStringList connectionList;
    // TODO MIDI IN: Handle MIDI source port in JACK process.
    KonfytJackPort* destOrSrcPort; // Destination (bus) for audio port, or source (input) for MIDI port.
    int noteOns;
    bool sustainNonZero;
    bool pitchbendNonZero;
    unsigned int fadeoutCounter;
    bool fadingOut;

    KonfytJackPort() : jack_pointer(NULL),
                       active(false),
                       prev_active(false),
                       solo(false),
                       mute(false),
                       gain(1),
                       destOrSrcPort(NULL),
                       noteOns(0),
                       sustainNonZero(false),
                       pitchbendNonZero(false),
                       fadeoutCounter(0),
                       fadingOut(false) {}

};

struct KonfytJackPluginPorts {
    int plugin_id;
    KonfytJackPort* midi;        // Send midi output to plugin
    KonfytJackPort* audio_in_l;  // Receive plugin audio
    KonfytJackPort* audio_in_r;
};

struct KonfytJackNoteOnRecord {
    int note;
    bool jackPortNotFluidsynth; // true for jack port, false for Fluidsynth
    int fluidsynthID;
    KonfytJackPort* port;
    KonfytJackPort* relatedPort;
    konfytMidiFilter filter;
    int globalTranspose;
};

struct KonfytJackConPair {
    QString srcPort;
    QString destPort;

    QString toString()
    {
        return srcPort + " --> " + destPort;
    }

    bool equals(const KonfytJackConPair &a) {
        return ( (this->srcPort == a.srcPort) && (this->destPort == a.destPort) );
    }
};



#endif // KONFYTJACKSTRUCTS_H
