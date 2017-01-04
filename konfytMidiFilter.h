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

private:
    QList<konfytMidiFilterZone> zoneList;


};

#endif // KONFYT_MIDI_FILTER_H
