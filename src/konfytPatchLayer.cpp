/******************************************************************************
 *
 * Copyright 2023 Gideon van der Kolf
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

#include "konfytPatchLayer.h"

void KonfytPatchLayer::setErrorMessage(QString msg)
{
    mErrorMessage = msg;
}

bool KonfytPatchLayer::hasError() const
{
    return !mErrorMessage.isEmpty();
}

QString KonfytPatchLayer::errorMessage() const
{
    return mErrorMessage;
}

QList<KonfytMidiEvent> KonfytPatchLayer::getMidiSendListEvents()
{
    QList<KonfytMidiEvent> events;
    foreach (MidiSendItem item, midiSendList) {
        events.append(item.midiEvent);
    }
    return events;
}

void KonfytPatchLayer::writeToXmlStream(QXmlStreamWriter *stream) const
{
    switch (mLayerType) {
    case KonfytPatchLayer::TypeUninitialized:
        break;
    case KonfytPatchLayer::TypeSoundfontProgram:
        writeSoundfontDataToXmlStream(stream);
        break;
    case KonfytPatchLayer::TypeSfz:
        writeSfzDataToXmlStream(stream);
        break;
    case KonfytPatchLayer::TypeMidiOut:
        writeMidiOutDataToXmlStream(stream);
        break;
    case KonfytPatchLayer::TypeAudioIn:
        writeAudioInDataToXmlStream(stream);
        break;
    }
}

void KonfytPatchLayer::readFromXmlStream(QXmlStreamReader *r, QString *errors)
{
    if (r->name() == XML_SF_LAYER) {

        readSoundfontDataFromXmlStream(r, errors);

    } else if (r->name() == XML_SFZ_LAYER) {

        readSfzDataFromXmlStream(r, errors);

    } else if (r->name() == XML_MIDIOUT) {

        readMidiOutDataFromXmlStream(r, errors);

    } else if (r->name() == XML_AUDIOIN) {

        readAudioInDataFromXmlStream(r, errors);

    }
}

QByteArray KonfytPatchLayer::toByteArray()
{
    QByteArray data;
    QXmlStreamWriter stream(&data);
    writeToXmlStream(&stream);
    return data;
}

void KonfytPatchLayer::fromByteArray(QByteArray data)
{
    QXmlStreamReader r(data);

    while (r.readNextStartElement()) {
        readFromXmlStream(&r);
        break;
    }
}

void KonfytPatchLayer::writeSoundfontDataToXmlStream(QXmlStreamWriter *stream) const
{
    stream->writeStartElement(XML_SF_LAYER);

    KonfytSoundPreset p = soundfontData.program;
    stream->writeTextElement(XML_SF_FILENAME, soundfontData.parentSoundfont);
    stream->writeTextElement(XML_SF_BANK, n2s(p.bank));
    stream->writeTextElement(XML_SF_PROGRAM, n2s(p.program));
    stream->writeTextElement(XML_SF_NAME, p.name);
    stream->writeTextElement(XML_SF_GAIN, n2s(mGain));
    stream->writeTextElement(XML_SF_BUS, n2s(mBusIdInProject));
    stream->writeTextElement(XML_SF_SOLO, bool2str(mSolo));
    stream->writeTextElement(XML_SF_MUTE, bool2str(mMute));
    stream->writeTextElement(XML_SF_MIDI_IN, n2s(mMidiInPortIdInProject));
    stream->writeTextElement(XML_RESET_OPTION,
                             konfytResetToString(mResetOption));

    // Midi filter
    mMidiFilter.writeToXMLStream(stream);
    // Script
    writeScriptToXmlStream(stream);

    stream->writeEndElement(); // Layer
}

void KonfytPatchLayer::readSoundfontDataFromXmlStream(QXmlStreamReader *r, QString *errors)
{
    LayerSoundfontData sfData;
    int bus = 0;
    int midiIn = 0;
    float gain = 1.0;
    bool solo = false;
    bool mute = false;
    KonfytMidiFilter midiFilter;
    LayerScriptData script;
    KonfytReset resetOption = KonfytReset::Inherit;

    while (r->readNextStartElement()) { // layer properties

        if (r->name() == XML_SF_FILENAME) {
            sfData.parentSoundfont = r->readElementText();
        } else if (r->name() == XML_SF_BANK) {
            sfData.program.bank = r->readElementText().toInt();
        } else if (r->name() == XML_SF_PROGRAM) {
            sfData.program.program = r->readElementText().toInt();
        } else if (r->name() == XML_SF_NAME) {
            sfData.program.name = r->readElementText();
        } else if (r->name() == XML_SF_GAIN) {
            gain = r->readElementText().toFloat();
        } else if (r->name() == XML_SF_SOLO) {
            solo = (r->readElementText() == "1");
        } else if (r->name() == XML_SF_MUTE) {
            mute = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER) {
            midiFilter.readFromXMLStream(r);
        } else if (r->name() == XML_SF_BUS) {
            bus = r->readElementText().toInt();
        } else if (r->name() == XML_SF_MIDI_IN) {
            midiIn = r->readElementText().toInt();
        } else if (r->name() == XML_SCRIPT) {
            script = readScriptFromXmlStream(r, errors);
        } else if (r->name() == XML_RESET_OPTION) {
            resetOption = konfytResetFromString(r->readElementText(), resetOption);
        } else {
            appendError(errors, "Unrecognized sfLayer element: " + r->name().toString() );
            r->skipCurrentElement();
        }

    }

    initLayer(sfData);
    setBusIdInProject(bus);
    setMidiInPortIdInProject(midiIn);
    setGain(gain);
    setSolo(solo);
    setMute(mute);
    setMidiFilter(midiFilter);
    setScript(script.content);
    setScriptEnabled(script.enabled);
    setPassMidiThrough(script.passMidiThrough);
    setResetOption(resetOption);
}

void KonfytPatchLayer::writeSfzDataToXmlStream(QXmlStreamWriter *stream) const
{
    stream->writeStartElement(XML_SFZ_LAYER);

    stream->writeTextElement(XML_SFZ_NAME, mName);
    stream->writeTextElement(XML_SFZ_PATH, sfzData.path);
    stream->writeTextElement(XML_SFZ_GAIN, n2s(mGain));
    stream->writeTextElement(XML_SFZ_BUS, n2s(mBusIdInProject) );
    stream->writeTextElement(XML_SFZ_SOLO, bool2str(mSolo));
    stream->writeTextElement(XML_SFZ_MUTE, bool2str(mMute));
    stream->writeTextElement(XML_SFZ_MIDI_IN, n2s(mMidiInPortIdInProject));
    stream->writeTextElement(XML_RESET_OPTION,
                             konfytResetToString(mResetOption));

    // Midi filter
    midiFilter().writeToXMLStream(stream);
    // Script
    writeScriptToXmlStream(stream);

    stream->writeEndElement(); // Layer
}

void KonfytPatchLayer::readSfzDataFromXmlStream(QXmlStreamReader *r, QString *errors)
{
    LayerSfzData p;
    int bus = 0;
    int midiIn = 0;
    QString name;
    float gain = 1.0;
    bool solo = false;
    bool mute = false;
    KonfytMidiFilter midiFilter;
    LayerScriptData script;
    KonfytReset resetOption = KonfytReset::Inherit;

    while (r->readNextStartElement()) {

        if (r->name() == XML_SFZ_NAME) {
            name = r->readElementText();
        } else if (r->name() == XML_SFZ_PATH) {
            p.path = r->readElementText();
        } else if (r->name() == XML_SFZ_GAIN) {
            gain = r->readElementText().toFloat();
        } else if (r->name() == XML_SFZ_BUS) {
            bus = r->readElementText().toInt();
        } else if (r->name() == XML_SFZ_MIDI_IN) {
            midiIn = r->readElementText().toInt();
        } else if (r->name() == XML_SFZ_SOLO) {
            solo = (r->readElementText() == "1");
        } else if (r->name() == XML_SFZ_MUTE) {
            mute = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER) {
            midiFilter.readFromXMLStream(r);
        } else if (r->name() == XML_SCRIPT) {
            script = readScriptFromXmlStream(r, errors);
        } else if (r->name() == XML_RESET_OPTION) {
            resetOption = konfytResetFromString(r->readElementText(), resetOption);
        }  else {
            appendError(errors, "Unrecognized sfzLayer element: " + r->name().toString() );
            r->skipCurrentElement();
        }

    }

    initLayer(p);
    setName(name);
    setBusIdInProject(bus);
    setMidiInPortIdInProject(midiIn);
    setGain(gain);
    setSolo(solo);
    setMute(mute);
    setMidiFilter(midiFilter);
    setScript(script.content);
    setScriptEnabled(script.enabled);
    setPassMidiThrough(script.passMidiThrough);
    setResetOption(resetOption);
}

void KonfytPatchLayer::writeMidiOutDataToXmlStream(QXmlStreamWriter *stream) const
{
    stream->writeStartElement(XML_MIDIOUT);

    stream->writeTextElement(XML_MIDIOUT_PORT,
                             n2s( midiOutputPortData.portIdInProject ));
    stream->writeTextElement(XML_MIDIOUT_SOLO, bool2str(mSolo));
    stream->writeTextElement(XML_MIDIOUT_MUTE, bool2str(mMute));
    stream->writeTextElement(XML_MIDIOUT_MIDI_IN, n2s(mMidiInPortIdInProject));
    stream->writeTextElement(XML_RESET_OPTION,
                             konfytResetToString(mResetOption));

    // Midi filter
    midiFilter().writeToXMLStream(stream);
    // Script
    writeScriptToXmlStream(stream);

    // MIDI Send list
    if (midiSendList.count()) {
        stream->writeStartElement(XML_MIDISENDLIST);

        foreach (MidiSendItem item, midiSendList) {
            item.writeToXMLStream(stream);
        }

        stream->writeEndElement();
    }

    stream->writeEndElement(); // Layer
}

void KonfytPatchLayer::readMidiOutDataFromXmlStream(QXmlStreamReader *r, QString *errors)
{
    LayerMidiOutData mp;
    int midiIn = 0;
    QList<MidiSendItem> midiSendItems;
    bool solo = false;
    bool mute = false;
    KonfytMidiFilter midiFilter;
    LayerScriptData script;
    KonfytReset resetOption = KonfytReset::Inherit;

    while (r->readNextStartElement()) { // port
        if (r->name() == XML_MIDIOUT_PORT) {
            mp.portIdInProject = r->readElementText().toInt();
        } else if (r->name() == XML_MIDIOUT_SOLO) {
            solo = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIOUT_MUTE) {
            mute = (r->readElementText() == "1");
        } else if (r->name() == XML_MIDIFILTER) {
            midiFilter.readFromXMLStream(r);
        } else if (r->name() == XML_SCRIPT) {
            script = readScriptFromXmlStream(r, errors);
        } else if (r->name() == XML_RESET_OPTION) {
            resetOption = konfytResetFromString(r->readElementText(), resetOption);
        }  else if (r->name() == XML_MIDIOUT_MIDI_IN) {
            midiIn = r->readElementText().toInt();
        } else if (r->name() == XML_MIDISENDLIST) {
            while (r->readNextStartElement()) {
                if (r->name() == MidiSendItem::XML_MIDI_SEND_ITEM) {
                    MidiSendItem item;
                    QString error = item.readFromXmlStream(r);
                    if (!error.isEmpty()) {
                        appendError(errors, "MidiSendItem read error: " + error);
                    }
                    midiSendItems.append(item);
                } else {
                    appendError(errors, "Unrecognized MIDI send list element: " + r->name().toString());
                    r->skipCurrentElement();
                }
            }
        } else {
            appendError(errors, "Unrecognized midiOutputPortLayer element: " + r->name().toString() );
            r->skipCurrentElement();
        }
    }

    // Add new midi port
    initLayer(mp);
    setMidiInPortIdInProject(midiIn);
    midiSendList = midiSendItems;
    setSolo(solo);
    setMute(mute);
    setMidiFilter(midiFilter);
    setScript(script.content);
    setScriptEnabled(script.enabled);
    setPassMidiThrough(script.passMidiThrough);
    setResetOption(resetOption);
}

void KonfytPatchLayer::writeAudioInDataToXmlStream(QXmlStreamWriter *stream) const
{
    stream->writeStartElement(XML_AUDIOIN);

    stream->writeTextElement(XML_AUDIOIN_NAME, mName);
    stream->writeTextElement(XML_AUDIOIN_PORT,
                             n2s(audioInPortData.portIdInProject));
    stream->writeTextElement(XML_AUDIOIN_GAIN, n2s(mGain));
    stream->writeTextElement(XML_AUDIOIN_BUS, n2s(mBusIdInProject));
    stream->writeTextElement(XML_AUDIOIN_SOLO, bool2str(mSolo));
    stream->writeTextElement(XML_AUDIOIN_MUTE, bool2str(mMute));
    stream->writeTextElement(XML_RESET_OPTION,
                             konfytResetToString(mResetOption));

    stream->writeEndElement(); // Layer
}

void KonfytPatchLayer::readAudioInDataFromXmlStream(QXmlStreamReader *r, QString *errors)
{
    LayerAudioInData a;
    int bus = 0;
    QString name;
    float gain = 1.0;
    bool solo = false;
    bool mute = false;
    KonfytReset resetOption = KonfytReset::Inherit;

    while (r->readNextStartElement()) {
        if (r->name() == XML_AUDIOIN_NAME) {
            name = r->readElementText();
        } else if (r->name() == XML_AUDIOIN_PORT) {
            a.portIdInProject = r->readElementText().toInt();
        } else if (r->name() == XML_AUDIOIN_GAIN) {
            gain = r->readElementText().toFloat();
        } else if (r->name() == XML_AUDIOIN_BUS) {
            bus = r->readElementText().toInt();
        } else if (r->name() == XML_AUDIOIN_SOLO) {
            solo = (r->readElementText() == "1");
        } else if (r->name() == XML_AUDIOIN_MUTE) {
            mute = (r->readElementText() == "1");
        } else if (r->name() == XML_RESET_OPTION) {
            resetOption = konfytResetFromString(r->readElementText(), resetOption);
        }  else {
            appendError(errors,
                        "Unrecognized audioInPortLayer element: " + r->name().toString() );
            r->skipCurrentElement();
        }
    }

    initLayer(a);
    setName(name);
    setBusIdInProject(bus);
    setGain(gain);
    setSolo(solo);
    setMute(mute);
    setResetOption(resetOption);
}

void KonfytPatchLayer::writeScriptToXmlStream(QXmlStreamWriter *stream) const
{
    stream->writeStartElement(XML_SCRIPT);

    stream->writeTextElement(XML_SCRIPT_CONTENT, mScript);
    stream->writeTextElement(XML_SCRIPT_ENABLED,
                             QVariant(mScriptEnabled).toString());
    stream->writeTextElement(XML_SCRIPT_PASS_MIDI_THROUGH,
                             QVariant(mPassMidiThrough).toString());

    stream->writeEndElement(); // script
}

void KonfytPatchLayer::appendError(QString *errorString, QString msg)
{
    if (errorString) {
        if (!errorString->isEmpty()) { errorString->append("\n"); }
        errorString->append(msg);
    }
}

KonfytPatchLayer::LayerScriptData KonfytPatchLayer::readScriptFromXmlStream(
        QXmlStreamReader *r, QString* /*errors*/)
{
    LayerScriptData ret;

    while (r->readNextStartElement()) {
        if (r->name() == XML_SCRIPT_CONTENT) {
            ret.content = r->readElementText();
        } else if (r->name() == XML_SCRIPT_ENABLED) {
            ret.enabled = QVariant(r->readElementText()).toBool();
        } else if (r->name() == XML_SCRIPT_PASS_MIDI_THROUGH) {
            ret.passMidiThrough = QVariant(r->readElementText()).toBool();
        } else {
            r->skipCurrentElement();
        }
    }

    return ret;
}

KonfytPatchLayer::KonfytPatchLayer()
{
    gainMidiCtrl.setValue(mGain * 127.0);
}

void KonfytPatchLayer::initLayer(LayerSoundfontData newLayerData)
{
    mLayerType = TypeSoundfontProgram;
    soundfontData = newLayerData;
    mName = soundfontData.parentSoundfont + "/" + soundfontData.program.name;
    mMidiFilter.passProg = false;
}

void KonfytPatchLayer::initLayer(LayerSfzData newLayerData)
{
    mLayerType = TypeSfz;
    sfzData = newLayerData;
    mName = "SFZ";
    mMidiFilter.passProg = false;
}

void KonfytPatchLayer::initLayer(LayerMidiOutData newLayerData)
{
    mLayerType = TypeMidiOut;
    midiOutputPortData = newLayerData;
    mName = "MIDI Out Port";
    mMidiFilter.passProg = true;
}

void KonfytPatchLayer::initLayer(LayerAudioInData newLayerData)
{
    mLayerType = TypeAudioIn;
    audioInPortData = newLayerData;
    mName = "Audio In Port";
}

QString KonfytPatchLayer::name() const
{
    return mName;
}

void KonfytPatchLayer::setName(QString name)
{
    mName = name;
}

float KonfytPatchLayer::gain() const
{
    return mGain;
}

void KonfytPatchLayer::setGain(float gain)
{
    mGain = gain;
    gainMidiCtrl.setValue(gain * 127.0);
}

/* Change the gain relatively by the specified MIDI value, ignoring MIDI pickup
 * range. */
void KonfytPatchLayer::addGainRelativeMidiValue(int value)
{
    value += gainMidiCtrl.value();
    value = qMax(0, qMin(127, value));

    gainMidiCtrl.setValue(value);
    mGain = gainMidiCtrl.value() / 127.0;
}

/* Set gain from MIDI, taking MIDI pickup range into account. */
void KonfytPatchLayer::setGainByMidi(int value)
{
    if (gainMidiCtrl.midiInput(value)) {
        mGain = gainMidiCtrl.value() / 127.0;
    }
}

void KonfytPatchLayer::setGainMidiPickupRange(int range)
{
    gainMidiCtrl.pickupRange = range;
}

int KonfytPatchLayer::gainMidiPickupRange()
{
    return gainMidiCtrl.pickupRange;
}

void KonfytPatchLayer::setSolo(bool isSolo)
{
    mSolo = isSolo;
}

void KonfytPatchLayer::setMute(bool isMute)
{
    mMute = isMute;
}

bool KonfytPatchLayer::isSolo() const
{
    return mSolo;
}

bool KonfytPatchLayer::isMute() const
{
    return mMute;
}

int KonfytPatchLayer::busIdInProject() const
{
    return mBusIdInProject;
}

void KonfytPatchLayer::setBusIdInProject(int bus)
{
    mBusIdInProject = bus;
}

int KonfytPatchLayer::midiInPortIdInProject() const
{
    return mMidiInPortIdInProject;
}

void KonfytPatchLayer::setMidiInPortIdInProject(int port)
{
    mMidiInPortIdInProject = port;
}

void KonfytPatchLayer::setResetOption(KonfytReset option)
{
    mResetOption = option;
}

KonfytReset KonfytPatchLayer::getResetOption()
{
    return mResetOption;
}

void KonfytPatchLayer::createResetSnapshot()
{
    mResetSnapshot.valid = true;
    mResetSnapshot.gain = mGain;
    mResetSnapshot.solo = mSolo;
    mResetSnapshot.mute = mMute;
}

void KonfytPatchLayer::restoreResetSnapshotIfAllowed(KonfytReset inheritedOption)
{
    KonfytReset resultingOption = konfytResetFromInherits(
                                        {mResetOption, inheritedOption},
                                        KonfytReset::NoReset);

    if (resultingOption != KonfytReset::Reset) { return; }

    if (!mResetSnapshot.valid) { return; }

    // Apply reset
    setGain(mResetSnapshot.gain);
    setSolo(mResetSnapshot.solo);
    setMute(mResetSnapshot.mute);
}

QString KonfytPatchLayer::script() const
{
    return mScript;
}

void KonfytPatchLayer::setScript(QString script)
{
    mScript = script;
}

bool KonfytPatchLayer::isScriptEnabled() const
{
    return mScriptEnabled;
}

void KonfytPatchLayer::setScriptEnabled(bool enable)
{
    mScriptEnabled = enable;
}

bool KonfytPatchLayer::isPassMidiThrough() const
{
    return mPassMidiThrough;
}

void KonfytPatchLayer::setPassMidiThrough(bool enable)
{
    mPassMidiThrough = enable;
}

KonfytMidiFilter KonfytPatchLayer::midiFilter() const
{
    return mMidiFilter;
}

void KonfytPatchLayer::setMidiFilter(KonfytMidiFilter midiFilter)
{
    mMidiFilter = midiFilter;
}

KonfytPatchLayer::LayerType KonfytPatchLayer::layerType() const
{
    return mLayerType;
}

/* Shorthand for determining of layer type has MIDI input. */
bool KonfytPatchLayer::hasMidiInput() const
{
    return    (mLayerType == TypeMidiOut)
           || (mLayerType == TypeSfz)
           || (mLayerType == TypeSoundfontProgram);
}
