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

#ifndef KONFYT_MIDI_FILTER_H
#define KONFYT_MIDI_FILTER_H

#include <QList>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "konfytDefines.h"
#include "konfytStructs.h"
#include "konfytMidi.h"

#define XML_MIDIFILTER "midiFilter"
#define XML_MIDIFILTER_ZONE "zone"
#define XML_MIDIFILTER_ZONE_LOWNOTE "lowNote"
#define XML_MIDIFILTER_ZONE_HINOTE "highNote"
#define XML_MIDIFILTER_ZONE_ADD "add"
#define XML_MIDIFILTER_ZONE_LOWVEL "lowVel"
#define XML_MIDIFILTER_ZONE_HIVEL "highVel"
#define XML_MIDIFILTER_ZONE_VEL_LIMIT_MIN "velLimitMin"
#define XML_MIDIFILTER_ZONE_VEL_LIMIT_MAX "velLimitMax"
#define XML_MIDIFILTER_PASSALLCC "passAllCC"
#define XML_MIDIFILTER_PASSPB "passPitchbend"
#define XML_MIDIFILTER_PASSPROG "passProg"
#define XML_MIDIFILTER_CC "cc"
#define XML_MIDIFILTER_INCHAN "inChan"
#define XML_MIDIFILTER_OUTCHAN "outChan"


struct KonfytMidiFilterZone {
    int lowNote;
    int highNote;
    int add;
    int lowVel;
    int highVel;
    int velLimitMin;
    int velLimitMax;

    KonfytMidiFilterZone() : lowNote(0),
                             highNote(127),
                             add(0),
                             lowVel(0),
                             highVel(127),
                             velLimitMin(0),
                             velLimitMax(127) {}
};

class KonfytMidiFilter
{
public:
    KonfytMidiFilter();
    void setPassAll();

    KonfytMidiFilterZone zone;
    void setZone(int lowNote, int highNote, int add, int lowVel, int highVel,
                 int velLimitMin, int velLimitMax);
    void setZone(KonfytMidiFilterZone newZone);

    bool passFilter(const KonfytMidiEvent *ev);
    KonfytMidiEvent modify(const KonfytMidiEvent* ev);

    QList<int> passCC;
    bool passAllCC;
    bool passProg;
    bool passPitchbend;
    int inChan;
    int outChan;

    void writeToXMLStream(QXmlStreamWriter* stream);
    void readFromXMLStream(QXmlStreamReader *r);

};

#endif // KONFYT_MIDI_FILTER_H
