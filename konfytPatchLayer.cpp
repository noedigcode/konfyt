#include "konfytPatchLayer.h"

konfytPatchLayer::konfytPatchLayer()
{
    // ----------------------------------------------------
    // Initialise variables
    // ----------------------------------------------------

    this->layerType = KonfytLayerType_Uninitialized;
    busIdInProject = 0;

}

void konfytPatchLayer::setErrorMessage(QString msg)
{
    this->errorMessage = msg;
}

bool konfytPatchLayer::hasError()
{
    if ( this->errorMessage.length() ) {
        return true;
    } else {
        return false;
    }
}

QString konfytPatchLayer::getErrorMessage()
{
    return this->errorMessage;
}


// Use to initialise the layer object.
// Accepts an ID which will later be used by the patch class to uniquely identify it.
void konfytPatchLayer::initLayer(int id, layerSoundfontStruct newLayerData)
{
    this->ID_in_patch = id;
    this->layerType = KonfytLayerType_SoundfontProgram;
    this->sfData = newLayerData;
}

void konfytPatchLayer::initLayer(int id, layerCarlaPluginStruct newLayerData)
{
    this->ID_in_patch = id;
    this->layerType = KonfytLayerType_CarlaPlugin;
    this->carlaPluginData = newLayerData;
}

void konfytPatchLayer::initLayer(int id, layerMidiOutStruct newLayerData)
{
    this->ID_in_patch = id;
    this->layerType = KonfytLayerType_MidiOut;
    this->midiOutputPortData = newLayerData;
}

void konfytPatchLayer::initLayer(int id, layerAudioInStruct newLayerData)
{
    this->ID_in_patch = id;
    this->layerType = KonfytLayerType_AudioIn;
    this->audioInPortData = newLayerData;
}

float konfytPatchLayer::getGain()
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



void konfytPatchLayer::setGain(float newGain)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->sfData.gain = newGain;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        this->carlaPluginData.gain = newGain;
    } else if (this->layerType == KonfytLayerType_AudioIn) {
        this->audioInPortData.gain = newGain;
    }
}

void konfytPatchLayer::setSolo(bool newSolo)
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

void konfytPatchLayer::setMute(bool newMute)
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

bool konfytPatchLayer::isSolo()
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
}

bool konfytPatchLayer::isMute()
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
}

konfytMidiFilter konfytPatchLayer::getMidiFilter()
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        return this->sfData.filter;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        return this->carlaPluginData.midiFilter;
    } else {
        return this->midiOutputPortData.filter;
    }
}

void konfytPatchLayer::setMidiFilter(konfytMidiFilter newFilter)
{
    if (this->layerType == KonfytLayerType_SoundfontProgram) {
        this->sfData.filter = newFilter;
    } else if (this->layerType == KonfytLayerType_CarlaPlugin) {
        this->carlaPluginData.midiFilter = newFilter;
    } else {
        this->midiOutputPortData.filter = newFilter;
    }
}



konfytLayerType konfytPatchLayer::getLayerType()
{
    return layerType;
}
