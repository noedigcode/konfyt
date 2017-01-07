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
        text = ("Noteoff " + n2s(data1));
    } else if (type == MIDI_EVENT_TYPE_NOTEON) {
        text = ("Noteon " + n2s(data1));
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
        text = ("Noteoff " + n2s(data1) + ", " + n2s(data2));
    } else if (type == MIDI_EVENT_TYPE_NOTEON) {
        text = ("Noteon " + n2s(data1) + ", " + n2s(data2));
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

