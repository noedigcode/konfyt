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

#ifndef KONFYT_MIDI_FILTER_H
#define KONFYT_MIDI_FILTER_H

#include "konfytUtils.h"
#include "konfytStructs.h"
#include "konfytMidi.h"
#include "xml.h"

#include <QList>

// ============================================================================

struct MidiFilterMapping {
    QList<int> inNodes;
    QList<int> outNodes;
    int map(int inValue);
    MidiFilterMapping();
    void update();
    int clamp(int value, int min, int max);
    void fromString(QString s);
    QString toString();
private:
    int mMap[128];
};

// ============================================================================

struct MidiFilterZone {
    int lowNote = 0;
    int highNote = 127;
    int add = 0;
    int lowVel = 0;
    int highVel = 127;
    int velLimitMin = 0;
    int velLimitMax = 127;
    int pitchDownMax = MIDI_PITCHBEND_SIGNED_MIN;
    int pitchUpMax = MIDI_PITCHBEND_SIGNED_MAX;
    MidiFilterMapping velocityMap;
};

// ============================================================================

class MidiFilter
{
public:
    static MidiFilter allPassFilter();

    MidiFilterZone zone;
    void setZone(int lowNote, int highNote, int add, int lowVel, int highVel,
                 int velLimitMin, int velLimitMax);
    void setZone(MidiFilterZone newZone);

    bool passFilter(const KonfytMidiEvent *ev);
    KonfytMidiEvent modify(const KonfytMidiEvent* ev);

    QList<int> passCC{64};
    QList<int> blockCC;
    bool passAllCC = false;
    bool passProg = false;
    bool passPitchbend = true;
    int inChan = -1; // -1 = any
    int outChan = -1; // -1 = original
    bool ignoreGlobalTranspose = false;

    Xml toXml() const;
    void readFromXml(Xml xml);

    void deprecatedVelocityToMap();

    Xml boolToXml(QString name, bool value) const;

    static constexpr const char* XML_MIDIFILTER = "midiFilter";
    static constexpr const char* XML_ZONE = "zone";
    static constexpr const char* XML_ZONE_LOWNOTE = "lowNote";
    static constexpr const char* XML_ZONE_HINOTE = "highNote";
    static constexpr const char* XML_ZONE_ADD = "add";
    static constexpr const char* XML_ZONE_LOWVEL = "lowVel";
    static constexpr const char* XML_ZONE_HIVEL = "highVel";
    static constexpr const char* XML_ZONE_VEL_LIMIT_MIN = "velLimitMin";
    static constexpr const char* XML_ZONE_VEL_LIMIT_MAX = "velLimitMax";
    static constexpr const char* XML_ZONE_PITCH_DOWN_MAX = "pitchDownMax";
    static constexpr const char* XML_ZONE_PITCH_UP_MAX = "pitchUpMax";
    static constexpr const char* XML_ZONE_VELOCITY_MAP = "velocityMap";
    static constexpr const char* XML_PASSALLCC = "passAllCC";
    static constexpr const char* XML_PASSPB = "passPitchbend";
    static constexpr const char* XML_PASSPROG = "passProg";
    static constexpr const char* XML_IGNORE_GLOBAL_TRANSPOSE = "ignoreGlobalTranspose";
    static constexpr const char* XML_CC = "cc";
    static constexpr const char* XML_BLOCK_CC = "blockcc";
    static constexpr const char* XML_INCHAN = "inChan";
    static constexpr const char* XML_OUTCHAN = "outChan";
};

#endif // KONFYT_MIDI_FILTER_H
