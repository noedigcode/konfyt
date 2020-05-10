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

#include "konfytPatchLayer.h"

void KonfytPatchLayer::setErrorMessage(QString msg)
{
    this->errorMessage = msg;
}

bool KonfytPatchLayer::hasError() const
{
    if ( this->errorMessage.length() ) {
        return true;
    } else {
        return false;
    }
}

QString KonfytPatchLayer::getErrorMessage() const
{
    return this->errorMessage;
}

QList<KonfytMidiEvent> KonfytPatchLayer::getMidiSendListEvents()
{
    QList<KonfytMidiEvent> events;
    foreach (MidiSendItem item, midiSendList) {
        events.append(item.midiEvent);
    }
    return events;
}


/* Use to initialise the layer object.
 * Accepts an ID which will later be used by the patch class to uniquely identify it. */
void KonfytPatchLayer::initLayer(int id, LayerSoundfontStruct newLayerData)
{
    this->idInPatch = id;
    this->layerType = KonfytLayerType_SoundfontProgram;
    this->soundfontData = newLayerData;
}

void KonfytPatchLayer::initLayer(int id, LayerSfzStruct newLayerData)
{
    this->idInPatch = id;
    this->layerType = KonfytLayerType_Sfz;
    this->sfzData = newLayerData;
}

void KonfytPatchLayer::initLayer(int id, LayerMidiOutStruct newLayerData)
{
    this->idInPatch = id;
    this->layerType = KonfytLayerType_MidiOut;
    this->midiOutputPortData = newLayerData;
}

void KonfytPatchLayer::initLayer(int id, LayerAudioInStruct newLayerData)
{
    this->idInPatch = id;
    this->layerType = KonfytLayerType_AudioIn;
    this->audioInPortData = newLayerData;
}

QString KonfytPatchLayer::getName() const
{
    switch (layerType) {
    case KonfytLayerType_AudioIn:
        return this->audioInPortData.name;
        break;
    case KonfytLayerType_Sfz:
        return this->sfzData.name;
        break;
    case KonfytLayerType_MidiOut:
        return "MIDI Out Port";
        break;
    case KonfytLayerType_SoundfontProgram:
        return this->soundfontData.program.parent_soundfont + "/" + this->soundfontData.program.name;
        break;
    case KonfytLayerType_Uninitialized:
        return "UNINITIALIZED LAYER";
        break;
    default:
        return "LAYER TYPE ERROR";
    }
}

float KonfytPatchLayer::getGain() const
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        return this->soundfontData.gain;
    } else if (this->layerType == KonfytLayerType_Sfz) {
        return this->sfzData.gain;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        return this->audioInPortData.gain;
    } else {
        return 0;
    }
}

void KonfytPatchLayer::setGain(float newGain)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->soundfontData.gain = newGain;
    } else if (this->layerType == KonfytLayerType_Sfz) {
        this->sfzData.gain = newGain;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        this->audioInPortData.gain = newGain;
    }
}

void KonfytPatchLayer::setSolo(bool newSolo)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->soundfontData.solo = newSolo;
    } else if (this->layerType == KonfytLayerType_Sfz) {
        this->sfzData.solo = newSolo;
    } else if (this->layerType == KonfytLayerType_MidiOut) {
        this->midiOutputPortData.solo = newSolo;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        this->audioInPortData.solo = newSolo;
    }
}

void KonfytPatchLayer::setMute(bool newMute)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->soundfontData.mute = newMute;
    } else if (this->layerType == KonfytLayerType_Sfz) {
        this->sfzData.mute = newMute;
    } else if (this->layerType == KonfytLayerType_MidiOut) {
        this->midiOutputPortData.mute = newMute;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        this->audioInPortData.mute = newMute;
    }
}

bool KonfytPatchLayer::isSolo() const
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        return this->soundfontData.solo;
    } else if (this->layerType == KonfytLayerType_Sfz) {
        return this->sfzData.solo;
    } else if (this->layerType == KonfytLayerType_MidiOut) {
        return this->midiOutputPortData.solo;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        return this->audioInPortData.solo;
    }
    return false;
}

bool KonfytPatchLayer::isMute() const
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        return this->soundfontData.mute;
    } else if (this->layerType == KonfytLayerType_Sfz) {
        return this->sfzData.mute;
    } else if (this->layerType == KonfytLayerType_MidiOut) {
        return this->midiOutputPortData.mute;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        return this->audioInPortData.mute;
    }
    return false;
}

KonfytMidiFilter KonfytPatchLayer::getMidiFilter() const
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        return this->soundfontData.filter;
    } else if (this->layerType == KonfytLayerType_Sfz) {
        return this->sfzData.midiFilter;
    } else {
        return this->midiOutputPortData.filter;
    }
}

void KonfytPatchLayer::setMidiFilter(KonfytMidiFilter newFilter)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->soundfontData.filter = newFilter;
    } else if (this->layerType == KonfytLayerType_Sfz) {
        this->sfzData.midiFilter = newFilter;
    } else {
        this->midiOutputPortData.filter = newFilter;
    }
}

KonfytLayerType KonfytPatchLayer::getLayerType() const
{
    return layerType;
}

/* Shorthand for determining of layer type has MIDI input. */
bool KonfytPatchLayer::hasMidiInput() const
{
    return    (layerType == KonfytLayerType_MidiOut)
           || (layerType == KonfytLayerType_Sfz)
           || (layerType == KonfytLayerType_SoundfontProgram);
}
