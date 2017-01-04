#ifndef KONFYTJACKSTRUCTS_H
#define KONFYTJACKSTRUCTS_H

#include "konfytMidiFilter.h"

typedef enum {
    KonfytJackPortType_AudioIn  = 0,
    KonfytJackPortType_AudioOut = 1,
    KonfytJackPortType_MidiIn   = 2,
    KonfytJackPortType_MidiOut  = 3,
} konfytJackPortType;

/* When creating, use constructor in order to initialise destinationPort to NULL.
 * Jack process callback uses this. */
typedef struct konfytJackPort_t {
    jack_port_t* jack_pointer;
    bool active;
    bool prev_active;
    void* buffer;
    konfytMidiFilter filter;
    bool solo;
    bool mute;
    float gain;
    QStringList connectionList;
    konfytJackPort_t* destinationPort; // For audio input ports, the destination output port (bus).

    // Constructor with initializer list. Instantiate all instances with constructor to take advantage of this.
    konfytJackPort_t() : jack_pointer(NULL), active(false), prev_active(false),
        solo(false), mute(false), gain(1), destinationPort(NULL) {}

} konfytJackPort;

typedef struct {

    int plugin_id;
    konfytJackPort* midi_out;    // Send midi output to plugin
    konfytJackPort* audio_in_l;  // Receive plugin audio
    konfytJackPort* audio_in_r;

} konfytJackPluginPorts;

#endif // KONFYTJACKSTRUCTS_H
