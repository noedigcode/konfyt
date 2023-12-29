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

#ifndef KONFYTJACKSTRUCTS_H
#define KONFYTJACKSTRUCTS_H

#include "konfytArrayList.h"
#include "konfytMidiFilter.h"
#include "ringbufferqmutex.h"
#include "konfytFluidsynthEngine.h"

#include <jack/jack.h>


struct KonfytJackPortsSpec
{
    QString name;
    QString midiOutConnectTo;
    KonfytMidiFilter midiFilter;
    QString audioInLeftConnectTo;
    QString audioInRightConnectTo;
};

struct KfJackAudioPort
{
    friend class KonfytJackEngine;
protected:
    float gain = 1;
    jack_port_t* jackPointer = nullptr;
    void* buffer;
    QStringList connectionList;
};

struct KfJackMidiPort
{
    friend class KonfytJackEngine;
protected:
    jack_port_t* jackPointer = nullptr;
    void* buffer;
    KonfytMidiFilter filter;
    // True to block events from being sent through, for when events need to be
    // diverted solely to scripting.
    bool blockDirectThrough = false;
    RingbufferQMutex<KonfytMidiEvent> eventsTxBuffer{100};
    QStringList connectionList;
    int noteOns = 0;
    bool sustainNonZero = false;
    bool pitchbendNonZero = false;
    int bankMSB[16] = {-1};
    int bankLSB[16] = {-1};
};

struct KonfytJackNoteOnRecord
{
    int note;
    int globalTranspose;
    int channel;
};

struct KfJackMidiRoute
{
    friend class KonfytJackEngine;
protected:
    bool active = false;
    bool prevActive = false;
    // True to block events from being sent through, for when events need to be
    // diverted solely to scripting.
    bool blockDirectThrough = false;
    KonfytMidiFilter preFilter;
    KonfytMidiFilter filter;
    KfJackMidiPort* source = nullptr;
    KfJackMidiPort* destPort = nullptr;
    KfFluidSynth* destFluidsynthID = nullptr;
    bool destIsJackPort = true;
    RingbufferQMutex<KonfytMidiEvent> eventsTxBuffer{100};
    uint16_t sustain = 0;
    uint16_t pitchbend = 0;
    KonfytArrayList<KonfytJackNoteOnRecord> noteOnList;
    int bankMSB[16] = {-1};
    int bankLSB[16] = {-1};
};

struct KfJackAudioRoute
{
    friend class KonfytJackEngine;
protected:
    bool active = false;
    bool prevActive = false;
    float gain = 1;
    unsigned int fadeoutCounter = 0;
    bool fadingOut = false;
    KfJackAudioPort* source = nullptr;
    KfJackAudioPort* dest = nullptr;
    float rxBufferSum = 0;
    int rxCycleCount = 0;
};

struct KfJackPluginPorts
{
    friend class KonfytJackEngine;
protected:
    KfFluidSynth* fluidSynthInEngine; // Id in plugin's respective engine (used for Fluidsynth)
    KfJackMidiPort* midi;        // Send midi output to plugin
    KfJackAudioPort* audioInLeft;  // Receive plugin audio
    KfJackAudioPort* audioInRight;
    KfJackMidiRoute* midiRoute = nullptr;
    KfJackAudioRoute* audioLeftRoute = nullptr;
    KfJackAudioRoute* audioRightRoute = nullptr;
};

struct KonfytJackConPair
{
    QString srcPort;
    QString destPort;
    bool makeNotBreak = true;

    QString toString()
    {
        return QString("%1 \u2B95 %2 \u2B95 %3")
                .arg(srcPort)
                .arg(makeNotBreak ? "make" : "break")
                .arg(destPort);
    }

    bool equals(const KonfytJackConPair &a)
    {
        return (    (this->srcPort == a.srcPort)
                 && (this->destPort == a.destPort)
                 && (this->makeNotBreak == a.makeNotBreak) );
    }
};
typedef QSharedPointer<KonfytJackConPair> KonfytJackConPairPtr;

struct KfJackMidiRxEvent
{
    KfJackMidiPort* sourcePort = nullptr;
    KfJackMidiRoute* midiRoute = nullptr;
    KonfytMidiEvent midiEvent;
};

struct KfJackAudioRxEvent
{
    KfJackMidiPort* sourcePort = nullptr;
    KfJackAudioRoute* audioRoute = nullptr;
    float data = 0;
};


#endif // KONFYTJACKSTRUCTS_H
