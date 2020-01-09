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

#ifndef KONFYTJACKSTRUCTS_H
#define KONFYTJACKSTRUCTS_H

#include "konfytMidiFilter.h"
#include "ringbufferqmutex.h"

enum KonfytJackPortType {
    KonfytJackPortType_AudioIn  = 0,
    KonfytJackPortType_AudioOut = 1,
    KonfytJackPortType_MidiIn   = 2,
    KonfytJackPortType_MidiOut  = 3,
};

struct KonfytJackPortsSpec
{
    QString name;
    QString midiOutConnectTo;
    KonfytMidiFilter midiFilter;
    QString audioInLeftConnectTo;
    QString audioInRightConnectTo;
};

struct KonfytJackPort {
    int id = 0;
    float gain = 1;
    jack_port_t* jack_pointer = NULL;
    void* buffer;
    KonfytMidiFilter filter;
    QStringList connectionList;
    int noteOns = 0;
    bool sustainNonZero = false;
    bool pitchbendNonZero = false;
};

struct KonfytJackMidiRoute {
    int id = 0;
    bool active = false;
    bool prev_active = false;
    KonfytMidiFilter filter;
    KonfytJackPort* source = NULL;
    KonfytJackPort* destPort = NULL;
    int destFluidsynthID = 0;
    bool destIsJackPort = true;
    RingbufferQMutex<KonfytMidiEvent> eventsTxBuffer{100};
};

struct KonfytJackAudioRoute {
    int id = 0;
    bool active = false;
    bool prev_active = false;
    float gain = 1;
    unsigned int fadeoutCounter = 0;
    bool fadingOut = false;
    KonfytJackPort* source = NULL;
    KonfytJackPort* dest = NULL;
};

struct KonfytJackPluginPorts {
    int id;
    int idInPluginEngine; // Id in plugin's respective engine (used for Fluidsynth)
    KonfytJackPort* midi;        // Send midi output to plugin
    KonfytJackPort* audio_in_l;  // Receive plugin audio
    KonfytJackPort* audio_in_r;
    int midiRouteId;
    int audioLeftRouteId;
    int audioRightRouteId;
};

struct KonfytJackNoteOnRecord {
    int note;
    bool jackPortNotFluidsynth; // true for jack port, false for Fluidsynth
    int fluidsynthID;
    KonfytJackPort* port;
    KonfytJackPort* sourcePort;
    KonfytMidiFilter filter;
    int globalTranspose;
};

struct KonfytJackConPair {
    QString srcPort;
    QString destPort;

    QString toString()
    {
        return srcPort + " \u2B95 " + destPort;
    }

    bool equals(const KonfytJackConPair &a) {
        return ( (this->srcPort == a.srcPort) && (this->destPort == a.destPort) );
    }
};



#endif // KONFYTJACKSTRUCTS_H
