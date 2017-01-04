#ifndef KONFYT_PATCH_LAYER_H
#define KONFYT_PATCH_LAYER_H

#include <QObject>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLineEdit>
#include <QSlider>
#include "konfytStructs.h"
#include "konfytMidiFilter.h"



enum konfytLayerType {
    KonfytLayerType_Uninitialized    = -1,
    KonfytLayerType_SoundfontProgram = 0,
    KonfytLayerType_CarlaPlugin      = 1, // for sfz, lv2, internal, etc.
    KonfytLayerType_MidiOut          = 2,
    KonfytLayerType_AudioIn          = 3
};

enum konfytCarlaPluginType {
    KonfytCarlaPluginType_SFZ = 1,
    KonfytCarlaPluginType_LV2 = 2,
    KonfytCarlaPluginType_Internal = 3
};

// ----------------------------------------------------
// Structure for soundfont program layer
// ----------------------------------------------------
typedef struct layerSoundfontStruct_t {
    konfytSoundfontProgram program;
    float gain;
    konfytMidiFilter filter;
    int indexInEngine;
    bool solo;
    bool mute;

    // Constructor with initializer list. Instantiate all instances with constructor to take advantage of this.
    layerSoundfontStruct_t() : gain(1), indexInEngine(-1), solo(true), mute(true) {}

} layerSoundfontStruct;

// ----------------------------------------------------
// Structure for carla plugin
// ----------------------------------------------------
typedef struct layerCarlaPluginStruct_t {
    QString name;
    QString path;
    konfytCarlaPluginType pluginType;
    QString midi_in_port;
    QString audio_out_port_left;
    QString audio_out_port_right;

    konfytMidiFilter midiFilter;

    int indexInEngine;

    float gain;
    bool solo;
    bool mute;

    // Constructor with initializer list. Instantiate all instances with constructor to take advantage of this.
    layerCarlaPluginStruct_t() : indexInEngine(-1), gain(1), solo(false), mute(false) {}

} layerCarlaPluginStruct;


// ----------------------------------------------------
// Structure for midi output port layer
// ----------------------------------------------------
typedef struct layerMidiOutputStruct_t {
    int portIdInProject;
    // TODO: IMPLEMENT SENDING EVENTS WHEN SWITCHING TO MIDI PORT
    // but use the konfytMidiEvent struct
    //QList<midiEventStruct> sendEventsStart; // List of midi events to send when switching to port
    konfytMidiFilter filter;
    bool solo;
    bool mute;

    // Constructor with initializer list. Instantiate all instances with constructor to take advantage of this.
    layerMidiOutputStruct_t() : solo(false), mute(false) {}

} layerMidiOutStruct;

// ----------------------------------------------------
// Structure for audio input port layer
// ----------------------------------------------------
typedef struct layerAudioInStruct_t {
    QString name;
    int portIdInProject;   // Index of audio input port in project
    float gain;
    bool solo;
    bool mute;

    layerAudioInStruct_t() : gain(1), solo(false), mute(false) {}

} layerAudioInStruct;


// ----------------------------------------------------
// Class
// ----------------------------------------------------
class konfytPatchLayer
{
public:
    explicit konfytPatchLayer();

    int ID_in_patch; // ID used in patch to uniquely identify layeritems within a patch.

    konfytLayerType getLayerType();

    // Use to initialize layer
    void initLayer(int id, layerSoundfontStruct newLayerData);
    void initLayer(int id, layerCarlaPluginStruct newLayerData);
    void initLayer(int id, layerMidiOutStruct newLayerData);
    void initLayer(int id, layerAudioInStruct newLayerData);

    float getGain();
    void setGain(float newGain);
    void setSolo(bool newSolo);
    void setMute(bool newMute);
    bool isSolo();
    bool isMute();

    int busIdInProject;

    konfytMidiFilter getMidiFilter();
    void setMidiFilter(konfytMidiFilter newFilter);

    void setErrorMessage(QString msg);
    bool hasError();
    QString getErrorMessage();

    // Depending on the layer type, one of the following is used:
    layerSoundfontStruct    sfData;
    layerCarlaPluginStruct  carlaPluginData;
    layerMidiOutStruct      midiOutputPortData;
    layerAudioInStruct      audioInPortData;


private:
    konfytLayerType layerType;

    QString errorMessage;
    
};

#endif // KONFYT_PATCH_LAYER_H
