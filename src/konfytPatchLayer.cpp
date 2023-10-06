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

KonfytPatchLayer::KonfytPatchLayer()
{
    gainMidiCtrl.setValue(mGain * 127.0);
}

void KonfytPatchLayer::initLayer(LayerSoundfontData newLayerData)
{
    mLayerType = TypeSoundfontProgram;
    soundfontData = newLayerData;
    mName = soundfontData.parentSoundfont + "/" + soundfontData.program.name;
}

void KonfytPatchLayer::initLayer(LayerSfzData newLayerData)
{
    mLayerType = TypeSfz;
    sfzData = newLayerData;
    mName = "SFZ";
}

void KonfytPatchLayer::initLayer(LayerMidiOutData newLayerData)
{
    mLayerType = TypeMidiOut;
    midiOutputPortData = newLayerData;
    mName = "MIDI Out Port";
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

void KonfytPatchLayer::setGainByMidi(int value)
{
    if (gainMidiCtrl.midiInput(value)) {
        setGain((float)gainMidiCtrl.value()/127.0);
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

QString KonfytPatchLayer::script() const
{
    return mScript;
}

void KonfytPatchLayer::setScript(QString script)
{
    mScript = script;
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
