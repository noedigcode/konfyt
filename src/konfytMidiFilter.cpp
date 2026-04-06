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


MidiFilter MidiFilter::allPassFilter()
{
    MidiFilter f;
    f.passCC.clear();
    f.blockCC.clear();
    f.passAllCC = true;
    f.passProg = true;
    f.passPitchbend = true;
    f.inChan = -1;
    f.outChan = -1;
    f.zone.lowNote = 0;
    f.zone.highNote = 127;
    f.zone.lowVel = 0;
    f.zone.highVel = 127;
    return f;
}

void MidiFilter::setZone(int lowNote, int highNote, int add, int lowVel, int highVel, int velLimitMin, int velLimitMax)
{
    zone.lowNote = lowNote;
    zone.highNote = highNote;
    zone.add = add;
    zone.lowVel = lowVel;
    zone.highVel = highVel;
    zone.velLimitMin = velLimitMin;
    zone.velLimitMax = velLimitMax;
}

void MidiFilter::setZone(MidiFilterZone newZone)
{
    zone = newZone;
}

/* Returns true if midi event in specified buffer passes based on
 * filter rules (e.g. note is in the required key and velocity zone). */
bool MidiFilter::passFilter(const KonfytMidiEvent* ev)
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
KonfytMidiEvent MidiFilter::modify(const KonfytMidiEvent* ev)
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

Xml MidiFilter::toXml() const
{
    Xml xml(XML_MIDIFILTER);

    // Note / velocity zone
    MidiFilterZone z = this->zone;
    Xml zoneXml(XML_ZONE);
    zoneXml.addTextChild(XML_ZONE_LOWNOTE, n2s(z.lowNote));
    zoneXml.addTextChild(XML_ZONE_HINOTE, n2s(z.highNote));
    zoneXml.addTextChild(XML_ZONE_ADD, n2s(z.add));
    zoneXml.addTextChild(XML_ZONE_LOWVEL, n2s(z.lowVel));
    zoneXml.addTextChild(XML_ZONE_HIVEL, n2s(z.highVel));
    zoneXml.addTextChild(XML_ZONE_VEL_LIMIT_MIN, n2s(z.velLimitMin));
    zoneXml.addTextChild(XML_ZONE_VEL_LIMIT_MAX, n2s(z.velLimitMax));
    zoneXml.addTextChild(XML_ZONE_PITCH_DOWN_MAX, n2s(z.pitchDownMax));
    zoneXml.addTextChild(XML_ZONE_PITCH_UP_MAX, n2s(z.pitchUpMax));
    zoneXml.addTextChild(XML_ZONE_VELOCITY_MAP, z.velocityMap.toString());
    xml.addChild(zoneXml);

    xml.addChild(boolToXml(XML_PASSALLCC, this->passAllCC));
    xml.addChild(boolToXml(XML_PASSPB, this->passPitchbend));
    xml.addChild(boolToXml(XML_PASSPROG, this->passProg));
    xml.addChild(boolToXml(XML_IGNORE_GLOBAL_TRANSPOSE, this->ignoreGlobalTranspose));

    // List of allowed CCs
    foreach (int cc, passCC) {
        xml.addTextChild(XML_CC, n2s(cc));
    }

    // List of blocked CCs
    foreach (int cc, blockCC) {
        xml.addTextChild(XML_BLOCK_CC, n2s(cc));
    }

    // Input/output channels
    xml.addTextChild(XML_INCHAN, n2s(this->inChan));
    xml.addTextChild(XML_OUTCHAN, n2s(this->outChan));

    return xml;
}

void MidiFilter::readFromXml(Xml xml)
{
    this->passCC.clear();
    this->blockCC.clear();

    // MIDI Zone

    Xml zoneXml = xml.child(XML_ZONE);
    MidiFilterZone z;
    zoneXml.setIntFromChild(XML_ZONE_LOWNOTE, &z.lowNote);
    zoneXml.setIntFromChild(XML_ZONE_HINOTE, &z.highNote);
    zoneXml.setIntFromChild(XML_ZONE_ADD, &z.add);
    zoneXml.setIntFromChild(XML_ZONE_LOWVEL, &z.lowVel);
    zoneXml.setIntFromChild(XML_ZONE_HIVEL, &z.highVel);
    zoneXml.setIntFromChild(XML_ZONE_VEL_LIMIT_MIN, &z.velLimitMin);
    zoneXml.setIntFromChild(XML_ZONE_VEL_LIMIT_MAX, &z.velLimitMax);
    zoneXml.setIntFromChild(XML_ZONE_PITCH_DOWN_MAX, &z.pitchDownMax);
    zoneXml.setIntFromChild(XML_ZONE_PITCH_UP_MAX, &z.pitchUpMax);
    Xml mapXml = zoneXml.child(XML_ZONE_VELOCITY_MAP);
    bool gotMap = mapXml.isValid();
    if (gotMap) { z.velocityMap.fromString(mapXml.text()); }

    this->setZone(z);

    if (!gotMap) {
        // No velocity map loaded. This is probably an old project file.
        // Convert the old velocity min/max and limits to a map.
        deprecatedVelocityToMap();
    }

    xml.setBoolFromChild(XML_PASSALLCC, &this->passAllCC);
    xml.setBoolFromChild(XML_PASSPB, &this->passPitchbend);
    xml.setBoolFromChild(XML_PASSPROG, &this->passProg);
    xml.setBoolFromChild(XML_IGNORE_GLOBAL_TRANSPOSE, &this->ignoreGlobalTranspose);

    foreach (Xml ccXml, xml.childrenNamed(XML_CC)) {
        this->passCC.append(ccXml.text().toInt());
    }
    foreach (Xml blockccXml, xml.childrenNamed(XML_BLOCK_CC)) {
        this->blockCC.append(blockccXml.text().toInt());
    }
    xml.setIntFromChild(XML_INCHAN, &this->inChan);
    xml.setIntFromChild(XML_OUTCHAN, &this->outChan);
}

/* Translate the old deprecated velocity min/max and limits to a velocity map. */
void MidiFilter::deprecatedVelocityToMap()
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

Xml MidiFilter::boolToXml(QString name, bool value) const
{
    // Convert bool to "1" or "0" string. This is used to ensure backwards
    // compatibility with older projects that specifically checked for this.
    return Xml(name, value ? "1" : "0");
}

int MidiFilterMapping::map(int inValue)
{
    if ((inValue < 0) || (inValue > 127)) {
        return 0;
    }
    return mMap[inValue];
}

MidiFilterMapping::MidiFilterMapping()
{
    inNodes << 0 << 127;
    outNodes << 0 << 127;
    update();
}

void MidiFilterMapping::update()
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

int MidiFilterMapping::clamp(int value, int min, int max)
{
    return qMax(min, qMin(max, value));
}

void MidiFilterMapping::fromString(QString s)
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

QString MidiFilterMapping::toString()
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

    return QString("%1; %2").arg(s1).arg(s2);
}

