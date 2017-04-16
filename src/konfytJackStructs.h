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

enum konfytJackPortType {
    KonfytJackPortType_AudioIn  = 0,
    KonfytJackPortType_AudioOut = 1,
    KonfytJackPortType_MidiIn   = 2,
    KonfytJackPortType_MidiOut  = 3,
};

struct konfytJackPort {
    jack_port_t* jack_pointer;
    bool active;
    bool prev_active;
    void* buffer;
    konfytMidiFilter filter;
    bool solo;
    bool mute;
    float gain;
    QStringList connectionList;
    konfytJackPort* destinationPort; // For audio input ports, the destination output port (bus).
    int noteOns;
    bool sustainNonZero;
    bool pitchbendNonZero;
    unsigned int fadeoutCounter;
    bool fadingOut;

    konfytJackPort() : jack_pointer(NULL),
                       active(false),
                       prev_active(false),
                       solo(false),
                       mute(false),
                       gain(1),
                       destinationPort(NULL),
                       noteOns(0),
                       sustainNonZero(false),
                       pitchbendNonZero(false),
                       fadeoutCounter(0),
                       fadingOut(false) {}

};

struct konfytJackPluginPorts {
    int plugin_id;
    konfytJackPort* midi_out;    // Send midi output to plugin
    konfytJackPort* audio_in_l;  // Receive plugin audio
    konfytJackPort* audio_in_r;

};

struct konfytJackNoteOnRecord {
    int note;
    bool jackPortNotFluidsynth; // true for jack port, false for Fluidsynth
    int fluidsynthID;
    konfytJackPort* port;
    konfytJackPort* relatedPort;
    konfytMidiFilter filter;
    int globalTranspose;
};



#endif // KONFYTJACKSTRUCTS_H
