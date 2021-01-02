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

#include "konfytMidi.h"

const char* MIDI_NOTE_NAMES[] = {
    "C-1", "C#-1", "D-1", "D#-1", "E-1", "F-1", "F#-1", "G-1", "G#-1", "A-1", "A#-1", "B-1",
    "C0", "C#0", "D0", "D#0", "E0", "F0", "F#0", "G0", "G#0", "A0", "A#0", "B0",
    "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1", "G#1", "A1", "A#1", "B1",
    "C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2", "G#2", "A2", "A#2", "B2",
    "C3", "C#3", "D3", "D#3", "E3", "F3", "F#3", "G3", "G#3", "A3", "A#3", "B3",
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4",
    "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5",
    "C6", "C#6", "D6", "D#6", "E6", "F6", "F#6", "G6", "G#6", "A6", "A#6", "B6",
    "C7", "C#7", "D7", "D#7", "E7", "F7", "F#7", "G7", "G#7", "A7", "A#7", "B7",
    "C8", "C#8", "D8", "D#8", "E8", "F8", "F#8", "G8", "G#8", "A8", "A#8", "B8",
    "C9", "C#9", "D9", "D#9", "E9", "F9", "F#9", "G9", "G#9"
};

int pitchbendDataToSignedInt(unsigned char *data12)
{
    return ( ((data12[1] & 0x7F)<<7) | (data12[0] & 0x7F) ) - 8192;
}

int pitchbendDataToSignedInt(int data1, int data2)
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
    } else {
        text = "Unhandled MIDI event type";
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
        unsigned char data12[2];
        data12[0] = data1 & 0x7F;
        data12[1] = data2 & 0x7F;
        text = "Pitchbend " + n2s( pitchbendDataToSignedInt(data12) );
    } else if (type == MIDI_EVENT_TYPE_PROGRAM) {
        text = ("Program " + n2s(data1));
        if (bankMSB >= 0) {
            if (bankLSB >= 0) {
                text = ("Prog " + n2s(data1) + " Bank "
                        + n2s( midiBanksToInt(bankMSB, bankLSB) ));
            }
        }
    } else {
        text = "Unhandled MIDI event type";
    }
    text = "Ch " + n2s(channel+1) + " " + text;
    return text;
}

int midiBanksToInt(int bankMSB, int bankLSB)
{
    return ( ((bankMSB & 0x7F)<<7) | (bankLSB & 0x7F) );
}

int midiBankMSB(int bankCombined)
{
    return ( (bankCombined >> 7) & 0x7F );
}

int midiBankLSB(int bankCombined)
{
    return ( bankCombined & 0x7F );
}

QString midiNoteName(int note)
{
    QString name;
    if ( (note >= 0) && (note <= 127) ) {
        name = QString(MIDI_NOTE_NAMES[note]);
    }
    return name;
}

KonfytMidiEvent::KonfytMidiEvent(const unsigned char *buffer, int size)
{
    mType = MIDI_TYPE_FROM_BUFFER(buffer);
    channel = MIDI_CHANNEL_FROM_BUFFER(buffer);
    mDatasize = size - 1;
    memcpy(mData, buffer+1, size-1);
}

int KonfytMidiEvent::dataSize() const
{
    return mDatasize;
}

unsigned char *KonfytMidiEvent::data()
{
    return mData;
}

void KonfytMidiEvent::setData1(unsigned char value)
{
    mData[0] = value;
}

void KonfytMidiEvent::setNote(uint8_t note)
{
    mData[0] = note;
}

void KonfytMidiEvent::setVelocity(uint8_t vel)
{
    mData[1] = vel;
}

void KonfytMidiEvent::setData2(unsigned char value)
{
    mData[2] = value;
}

void KonfytMidiEvent::setDataFromHexString(QString hexString)
{
    hexString = hexString.replace(" ", "");
    mDatasize = 0;
    for (int i=1; i < hexString.count(); i += 2) {
        uint8_t hexVal = hexString.mid(i-1, 2).toInt(nullptr, 16);
        mData[mDatasize] = hexVal;
        mDatasize++;
    }
}

QString KonfytMidiEvent::dataToHexString() const
{
    QString ret;
    for (int i=0; i < mDatasize; i++) {
        if (i > 0) { ret += " "; }
        ret += QString("%1").arg(mData[i], 2, 16, QChar('0'));
    }
    return ret.toUpper();
}

int KonfytMidiEvent::type() const
{
    return mType;
}

void KonfytMidiEvent::setType(int t)
{
    mType = t;
    switch (t) {
    case MIDI_EVENT_TYPE_NOTEON:
    case MIDI_EVENT_TYPE_NOTEOFF:
    case MIDI_EVENT_TYPE_CC:
    case MIDI_EVENT_TYPE_PITCHBEND:
        mDatasize = 2;
        break;
    case MIDI_EVENT_TYPE_PROGRAM:
        mDatasize = 1;
        break;
    }
}

void KonfytMidiEvent::setNoteOn(uint8_t note, uint8_t velocity)
{
    mType = MIDI_EVENT_TYPE_NOTEON;
    mData[0] = note;
    mData[1] = velocity;
    mDatasize = 2;
}

void KonfytMidiEvent::setNoteOff(uint8_t note, uint8_t velocity)
{
    mType = MIDI_EVENT_TYPE_NOTEOFF;
    mData[0] = note;
    mData[1] = velocity;
    mDatasize = 2;
}

void KonfytMidiEvent::setCC(uint8_t cc, uint8_t value)
{
    mType = MIDI_EVENT_TYPE_CC;
    mData[0] = cc;
    mData[1] = value;
    mDatasize = 2;
}

void KonfytMidiEvent::setProgram(uint8_t program)
{
    mType = MIDI_EVENT_TYPE_PROGRAM;
    mData[0] = program;
    mDatasize = 1;
}

void KonfytMidiEvent::setSysEx(const unsigned char *bytes, int size)
{
    mType = MIDI_EVENT_TYPE_SYSTEM;
    channel = 0;
    memcpy(mData, bytes, size);
    mDatasize = size;
}

int KonfytMidiEvent::note() const { return mData[0]; }

int KonfytMidiEvent::velocity() const { return mData[1]; }

int KonfytMidiEvent::data1() const { return mData[0]; }

int KonfytMidiEvent::data2() const { return mData[1]; }

int KonfytMidiEvent::program() const { return mData[0]; }

int KonfytMidiEvent::bufferSizeRequired() const {
    return mDatasize + 1;
}

int KonfytMidiEvent::toBuffer(unsigned char *buffer) const {
    buffer[0] = mType | channel;
    memcpy(buffer+1, mData, mDatasize);
    return mDatasize + 1;
}

void KonfytMidiEvent::msbToBuffer(unsigned char *buffer) const {
    buffer[0] = MIDI_EVENT_TYPE_CC | channel;
    buffer[1] = MIDI_CC_BANK_MSB;
    buffer[2] = bankMSB;
}

void KonfytMidiEvent::lsbToBuffer(unsigned char *buffer) const {
    buffer[0] = MIDI_EVENT_TYPE_CC | channel;
    buffer[1] = MIDI_CC_BANK_LSB;
    buffer[2] = bankLSB;
}

int KonfytMidiEvent::pitchbendValueSigned() const
{
    return pitchbendDataToSignedInt(mData[0], mData[1]);
}

void KonfytMidiEvent::setPitchbend(int value)
{
    mType = MIDI_EVENT_TYPE_PITCHBEND;
    pitchbendSignedIntToData(value, mData);
    mDatasize = 2;
}

QString KonfytMidiEvent::toString() const
{
    if (mType == MIDI_EVENT_TYPE_SYSTEM) {
        QString text = "SYSEX " + dataToHexString();
        return text;
    } else {
        return midiEventToString(mType, channel, mData[0], mData[1], bankMSB, bankLSB);
    }
}

void KonfytMidiEvent::writeToXMLStream(QXmlStreamWriter *w) const
{
    w->writeStartElement(XML_MIDIEVENT);

    w->writeTextElement(XML_MIDIEVENT_TYPE, n2s(type()));
    w->writeTextElement(XML_MIDIEVENT_CHANNEL, n2s(channel));
    if (dataSize() > 2) {
        w->writeTextElement(XML_MIDIEVENT_DATAHEX, dataToHexString());
    } else {
        w->writeTextElement(XML_MIDIEVENT_DATA1, n2s(data1()));
        w->writeTextElement(XML_MIDIEVENT_DATA2, n2s(data2()));
    }
    w->writeTextElement(XML_MIDIEVENT_BANKMSB, n2s(bankMSB));
    w->writeTextElement(XML_MIDIEVENT_BANKLSB, n2s(bankLSB));

    w->writeEndElement();
}

QString KonfytMidiEvent::readFromXmlStream(QXmlStreamReader *r)
{
    QString error;
    while (r->readNextStartElement()) {
        if (r->name() == XML_MIDIEVENT_TYPE) {
            setType( r->readElementText().toInt() );
        } else if (r->name() == XML_MIDIEVENT_CHANNEL) {
            channel = r->readElementText().toInt();
        } else if (r->name() == XML_MIDIEVENT_DATA1) {
            setData1( r->readElementText().toInt() );
        } else if (r->name() == XML_MIDIEVENT_DATA2) {
            setData2( r->readElementText().toInt() );
        } else if (r->name() == XML_MIDIEVENT_DATAHEX) {
            setDataFromHexString( r->readElementText() );
        } else if (r->name() == XML_MIDIEVENT_BANKMSB) {
            bankMSB = r->readElementText().toInt();
        } else if (r->name() == XML_MIDIEVENT_BANKLSB) {
            bankLSB = r->readElementText().toInt();
        } else {
            error += QString("KonfytMidiEvent::readFromXmlStream:"
                             "Unrecognized MIDI event element: %1\n")
                    .arg( r->name().toString() );
            r->skipCurrentElement();
        }
    }
    return error;
}

QString MidiSendItem::toString() const
{
    if (description.isEmpty()) {
        return midiEvent.toString();
    } else {
        return QString("%1 (%2)").arg(description, midiEvent.toString());
    }
}

void MidiSendItem::writeToXMLStream(QXmlStreamWriter *w) const
{
    w->writeStartElement(XML_MIDI_SEND_ITEM);

    w->writeTextElement(XML_MIDI_SEND_ITEM_DESCRIPTION, description);
    midiEvent.writeToXMLStream(w);

    w->writeEndElement();
}

QString MidiSendItem::readFromXmlStream(QXmlStreamReader *r)
{
    QString error;
    while (r->readNextStartElement()) {
        if (r->name() == XML_MIDI_SEND_ITEM) {
            error += readFromXmlStream(r);
        } else if (r->name() == XML_MIDI_SEND_ITEM_DESCRIPTION) {
            description = r->readElementText();
        } else if (r->name() == XML_MIDIEVENT) {
            error += midiEvent.readFromXmlStream(r);
        } else {
            error += QString("MidiSendItem::readFromXmlStream:"
                             "Unrecognized XML element: %1\n")
                    .arg( r->name().toString() );
        }
    }
    return error;
}

