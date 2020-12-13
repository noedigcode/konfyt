/******************************************************************************
 *
 * Copyright 2020 Gideon van der Kolf
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

#include "konfytDefines.h"

#include <QString>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

#define MIDI_EVENT_TYPE_NOTEON 0x90
#define MIDI_EVENT_TYPE_NOTEOFF 0x80
#define MIDI_EVENT_TYPE_POLY_AFTERTOUCH 0xA0
#define MIDI_EVENT_TYPE_CC 0xB0
#define MIDI_EVENT_TYPE_PROGRAM 0xC0
#define MIDI_EVENT_TYPE_AFTERTOUCH 0xD0
#define MIDI_EVENT_TYPE_PITCHBEND 0xE0
#define MIDI_EVENT_TYPE_SYSTEM 0xF0

#define MIDI_MSG_ALL_NOTES_OFF 0x7B

#define MIDI_CC_BANK_MSB 0
#define MIDI_CC_BANK_LSB 32
#define MIDI_CC_ALL_NOTES_OFF 123

#define MIDI_PITCHBEND_ZERO 8192
#define MIDI_PITCHBEND_MAX 16383

#define MIDI_PITCHBEND_SIGNED_MIN -8192
#define MIDI_PITCHBEND_SIGNED_MAX 8191

#define MIDI_TYPE_FROM_BUFFER(x) x[0]&0xF0
#define MIDI_CHANNEL_FROM_BUFFER(x) x[0]&0x0F
#define MIDI_DATA1_FROM_BUFFER(x) x[1]
#define MIDI_DATA2_FROM_BUFFER(x) x[2]

#define MIDI_DATA_MAX_SIZE 64

#define MIDI_SUSTAIN_THRESH 63

#define XML_MIDIEVENT "midiEvent"
#define XML_MIDIEVENT_TYPE "type"
#define XML_MIDIEVENT_CHANNEL "channel"
#define XML_MIDIEVENT_DATA1 "data1"
#define XML_MIDIEVENT_DATA2 "data2"
#define XML_MIDIEVENT_DATAHEX "dataHex"
#define XML_MIDIEVENT_BANKMSB "bankMSB"
#define XML_MIDIEVENT_BANKLSB "bankLSB"

#define XML_MIDI_SEND_ITEM "midiSendItem"
#define XML_MIDI_SEND_ITEM_DESCRIPTION "description"

int pitchbendDataToSignedInt(unsigned char *data12);
int pitchbendDataToSignedInt(int data1, int data2);
void pitchbendSignedIntToData(int p, unsigned char *data12);
int hashMidiEventToInt(int type, int channel, int data1, int bankMSB, int bankLSB);
QString midiNoteName(int note);
QString midiEventToString(int type, int channel, int data1, int bankMSB, int bankLSB);
QString midiEventToString(int type, int channel, int data1, int data2, int bankMSB, int bankLSB);


struct KonfytMidiEvent
{
private:
    int mType = MIDI_EVENT_TYPE_NOTEON; // Status byte without channel (i.e. same as channel=0)
    int mDatasize = 0;
    unsigned char mData[MIDI_DATA_MAX_SIZE];

public:
    int channel = 0;
    int bankMSB = -1;
    int bankLSB = -1;

    KonfytMidiEvent()
    {
        mData[0] = 0;
        mData[1] = 0;
    }
    KonfytMidiEvent(const unsigned char* buffer, int size);

    /* Returns the number of data bytes, excluding the type/channel byte. */
    int dataSize() const;
    /* Returns buffer of data bytes, excluding type/channel byte. */
    unsigned char* data();

    /* Sets first data byte, but does not change dataSize. */
    void setData1(unsigned char value);
    void setNote(uint8_t note);
    void setVelocity(uint8_t vel);
    /* Sets second data byte, but does not change dataSize. */
    void setData2(unsigned char value);

    /* Sets data and dataSize by converting characters in a string to
     * 8-bit hex values. Any spaces are removed, thus hex values in the string
     * may be separated by spaces for readibility. */
    void setDataFromHexString(QString hexString);

    /* Returns a string containing data as space separated 8-bit (2 character)
     * hex values. */
    QString dataToHexString() const;

    int type() const;

    /* Set type as well as appropriate data size. */
    void setType(int t);
    void setNoteOn(uint8_t note, uint8_t velocity);
    void setNoteOff(uint8_t note, uint8_t velocity);
    void setCC(uint8_t cc, uint8_t value);
    void setProgram(uint8_t program);
    void setSysEx(const unsigned char* bytes, int size);

    int note() const;
    int velocity() const;
    int data1() const;
    int data2() const;
    int program() const;

    /* Returns the number of bytes required to write this MIDI event to a buffer,
     * which includes the type/channel byte plus the data bytes. */
    int bufferSizeRequired() const;
    int toBuffer(unsigned char* buffer) const;
    void msbToBuffer(unsigned char* buffer) const;
    void lsbToBuffer(unsigned char* buffer) const;

    // Return pitchbend value between -8192 and 8191
    int pitchbendValueSigned() const;
    void setPitchbend(int value);

    QString toString() const;

    void writeToXMLStream(QXmlStreamWriter* w) const;
    /* Reads from XML stream and returns error messages. */
    QString readFromXmlStream(QXmlStreamReader* r);

};


struct MidiSendItem
{
    QString description;
    KonfytMidiEvent midiEvent;
    QString filename; // Not saved with event, only used when loading/displaying in GUI.

    QString toString() const;

    void writeToXMLStream(QXmlStreamWriter* w) const;
    /* Reads from XML stream and returns error messages. */
    QString readFromXmlStream(QXmlStreamReader* r);
};


#endif // KONFYTMIDI_H
