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

#include "konfytMidiFilter.h"

#include <QRegularExpression>

void KonfytMidiFilter::setPassAll()
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

void KonfytMidiFilter::setZone(int lowNote, int highNote, int add, int lowVel, int highVel, int velLimitMin, int velLimitMax)
{
    zone.lowNote = lowNote;
    zone.highNote = highNote;
    zone.add = add;
    zone.lowVel = lowVel;
    zone.highVel = highVel;
    zone.velLimitMin = velLimitMin;
    zone.velLimitMax = velLimitMax;
}

void KonfytMidiFilter::setZone(KonfytMidiFilterZone newZone)
{
    zone = newZone;
}

/* Returns true if midi event in specified buffer passes based on
 * filter rules (e.g. note is in the required key and velocity zone). */
bool KonfytMidiFilter::passFilter(const KonfytMidiEvent* ev)
{
    bool pass = false;

    // If inChan < 0, pass for any channel. Otherwise, channel must match.
    if (inChan >= 0) {
        if (ev->channel != inChan) {
            return false;
        }
    }

    if (ev->type() == MIDI_EVENT_TYPE_CC) {

        if (!this->blockCC.contains(ev->data1())) {
            if (this->passAllCC) {
                pass = true;
            } else {
                if (this->passCC.contains(ev->data1())) {
                    pass = true;
                }
            }
        }

    } else if ( ev->type() == MIDI_EVENT_TYPE_PROGRAM ) {

        if (this->passProg) { pass = true; }

    } else if ( ev->type() == MIDI_EVENT_TYPE_PITCHBEND ) {

        if (this->passPitchbend) { pass = true; }

    } else if ( (ev->type() == MIDI_EVENT_TYPE_NOTEON) || (ev->type() == MIDI_EVENT_TYPE_NOTEOFF) ) {

        // Check note
        if ( (ev->note() >= zone.lowNote) && (ev->note() <= zone.highNote) ) {
            // If NoteOn, check velocity
            if (ev->type() == MIDI_EVENT_TYPE_NOTEON) {
                if (zone.velocityMap.map(ev->velocity()) > 0) {
                    // Check if note will still be valid after addition
                    int note = ev->note() + zone.add;
                    if ( (note <= 127) && (note >= 0) ) {
                        pass = true;
                    }
                }
            } else {
                // else, if note off, pass if note will be valid after addition
                int note = ev->note() + zone.add;
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
KonfytMidiEvent KonfytMidiFilter::modify(const KonfytMidiEvent* ev)
{
    KonfytMidiEvent r = *ev;

    // Set output channel if outChan >= 0; If outChan is -1, leave channel as is.
    if (outChan >= 0) {
        r.channel = outChan;
    }

    if ( (r.type() == MIDI_EVENT_TYPE_NOTEON) || (r.type() == MIDI_EVENT_TYPE_NOTEOFF) ) {
        // Modify based on addition
        r.setNote( r.note() + zone.add );
        // Velocity map
        if (r.type() == MIDI_EVENT_TYPE_NOTEON) {
            r.setVelocity(zone.velocityMap.map(r.velocity()));
        }
    } else if (r.type() == MIDI_EVENT_TYPE_PITCHBEND) {
        float in = r.pitchbendValueSigned();
        float range = in < 0 ? zone.pitchDownMax : zone.pitchUpMax;
        float max = in < 0 ? MIDI_PITCHBEND_SIGNED_MIN : MIDI_PITCHBEND_SIGNED_MAX;
        int out = (in / max) * range;
        r.setPitchbend(out);
    }
    return r;
}

void KonfytMidiFilter::writeToXMLStream(QXmlStreamWriter *stream)
{
    stream->writeStartElement(XML_MIDIFILTER);

    // Note / velocity zone
    KonfytMidiFilterZone z = this->zone;
    stream->writeStartElement(XML_MIDIFILTER_ZONE);
    stream->writeTextElement(XML_MIDIFILTER_ZONE_LOWNOTE, n2s(z.lowNote));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_HINOTE, n2s(z.highNote));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_ADD, n2s(z.add));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_LOWVEL, n2s(z.lowVel));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_HIVEL, n2s(z.highVel));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_VEL_LIMIT_MIN, n2s(z.velLimitMin));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_VEL_LIMIT_MAX, n2s(z.velLimitMax));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_PITCH_DOWN_MAX, n2s(z.pitchDownMax));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_PITCH_UP_MAX, n2s(z.pitchUpMax));
    stream->writeTextElement(XML_MIDIFILTER_ZONE_VELOCITY_MAP, z.velocityMap.toString());
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

    // ignoreGlobalTranspose
    stream->writeTextElement(XML_MIDIFILTER_IGNORE_GLOBAL_TRANSPOSE,
                             bool2str(this->ignoreGlobalTranspose));

    // List of allowed CCs
    for (int i=0; i<this->passCC.count(); i++) {
        stream->writeTextElement(XML_MIDIFILTER_CC, n2s(this->passCC.at(i)));
    }
    // List of blocked CCs
    foreach (int cc, blockCC) {
        stream->writeTextElement(XML_MIDIFILTER_BLOCK_CC, n2s(cc));
    }

    // Input/output channels
    stream->writeTextElement(XML_MIDIFILTER_INCHAN, n2s(this->inChan));
    stream->writeTextElement(XML_MIDIFILTER_OUTCHAN, n2s(this->outChan));

    stream->writeEndElement(); // midiFilter
}

void KonfytMidiFilter::readFromXMLStream(QXmlStreamReader *r)
{
    this->passCC.clear();
    this->blockCC.clear();

    while (r->readNextStartElement()) { // Filter properties
        if (r->name() == XML_MIDIFILTER_ZONE) {
            KonfytMidiFilterZone z;
            bool gotMap = false;
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
                } else if (r->name() == XML_MIDIFILTER_ZONE_VEL_LIMIT_MAX) {
                    z.velLimitMax = r->readElementText().toInt();
                } else if (r->name() == XML_MIDIFILTER_ZONE_PITCH_DOWN_MAX) {
                    z.pitchDownMax = r->readElementText().toInt();
                } else if (r->name() == XML_MIDIFILTER_ZONE_PITCH_UP_MAX) {
                    z.pitchUpMax = r->readElementText().toInt();
                } else if (r->name() == XML_MIDIFILTER_ZONE_VELOCITY_MAP) {
                    gotMap = true;
                    z.velocityMap.fromString(r->readElementText());
                } else {
                    r->skipCurrentElement();
                }
            } // end zone while
            this->setZone(z);
            if (!gotMap) {
                // No velocity map loaded. This is probably an old project file.
                // Convert the old velocity min/max and limits to a map.
                deprecatedVelocityToMap();
            }
        } else if (r->name() == XML_MIDIFILTER_PASSALLCC) {
            this->passAllCC = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER_PASSPB) {
            this->passPitchbend = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER_PASSPROG) {
            this->passProg = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER_IGNORE_GLOBAL_TRANSPOSE) {
            this->ignoreGlobalTranspose = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER_CC) {
            this->passCC.append(r->readElementText().toInt());
        } else if (r->name() == XML_MIDIFILTER_BLOCK_CC) {
            this->blockCC.append(r->readElementText().toInt());
        } else if (r->name() == XML_MIDIFILTER_INCHAN) {
            this->inChan = r->readElementText().toInt();
        } else if (r->name() == XML_MIDIFILTER_OUTCHAN) {
            this->outChan = r->readElementText().toInt();
        } else {
            r->skipCurrentElement();
        }
    } // end filter while
}

/* Translate the old deprecated velocity min/max and limits to a velocity map. */
void KonfytMidiFilter::deprecatedVelocityToMap()
{
    zone.velocityMap.inNodes.clear();
    zone.velocityMap.outNodes.clear();

    if (zone.lowVel > 0) {
        zone.velocityMap.inNodes.append(zone.lowVel - 1);
        zone.velocityMap.outNodes.append(0);
    }
    zone.velocityMap.inNodes.append(zone.lowVel);
    zone.velocityMap.outNodes.append(zone.lowVel);

    zone.velocityMap.inNodes.append(zone.highVel);
    zone.velocityMap.outNodes.append(zone.highVel);
    if (zone.highVel < 127) {
        zone.velocityMap.inNodes.append(zone.highVel + 1);
        zone.velocityMap.outNodes.append(0);
    }

    if (zone.velLimitMin > zone.lowVel) {
        for (int i=0; i < zone.velocityMap.inNodes.count(); i++) {
            if (zone.velocityMap.inNodes[i] == zone.lowVel) {
                zone.velocityMap.outNodes[i] = zone.velLimitMin;
                zone.velocityMap.inNodes.insert(i+1, zone.velLimitMin);
                zone.velocityMap.outNodes.insert(i+1, zone.velLimitMin);
                break;
            }
        }
    }

    if (zone.velLimitMax < zone.highVel) {
        for (int i=0; i < zone.velocityMap.inNodes.count(); i++) {
            if (zone.velocityMap.inNodes[i] == zone.highVel) {
                zone.velocityMap.outNodes[i] = zone.velLimitMax;
                zone.velocityMap.inNodes.insert(i, zone.velLimitMax);
                zone.velocityMap.outNodes.insert(i, zone.velLimitMax);
                break;
            }
        }
    }
}

int KonfytMidiMapping::map(int inValue)
{
    if ((inValue < 0) || (inValue > 127)) {
        return 0;
    }
    return mMap[inValue];
}

KonfytMidiMapping::KonfytMidiMapping()
{
    inNodes << 0 << 127;
    outNodes << 0 << 127;
    update();
}

void KonfytMidiMapping::update()
{
    int lastin = 0;
    int lastout = outNodes.value(0);
    int mapi = 0;
    for (int i = 0; i < inNodes.count(); i++) {

        int in = clamp(inNodes.value(i), 0, 127);
        int out = clamp(outNodes.value(i), 0, 127);

        float inrange = clamp(in - lastin, 1, 127);

        for (; mapi <= in; mapi++) {

            mMap[mapi] = (mapi - lastin)/inrange * (out - lastout) + lastout;
        }

        lastin = in;
        lastout = out;
    }
    for (; mapi < 128; mapi++) {
        mMap[mapi] = lastout;
    }
}

int KonfytMidiMapping::clamp(int value, int min, int max)
{
    return qMax(min, qMin(max, value));
}

void KonfytMidiMapping::fromString(QString s)
{
    // Decode string in the form [i1, i2, ... in; o1, o2, ... on]
    // Where i1 to in and o1 to on are integers,
    // "[" and "]" are optional,
    // Commas and spaces are optional in that either commas, spaces, or both
    // may be used. Only the ";" is required.

    // Remove everything before and including "["
    s.remove(QRegularExpression(".*\\["));
    // Remove everything after and including "]"
    s.remove(QRegularExpression("\\].*"));
    // Input and output terms are separated with ";"
    QStringList terms = s.split(";");    // Replace commas with spaces and simplify
    QStringList in = terms.value(0).replace(",", " ").simplified().split(" ");
    QStringList out = terms.value(1).replace(",", " ").simplified().split(" ");
    inNodes.clear();
    foreach (QString t, in) {
        inNodes.append(t.toInt());
    }
    outNodes.clear();
    foreach (QString t, out) {
        outNodes.append(t.toInt());
    }
    update();
}

QString KonfytMidiMapping::toString()
{
    QString s1;
    foreach (int i, inNodes) {
        if (!s1.isEmpty()) { s1 += " "; }
        s1.append(QString::number(i));
    }
    QString s2;
    foreach (int i, outNodes) {
        if (!s2.isEmpty()) { s2 += " "; }
        s2.append(QString::number(i));
    }

    return QString("%1; %2\n").arg(s1).arg(s2);
}

