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

#include "konfytMidiFilter.h"

konfytMidiFilter::konfytMidiFilter()
{
    this->passAllCC = false;

    this->passCC.append(64); // 64 = sustain pedal

    this->passProg = false;
    this->passPitchbend = true;

    this->inChan = -1; // -1 = any
    this->outChan = -1; // -1 = original

}

void konfytMidiFilter::addZone(int lowNote, int highNote, int multiply, int add, int lowVel, int highVel)
{
    konfytMidiFilterZone z;
    z.lowNote = lowNote;
    z.highNote = highNote;
    z.multiply = multiply;
    z.add = add;
    z.lowVel = lowVel;
    z.highVel = highVel;
    zoneList.append(z);
}

void konfytMidiFilter::addZone(konfytMidiFilterZone newZone)
{
    zoneList.append(newZone);
}

QList<konfytMidiFilterZone> konfytMidiFilter::getZoneList()
{
    return zoneList;
}

int konfytMidiFilter::numZones()
{
    return zoneList.count();
}

void konfytMidiFilter::removeZone(int i)
{
    if ( (i>=0) && (i<zoneList.count())) {
        zoneList.removeAt(i);
    }
}

// Returns true if midi event in specified buffer passes based on
// filter rules (e.g. note is in the required key and velocity zone)
bool konfytMidiFilter::passFilter(const konfytMidiEvent* ev)
{
    bool pass = false;

    // If inChan < 0, pass for any channel. Otherwise, channel must match.
    if (inChan >= 0) {
        if (ev->channel != inChan) {
            return false;
        }
    }

    if (ev->type == MIDI_EVENT_TYPE_CC) {

        if (this->passAllCC) {
            pass = true;
        } else {
            if (this->passCC.contains(ev->data1)) {
                pass = true;
            }
        }

    } else if ( ev->type == MIDI_EVENT_TYPE_PROGRAM ) {

        if (this->passProg) { pass = true; }

    } else if ( ev->type == MIDI_EVENT_TYPE_PITCHBEND ) {

        if (this->passPitchbend) { pass = true; }

    } else if ( (ev->type == MIDI_EVENT_TYPE_NOTEON) || (ev->type == MIDI_EVENT_TYPE_NOTEOFF) ) {

        // Pass by default if no zones are defined.
        if (zoneList.count() == 0) {
            return true;
        }

        for (int i=0; i<zoneList.count(); i++) {
            konfytMidiFilterZone z = zoneList.at(i);
            // Check note
            if ( (ev->data1>=z.lowNote) && (ev->data1<=z.highNote) ) {
                // If NoteOn, check velocity
                if (ev->type == MIDI_EVENT_TYPE_NOTEON) {
                    if ( (ev->data2>=z.lowVel) && (ev->data2<=z.highVel) ) {
                        pass = true;
                    }
                } else {
                    // else, if note off, always pass
                    pass = true;
                }
            }
        }

    }

    return pass;
}

// Modify buffer (containing midi event) based on filter rules, e.g.
// transposing, midi channel, etc.
konfytMidiEvent konfytMidiFilter::modify(const konfytMidiEvent* ev)
{
    konfytMidiEvent r = *ev;

    // Set output channel if outChan >= 0; If outChan is -1, leave channel as is.
    if (outChan >= 0) {
        r.channel = outChan;
    }

    if ( (r.type == MIDI_EVENT_TYPE_NOTEON) || (r.type == MIDI_EVENT_TYPE_NOTEOFF) ) {

        for (int i=0; i<zoneList.count(); i++) {
            konfytMidiFilterZone z = zoneList.at(i);
            if ( (r.data1>=z.lowNote) && (r.data1<=z.highNote) ) { // Check if in zone
                // Modify based on multiplification and addition
                r.data1 = r.data1*z.multiply + z.add;
                if (r.data1<0) { r.data1 = 0; }
                if (r.data1 > 127) { r.data1 = 127; }
            }
        }
    }
    return r;
}
