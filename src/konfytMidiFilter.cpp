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

void konfytMidiFilter::setZone(int lowNote, int highNote, int multiply, int add, int lowVel, int highVel)
{
    zone.lowNote = lowNote;
    zone.highNote = highNote;
    zone.multiply = multiply;
    zone.add = add;
    zone.lowVel = lowVel;
    zone.highVel = highVel;
}

void konfytMidiFilter::setZone(konfytMidiFilterZone newZone)
{
    zone = newZone;
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

        // Check note
        if ( (ev->data1>=zone.lowNote) && (ev->data1<=zone.highNote) ) {
            // If NoteOn, check velocity
            if (ev->type == MIDI_EVENT_TYPE_NOTEON) {
                if ( (ev->data2>=zone.lowVel) && (ev->data2<=zone.highVel) ) {
                    pass = true;
                }
            } else {
                // else, if note off, always pass
                pass = true;
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

        if ( (r.data1>=zone.lowNote) && (r.data1<=zone.highNote) ) { // Check if in zone
            // Modify based on multiplification and addition
            r.data1 = r.data1*zone.multiply + zone.add;
            // TODO: In the following cases the note shouldn't actually be passed.
            //       We don't want an unexpected note with value 0 or 127.
            //       For now, velocity is just made zero.
            if (r.data1<0) { r.data1 = 0; r.data2 = 0; }
            if (r.data1 > 127) { r.data1 = 127; r.data2 = 0; }
        }
    }
    return r;
}
