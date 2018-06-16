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

void konfytMidiFilter::setPassAll()
{
    this->passAllCC = true;
    this->passProg = true;
    this->passPitchbend = true;
    this->inChan = -1;
    this->outChan = -1;
    this->zone.lowNote = 0;
    this->zone.highNote = 127;
    this->zone.lowVel = 0;
    this->zone.highVel = 127;
}

void konfytMidiFilter::setZone(int lowNote, int highNote, int add, int lowVel, int highVel, int velLimitMin)
{
    zone.lowNote = lowNote;
    zone.highNote = highNote;
    zone.add = add;
    zone.lowVel = lowVel;
    zone.highVel = highVel;
    zone.velLimitMin = velLimitMin;
}

void konfytMidiFilter::setZone(konfytMidiFilterZone newZone)
{
    zone = newZone;
}

/* Returns true if midi event in specified buffer passes based on
 * filter rules (e.g. note is in the required key and velocity zone). */
bool konfytMidiFilter::passFilter(const KonfytMidiEvent* ev)
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
                    // Check if note will still be valid after addition
                    int note = ev->data1 + zone.add;
                    if ( (note <= 127) && (note >= 0) ) {
                        pass = true;
                    }
                }
            } else {
                // else, if note off, pass if note will be valid after addition
                int note = ev->data1 + zone.add;
                if ( (note <= 127) && (note >= 0) ) {
                    pass = true;
                }
            }
        }

    }

    return pass;
}

/* Modify buffer (containing midi event) based on filter rules,
 * e.g. transposing, midi channel, etc.
 * It is assumed that passFilter() has already been called and returned true. */
KonfytMidiEvent konfytMidiFilter::modify(const KonfytMidiEvent* ev)
{
    KonfytMidiEvent r = *ev;

    // Set output channel if outChan >= 0; If outChan is -1, leave channel as is.
    if (outChan >= 0) {
        r.channel = outChan;
    }

    if ( (r.type == MIDI_EVENT_TYPE_NOTEON) || (r.type == MIDI_EVENT_TYPE_NOTEOFF) ) {
        // Modify based on addition
        r.data1 = r.data1 + zone.add;
        // Limit velocity
        if (r.type == MIDI_EVENT_TYPE_NOTEON) {
            if (r.data2 < zone.velLimitMin) {
                r.data2 = zone.velLimitMin;
            }
        }
    }
    return r;
}

void konfytMidiFilter::writeToXMLStream(QXmlStreamWriter *stream)
{
    stream->writeStartElement(XML_MIDIFILTER);

    // Note / velocity zone
    konfytMidiFilterZone z = this->zone;
    stream->writeStartElement(XML_MIDIFILTER_ZONE);
    stream->writeTextElement(XML_MIDIFILTER_ZONE_LOWNOTE, n2s(z.lowNote));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_HINOTE, n2s(z.highNote));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_ADD, n2s(z.add));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_LOWVEL, n2s(z.lowVel));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_HIVEL, n2s(z.highVel));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_VEL_LIMIT_MIN, n2s(z.velLimitMin));
    stream->writeEndElement();

    // passAllCC
    QString tempBool;
    if (this->passAllCC) { tempBool = "1"; } else { tempBool = "0"; }
    stream->writeTextElement(XML_MIDIFILTER_PASSALLCC, tempBool);

    // passPitchbend
    if (this->passPitchbend) { tempBool = "1"; } else { tempBool = "0"; }
    stream->writeTextElement(XML_MIDIFILTER_PASSPB, tempBool);

    // passProg
    if (this->passProg) { tempBool = "1"; } else { tempBool = "0"; }
    stream->writeTextElement(XML_MIDIFILTER_PASSPROG, tempBool);

    // CC list
    for (int i=0; i<this->passCC.count(); i++) {
        stream->writeTextElement(XML_MIDIFILTER_CC, n2s(this->passCC.at(i)));
    }

    // Input/output channels
    stream->writeTextElement(XML_MIDIFILTER_INCHAN, n2s(this->inChan));
    stream->writeTextElement(XML_MIDIFILTER_OUTCHAN, n2s(this->outChan));

    stream->writeEndElement(); // midiFilter
}

void konfytMidiFilter::readFromXMLStream(QXmlStreamReader *r)
{
    this->passCC.clear();

    while (r->readNextStartElement()) { // Filter properties
        if (r->name() == XML_MIDIFILTER_ZONE) {
            konfytMidiFilterZone z;
            while (r->readNextStartElement()) { // zone properties
                if (r->name() == XML_MIDIFILTER_ZONE_LOWNOTE) {
                    z.lowNote = r->readElementText().toInt();
                } else if (r->name() == XML_MIDIFILTER_ZONE_HINOTE) {
                    z.highNote = r->readElementText().toInt();
                } else if (r->name() == XML_MIDIFILTER_ZONE_ADD) {
                    z.add = r->readElementText().toInt();
                } else if (r->name() ==  XML_MIDIFILTER_ZONE_LOWVEL) {
                    z.lowVel = r->readElementText().toInt();
                } else if (r->name() == XML_MIDIFILTER_ZONE_HIVEL) {
                    z.highVel = r->readElementText().toInt();
                } else if (r->name() == XML_MIDIFILTER_ZONE_VEL_LIMIT_MIN) {
                    z.velLimitMin = r->readElementText().toInt();
                } else {
                    r->skipCurrentElement();
                }
            } // end zone while
            this->setZone(z);
        } else if (r->name() == XML_MIDIFILTER_PASSALLCC) {
            this->passAllCC = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER_PASSPB) {
            this->passPitchbend = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER_PASSPROG) {
            this->passProg = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER_CC) {
            this->passCC.append(r->readElementText().toInt());
        } else if (r->name() == XML_MIDIFILTER_INCHAN) {
            this->inChan = r->readElementText().toInt();
        } else if (r->name() == XML_MIDIFILTER_OUTCHAN) {
            this->outChan = r->readElementText().toInt();
        } else {
            r->skipCurrentElement();
        }
    } // end filter while
}
