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

#include <QFileInfo>

void PatchLayer::setErrorMessage(QString msg)
{
    mErrorMessage = msg;
}

bool PatchLayer::hasError() const
{
    return !mErrorMessage.isEmpty();
}

QString PatchLayer::errorMessage() const
{
    return mErrorMessage;
}

QList<KonfytMidiEvent> PatchLayer::getMidiSendListEvents()
{
    QList<KonfytMidiEvent> events;
    foreach (MidiSendItem item, midiSendList) {
        events.append(item.midiEvent);
    }
    return events;
}

Xml PatchLayer::toXml() const
{
    Xml xml;

    switch (mLayerType) {
    case PatchLayer::TypeUninitialized:
        break;
    case PatchLayer::TypeSoundfontProgram:
        xml = soundfontDataToXml();
        break;
    case PatchLayer::TypeSfz:
        xml = sfzDataToXml();
        break;
    case PatchLayer::TypeMidiOut:
        xml = midiOutDataToXml();
        break;
    case PatchLayer::TypeAudioIn:
        xml = audioInDataToXml();
        break;
    }

    return xml;
}

void PatchLayer::readFromXml(Xml xml)
{
    if (xml.name() == XML_SF_LAYER) {

        readSoundfontDataFromXml(xml);

    } else if (xml.name() == XML_SFZ_LAYER) {

        readSfzDataFromXml(xml);

    } else if (xml.name() == XML_MIDIOUT_LAYER) {

        readMidiOutDataFromXml(xml);

    } else if (xml.name() == XML_AUDIOIN_LAYER) {

        readAudioInDataFromXml(xml);

    }
}

QByteArray PatchLayer::toXmlByteArray() const
{
    Xml xml = toXml();
    return xml.toByteArray();
}

Result PatchLayer::fromXmlByteArray(QByteArray data)
{
    Xml xml;
    Xml::Result xmlResult = xml.loadFromData(data);
    if (!xmlResult.ok) {
        return Result::failure(QString("Error loading XML: %1")
                               .arg(xmlResult.toString()));
    }

    readFromXml(xml);

    return Result::success();
}

void PatchLayer::updateName()
{
    switch (mLayerType) {
    case PatchLayer::TypeUninitialized:
        mName = "UNINITIALIZED LAYER";
        break;
    case PatchLayer::TypeSoundfontProgram: {

        QString filename = QFileInfo(soundfontData.soundfontFilePath).fileName();
        mName = QString("%1/%2").arg(filename)
                .arg(soundfontData.program.name);

    } break;
    case PatchLayer::TypeSfz: {

        QFileInfo fi(sfzData.path);
        QString filename = fi.fileName();
        QString parentDir = QFileInfo(fi.path()).fileName();
        if (fi.baseName().toLower() == parentDir.toLower()) {
            mName = filename;
        } else {
            mName = QString("%1/%2").arg(parentDir, filename);
        }

    } break;
    case PatchLayer::TypeMidiOut:

        mName = QString("MIDI: %1 → %2")
                .arg(mMidiInPortIdInProject)
                .arg(midiOutputPortData.portIdInProject);

        break;
    case PatchLayer::TypeAudioIn:

        mName = QString("Audio: %1 → %2")
                .arg(audioInPortData.portIdInProject)
                .arg(mBusIdInProject);

        break;

    }
}

Xml PatchLayer::soundfontDataToXml() const
{
    Xml xml(XML_SF_LAYER);

    KonfytSoundPreset p = soundfontData.program;
    xml.addTextChild(XML_SF_FILENAME, soundfontData.soundfontFilePath);
    xml.addTextChild(XML_SF_BANK, n2s(p.bank));
    xml.addTextChild(XML_SF_PROGRAM, n2s(p.program));
    xml.addTextChild(XML_SF_PROGRAM_NAME, soundfontData.program.name);

    addCommonDataToXml(&xml);

    return xml;
}

void PatchLayer::readSoundfontDataFromXml(Xml xml)
{
    SoundfontData sfData;
    sfData.soundfontFilePath = xml.childText(XML_SF_FILENAME);
    xml.setIntFromChild(XML_SF_BANK, &sfData.program.bank);
    xml.setIntFromChild(XML_SF_PROGRAM, &sfData.program.program);
    sfData.program.name = xml.childText(XML_SF_PROGRAM_NAME);
    initLayer(sfData);

    readCommonDataFromXml(xml);
}

Xml PatchLayer::sfzDataToXml() const
{
    Xml xml(XML_SFZ_LAYER);

    xml.addTextChild(XML_SFZ_PATH, sfzData.path);

    addCommonDataToXml(&xml);

    return xml;
}

void PatchLayer::readSfzDataFromXml(Xml xml)
{
    SfzData data;
    data.path = xml.childText(XML_SFZ_PATH);
    initLayer(data);

    readCommonDataFromXml(xml);
}

Xml PatchLayer::midiOutDataToXml() const
{
    Xml xml(XML_MIDIOUT_LAYER);

    xml.addTextChild(XML_MIDIOUT_PORT, n2s( midiOutputPortData.portIdInProject ));

    // MIDI Send list
    if (midiSendList.count()) {

        Xml sendListXml(XML_MIDISENDLIST);
        foreach (MidiSendItem item, midiSendList) {
            sendListXml.addChild(item.toXml());
        }
        xml.addChild(sendListXml);
    }

    addCommonDataToXml(&xml);

    return xml;
}

void PatchLayer::readMidiOutDataFromXml(Xml xml)
{
    MidiOutData data;
    xml.setIntFromChild(XML_MIDIOUT_PORT, &data.portIdInProject);
    initLayer(data);

    midiSendList.clear();
    Xml midiSendListXml = xml.child(XML_MIDISENDLIST);
    foreach (Xml itemXml, midiSendListXml.childrenNamed(MidiSendItem::XML_MIDI_SEND_ITEM)) {
        MidiSendItem item;
        item.readFromXml(itemXml);
        midiSendList.append(item);
    }

    readCommonDataFromXml(xml);
}

Xml PatchLayer::audioInDataToXml() const
{
    Xml xml(XML_AUDIOIN_LAYER);

    xml.addTextChild(XML_AUDIOIN_PORT, n2s(audioInPortData.portIdInProject));

    addCommonDataToXml(&xml);

    return xml;
}

void PatchLayer::readAudioInDataFromXml(Xml xml)
{
    AudioInData data;
    xml.setIntFromChild(XML_AUDIOIN_PORT, &data.portIdInProject);
    initLayer(data);

    readCommonDataFromXml(xml);
}

void PatchLayer::addCommonDataToXml(Xml *xml) const
{
    xml->addTextChild(XML_SOLO, bool2str(mSolo));
    xml->addTextChild(XML_MUTE, bool2str(mMute));
    xml->addTextChild(XML_RESET_OPTION, konfytResetToString(mResetOption));

    if (hasAudioOutput()) {
        xml->addTextChild(XML_GAIN, n2s(mGain));
        xml->addTextChild(XML_BUS, n2s(mBusIdInProject));
    }

    if (hasMidiInput()) {

        xml->addTextChild(XML_MIDI_IN, n2s(mMidiInPortIdInProject));

        xml->addChild(mMidiFilter.toXml());

        // Script
        Xml scriptXml(XML_SCRIPT);
        scriptXml.addTextChild(XML_SCRIPT_CONTENT, mScript);
        scriptXml.addTextChild(XML_SCRIPT_ENABLED,
                                 QVariant(mScriptEnabled).toString());
        scriptXml.addTextChild(XML_SCRIPT_PASS_MIDI_THROUGH,
                                 QVariant(mPassMidiThrough).toString());
        xml->addChild(scriptXml);

    }
}

void PatchLayer::readCommonDataFromXml(Xml xml)
{
    xml.setFloatFromChild(XML_GAIN, &mGain);
    xml.setBoolFromChild(XML_SOLO, &mSolo);
    xml.setBoolFromChild(XML_MUTE, &mMute);
    mMidiFilter.readFromXml(xml.child(MidiFilter::XML_MIDIFILTER));
    xml.setIntFromChild(XML_BUS, &mBusIdInProject);
    xml.setIntFromChild(XML_MIDI_IN, &mMidiInPortIdInProject);

    LayerScriptData script = readScriptFromXml(xml.child(XML_SCRIPT));
    setScript(script.content);
    setScriptEnabled(script.enabled);
    setPassMidiThrough(script.passMidiThrough);

    mResetOption = konfytResetFromString(xml.childText(XML_RESET_OPTION),
                                         mResetOption);
}

PatchLayer::LayerScriptData PatchLayer::readScriptFromXml(Xml xml)
{
    LayerScriptData ret;

    ret.content = xml.childText(XML_SCRIPT_CONTENT);
    xml.setBoolFromChild(XML_SCRIPT_ENABLED, &ret.enabled);
    xml.setBoolFromChild(XML_SCRIPT_PASS_MIDI_THROUGH, &ret.passMidiThrough);

    return ret;
}

PatchLayer::PatchLayer()
{
    gainMidiCtrl.setValue(mGain * 127.0);
}

void PatchLayer::initLayer(SoundfontData newLayerData)
{
    mLayerType = TypeSoundfontProgram;
    soundfontData = newLayerData;
    mMidiFilter.passProg = false;

    updateName();
}

void PatchLayer::initLayer(SfzData newLayerData)
{
    mLayerType = TypeSfz;
    sfzData = newLayerData;
    mMidiFilter.passProg = false;

    updateName();
}

void PatchLayer::initLayer(MidiOutData newLayerData)
{
    mLayerType = TypeMidiOut;
    midiOutputPortData = newLayerData;
    mMidiFilter.passProg = true;

    updateName();
}

void PatchLayer::initLayer(AudioInData newLayerData)
{
    mLayerType = TypeAudioIn;
    audioInPortData = newLayerData;

    updateName();
}

QString PatchLayer::name() const
{
    return mName;
}

float PatchLayer::gain() const
{
    return mGain;
}

void PatchLayer::setGain(float gain)
{
    mGain = gain;
    gainMidiCtrl.setValue(gain * 127.0);
}

/* Change the gain relatively by the specified MIDI value, ignoring MIDI pickup
 * range. */
void PatchLayer::addGainRelativeMidiValue(int value)
{
    value += gainMidiCtrl.value();
    value = qMax(0, qMin(127, value));

    gainMidiCtrl.setValue(value);
    mGain = gainMidiCtrl.value() / 127.0;
}

/* Set gain from MIDI, taking MIDI pickup range into account. */
void PatchLayer::setGainByMidi(int value)
{
    if (gainMidiCtrl.midiInput(value)) {
        mGain = gainMidiCtrl.value() / 127.0;
    }
}

void PatchLayer::setGainMidiPickupRange(int range)
{
    gainMidiCtrl.pickupRange = range;
}

int PatchLayer::gainMidiPickupRange()
{
    return gainMidiCtrl.pickupRange;
}

void PatchLayer::setSolo(bool isSolo)
{
    mSolo = isSolo;
}

void PatchLayer::setMute(bool isMute)
{
    mMute = isMute;
}

bool PatchLayer::isSolo() const
{
    return mSolo;
}

bool PatchLayer::isMute() const
{
    return mMute;
}

int PatchLayer::busIdInProject() const
{
    return mBusIdInProject;
}

void PatchLayer::setBusIdInProject(int bus)
{
    mBusIdInProject = bus;
    updateName();
}

int PatchLayer::midiInPortIdInProject() const
{
    return mMidiInPortIdInProject;
}

void PatchLayer::setMidiInPortIdInProject(int port)
{
    mMidiInPortIdInProject = port;
    updateName();
}

void PatchLayer::setResetOption(KonfytReset option)
{
    mResetOption = option;
}

KonfytReset PatchLayer::getResetOption()
{
    return mResetOption;
}

void PatchLayer::createResetSnapshot()
{
    mResetSnapshot.valid = true;
    mResetSnapshot.gain = mGain;
    mResetSnapshot.solo = mSolo;
    mResetSnapshot.mute = mMute;
}

void PatchLayer::restoreResetSnapshotIfAllowed(KonfytReset inheritedOption)
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

QString PatchLayer::script() const
{
    return mScript;
}

void PatchLayer::setScript(QString script)
{
    mScript = script;
}

bool PatchLayer::isScriptEnabled() const
{
    return mScriptEnabled;
}

void PatchLayer::setScriptEnabled(bool enable)
{
    mScriptEnabled = enable;
}

bool PatchLayer::isPassMidiThrough() const
{
    return mPassMidiThrough;
}

void PatchLayer::setPassMidiThrough(bool enable)
{
    mPassMidiThrough = enable;
}

MidiFilter PatchLayer::midiFilter() const
{
    return mMidiFilter;
}

void PatchLayer::setMidiFilter(MidiFilter midiFilter)
{
    mMidiFilter = midiFilter;
}

PatchLayer::LayerType PatchLayer::layerType() const
{
    return mLayerType;
}

/* Shorthand for determining of layer type has MIDI input. */
bool PatchLayer::hasMidiInput() const
{
    return    (mLayerType == TypeMidiOut)
           || (mLayerType == TypeSfz)
           || (mLayerType == TypeSoundfontProgram);
}

bool PatchLayer::hasAudioOutput() const
{
    return    (mLayerType == TypeAudioIn)
           || (mLayerType == TypeSfz)
           || (mLayerType == TypeSoundfontProgram);
}
