/******************************************************************************
 *
 * Copyright 2019 Gideon van der Kolf
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

#ifndef KONFYTMIDI_H
#define KONFYTMIDI_H

#include <QString>
#include "konfytDefines.h"

#define MIDI_EVENT_TYPE_NOTEON 0x90
#define MIDI_EVENT_TYPE_NOTEOFF 0x80
#define MIDI_EVENT_TYPE_POLY_AFTERTOUCH 0xA0
#define MIDI_EVENT_TYPE_CC 0xB0
#define MIDI_EVENT_TYPE_PROGRAM 0xC0
#define MIDI_EVENT_TYPE_AFTERTOUCH 0xD0
#define MIDI_EVENT_TYPE_PITCHBEND 0xE0
#define MIDI_EVENT_TYPE_SYSTEM 0xF0

#define MIDI_MSG_ALL_NOTES_OFF 0x7B

#define MIDI_CC_MSB 0
#define MIDI_CC_LSB 32
#define MIDI_CC_ALL_NOTES_OFF 123

#define MIDI_PITCHBEND_ZERO 8192
#define MIDI_PITCHBEND_MAX 16383

#define MIDI_PITCHBEND_SIGNED_MIN -8192
#define MIDI_PITCHBEND_SIGNED_MAX 8191

#define MIDI_TYPE_FROM_BUFFER(x) x[0]&0xF0
#define MIDI_CHANNEL_FROM_BUFFER(x) x[0]&0x0F
#define MIDI_DATA1_FROM_BUFFER(x) x[1]
#define MIDI_DATA2_FROM_BUFFER(x) x[2]

int pitchbendDataToInt_signed(unsigned char *data12);
int pitchbendDataToInt_signed(int data1, int data2);
void pitchbendSignedIntToData(int p, unsigned char *data12);
int hashMidiEventToInt(int type, int channel, int data1, int bankMSB, int bankLSB);
QString midiNoteName(int note);
QString midiEventToString(int type, int channel, int data1, int bankMSB, int bankLSB);
QString midiEventToString(int type, int channel, int data1, int data2, int bankMSB, int bankLSB);


struct KonfytMidiEvent {
    int sourceId = -1;
    int type = MIDI_EVENT_TYPE_NOTEON; // Status bit without channel (i.e. same as channel=0)
    int channel = 0;
    int data1 = 0;
    int data2 = 0;
    int bankMSB = -1;
    int bankLSB = -1;

    KonfytMidiEvent() {}
    KonfytMidiEvent(const unsigned char* buffer, int size) {
        type = MIDI_TYPE_FROM_BUFFER(buffer);
        channel = MIDI_CHANNEL_FROM_BUFFER(buffer);
        data1 = MIDI_DATA1_FROM_BUFFER(buffer);
        if (size >= 3) { data2 = MIDI_DATA2_FROM_BUFFER(buffer); }
        else { data2 = -1; }
        bankMSB = -1;
        bankLSB = -1;
    }

    int toBuffer(unsigned char* buffer) {
        buffer[0] = type | channel;
        buffer[1] = data1;
        if (data2 >= 0) {
            buffer[2] = data2;
            return 3;
        } else return 2;
    }

    int bufferSizeRequired() {
        if ( (type == MIDI_EVENT_TYPE_PROGRAM) || (data2 < 0) ) {
            return 2;
        } else {
            return 3;
        }
    }

    void msbToBuffer(unsigned char* buffer) {
        buffer[0] = MIDI_EVENT_TYPE_CC | channel;
        buffer[1] = MIDI_CC_MSB;
        buffer[2] = bankMSB;
    }

    void lsbToBuffer(unsigned char* buffer) {
        buffer[0] = MIDI_EVENT_TYPE_CC | channel;
        buffer[1] = MIDI_CC_LSB;
        buffer[2] = bankLSB;
    }

    // Return pitchbend value between -8192 and 8191
    int pitchbendValue_signed() const
    {
        return pitchbendDataToInt_signed(data1, data2);
    }

    void setPitchbendData(int value)
    {
        unsigned char data12[2];
        pitchbendSignedIntToData(value, data12);
        data1 = data12[0];
        data2 = data12[1];
    }

    QString toString()
    {
        return midiEventToString(type, channel, data1, data2, bankMSB, bankLSB);
    }

};





#endif // KONFYTMIDI_H
