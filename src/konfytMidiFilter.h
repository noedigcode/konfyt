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
#include "konfytDefines.h"
#include "konfytStructs.h"
#include "konfytMidi.h"

struct konfytMidiFilterZone {
    int lowNote;
    int highNote;
    int multiply;
    int add;
    int lowVel;
    int highVel;
};

class konfytMidiFilter
{
public:
    konfytMidiFilter();

    void addZone(int lowNote, int highNote, int multiply, int add, int lowVel, int highVel);
    void addZone(konfytMidiFilterZone newZone);
    QList<konfytMidiFilterZone> getZoneList();
    int numZones();
    void removeZone(int i);

    bool passFilter(const konfytMidiEvent *ev);
    konfytMidiEvent modify(const konfytMidiEvent* ev);

    QList<int> passCC;
    bool passAllCC;
    bool passProg;
    bool passPitchbend;
    int inChan;
    int outChan;

private:
    QList<konfytMidiFilterZone> zoneList;


};

#endif // KONFYT_MIDI_FILTER_H
