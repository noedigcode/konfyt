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

#include "konfytPatchLayer.h"

KonfytPatchLayer::KonfytPatchLayer()
{
    // ----------------------------------------------------
    // Initialise variables
    // ----------------------------------------------------

    this->layerType = KonfytLayerType_Uninitialized;
    busIdInProject = 0;
    midiInPortIdInProject = 0;
}

void KonfytPatchLayer::setErrorMessage(QString msg)
{
    this->errorMessage = msg;
}

bool KonfytPatchLayer::hasError()
{
    if ( this->errorMessage.length() ) {
        return true;
    } else {
        return false;
    }
}

QString KonfytPatchLayer::getErrorMessage()
{
    return this->errorMessage;
}


// Use to initialise the layer object.
// Accepts an ID which will later be used by the patch class to uniquely identify it.
void KonfytPatchLayer::initLayer(int id, LayerSoundfontStruct newLayerData)
{
    this->idInPatch = id;
    this->layerType = KonfytLayerType_SoundfontProgram;
    this->sfData = newLayerData;
}

void KonfytPatchLayer::initLayer(int id, LayerCarlaPluginStruct newLayerData)
{
    this->idInPatch = id;
    this->layerType = KonfytLayerType_CarlaPlugin;
    this->carlaPluginData = newLayerData;
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

QString KonfytPatchLayer::getName()
{
    switch (layerType) {
    case KonfytLayerType_AudioIn:
        return this->audioInPortData.name;
        break;
    case KonfytLayerType_CarlaPlugin:
        return this->carlaPluginData.name;
        break;
    case KonfytLayerType_MidiOut:
        return "MIDI Out Port";
        break;
    case KonfytLayerType_SoundfontProgram:
        return this->sfData.program.parent_soundfont + "/" + this->sfData.program.name;
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
        return this->sfData.gain;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        return this->carlaPluginData.gain;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        return this->audioInPortData.gain;
    } else {
        return 0;
    }
}

void KonfytPatchLayer::setGain(float newGain)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->sfData.gain = newGain;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        this->carlaPluginData.gain = newGain;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        this->audioInPortData.gain = newGain;
    }
}

void KonfytPatchLayer::setSolo(bool newSolo)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->sfData.solo = newSolo;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        this->carlaPluginData.solo = newSolo;
    } else if (this->layerType == KonfytLayerType_MidiOut) {
        this->midiOutputPortData.solo = newSolo;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        this->audioInPortData.solo = newSolo;
    }
}

void KonfytPatchLayer::setMute(bool newMute)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->sfData.mute = newMute;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        this->carlaPluginData.mute = newMute;
    } else if (this->layerType == KonfytLayerType_MidiOut) {
        this->midiOutputPortData.mute = newMute;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        this->audioInPortData.mute = newMute;
    }
}

bool KonfytPatchLayer::isSolo() const
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        return this->sfData.solo;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        return this->carlaPluginData.solo;
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
        return this->sfData.mute;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        return this->carlaPluginData.mute;
    } else if (this->layerType == KonfytLayerType_MidiOut) {
        return this->midiOutputPortData.mute;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        return this->audioInPortData.mute;
    }
    return false;
}

KonfytMidiFilter KonfytPatchLayer::getMidiFilter()
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        return this->sfData.filter;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        return this->carlaPluginData.midiFilter;
    } else {
        return this->midiOutputPortData.filter;
    }
}

void KonfytPatchLayer::setMidiFilter(KonfytMidiFilter newFilter)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->sfData.filter = newFilter;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        this->carlaPluginData.midiFilter = newFilter;
    } else {
        this->midiOutputPortData.filter = newFilter;
    }
}



KonfytLayerType KonfytPatchLayer::getLayerType() const
{
    return layerType;
}
