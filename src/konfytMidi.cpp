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

#include "konfytMidi.h"

const char* MIDI_NOTE_NAMES[] = {
    "C0", "C#0", "D0", "D#0", "E0", "F0", "F#0", "G0", "G#0", "A0", "A#0", "B0",
    "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1", "G#1", "A1", "A#1", "B1",
    "C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2", "A2", "A#2", "B2",
    "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "A#3", "B3",
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4",
    "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5",
    "C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6",
    "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7", "A7", "A#7", "B7",
    "C8", "C#8", "D8", "D#8", "E8", "F8", "F#8", "G8", "G#8", "A8", "A#8", "B8",
    "C9", "C#9", "D9", "D#9", "E9", "F9", "F#9", "G9", "G#9", "A9", "A#9", "B9",
    "C10", "C#10", "D10", "D#10", "E10", "F10", "F#10", "G10"
};

int pitchbendDataToInt_signed(unsigned char *data12)
{
    return ( ((data12[1] & 0x7F)<<7) | (data12[0] & 0x7F) ) - 8192;
}

int pitchbendDataToInt_signed(int data1, int data2)
{
    return ( ((data2 & 0x7F)<<7) | (data1 & 0x7F) ) - 8192;
}

void pitchbendSignedIntToData(int p, unsigned char *data12)
{
    p += 8192;
    data12[0] = p & 0x7F;
    data12[1] = (p >> 7) & 0x7F;
}

int hashMidiEventToInt(int type, int channel, int data1, int bankMSB, int bankLSB)
{
    // type chan data1     bankMSB   bankLSB
    // 1111 1111 1111 1111 1111 1111 1111 1111

    return (
               (
                    (
                        ( ( ( (type&0xF0) | (channel&0xF) ) << 8 ) | (data1&0x7F) ) << 8
                    ) | (bankMSB&0x7F)
               ) << 8
            ) | (bankLSB&0x7F);
}

QString midiEventToString(int type, int channel, int data1, int bankMSB, int bankLSB)
{
    QString text;
    if (type == MIDI_EVENT_TYPE_CC) {
        text = ("CC " + n2s(data1));
    } else if (type == MIDI_EVENT_TYPE_NOTEOFF) {
        text = ("Noteoff " + n2s(data1) + "  (" + midiNoteName(data1) + ")");
    } else if (type == MIDI_EVENT_TYPE_NOTEON) {
        text = ("Noteon " + n2s(data1) + "  (" + midiNoteName(data1) + ")");
    } else if (type == MIDI_EVENT_TYPE_PITCHBEND) {
        text = ("Pitchbend");
    } else if (type == MIDI_EVENT_TYPE_PROGRAM) {
        text = ("Program " + n2s(data1));
        if (bankMSB >= 0) {
            if (bankLSB >= 0) {
                text = ("Prog " + n2s(data1) + " Bank "
                        + n2s( ((bankMSB & 0x7F)<<7)
                               | (bankLSB & 0x7F) ));
            }
        }
    }
    text = "Ch " + n2s(channel+1) + " " + text;
    return text;
}

QString midiEventToString(int type, int channel, int data1, int data2, int bankMSB, int bankLSB)
{
    QString text;
    if (type == MIDI_EVENT_TYPE_CC) {
        text = ("CC " + n2s(data1) + ", " + n2s(data2));
    } else if (type == MIDI_EVENT_TYPE_NOTEOFF) {
        text = "Noteoff " + n2s(data1) + ", " + n2s(data2) + "  (" + midiNoteName(data1) + ")";
    } else if (type == MIDI_EVENT_TYPE_NOTEON) {
        text = "Noteon " + n2s(data1) + ", " + n2s(data2) + "  (" + midiNoteName(data1) + ")";
    } else if (type == MIDI_EVENT_TYPE_PITCHBEND) {
        //text = "Pitchbend " + n2s( ( ((data2 & 0x7F)<<7) | (data1 & 0x7F) ) - 8192 );
        unsigned char data12[2];
        data12[0] = data1 & 0x7F;
        data12[1] = data2 & 0x7F;
        text = "Pitchbend " + n2s( pitchbendDataToInt_signed(data12) );
    } else if (type == MIDI_EVENT_TYPE_PROGRAM) {
        text = ("Program " + n2s(data1));
        if (bankMSB >= 0) {
            if (bankLSB >= 0) {
                text = ("Prog " + n2s(data1) + " Bank "
                        + n2s( ((bankMSB & 0x7F)<<7)
                               | (bankLSB & 0x7F) ));
            }
        }
    }
    text = "Ch " + n2s(channel+1) + " " + text;
    return text;
}


QString midiNoteName(int note)
{
    QString name;
    if ( (note >= 0) && (note <= 127) ) {
        name = QString(MIDI_NOTE_NAMES[note]);
    }
    return name;
}
