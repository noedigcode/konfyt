/******************************************************************************
 *
 * Copyright 2017 Gideon van der Kolf
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

#ifndef KONFYT_JACK_ENGINE_H
#define KONFYT_JACK_ENGINE_H

#include <QObject>
#include <QStringList>
#include <QBasicTimer>
#include <QTimerEvent>
#include <QMap>

#include <jack/jack.h>
#include <jack/midiport.h>

#include "konfytPatch.h"
#include "konfytProject.h"
#include "konfytDefines.h"
#include "konfytStructs.h"
#include "konfytFluidsynthEngine.h"
#include "konfytStructs.h"
#include "konfytJackStructs.h"
#include "konfytArrayList.h"

#define KONFYT_JACK_MIDI_IN_PORT_NAME "midi_in"
#define KONFYT_JACK_MIDI_OUT_PORT_NAME "midi_out_"
#define KONFYT_JACK_AUDIO_IN_PORT_NAME "audio_in_"
#define KONFYT_JACK_AUDIO_BUS_PORT_NAME "audio_bus_"
#define KONFYT_JACK_AUDIO_OUT_PORT_NAME "audio_out_"
#define KONFYT_JACK_DEFAULT_CLIENT_NAME "Konfyt"   // Default client name. Actual name is set in the Jack client.
#define KONFYT_JACK_SYSTEM_OUT_LEFT "system:playback_1"
#define KONFYT_JACK_SYSTEM_OUT_RIGHT "system:playback_2"

#define KONFYT_JACK_SUSTAIN_THRESH 63


typedef void (*send_midi_to_fluidsynth_t)(unsigned char* data, int size);

class konfytJackEngine : public QObject
{
    Q_OBJECT
public:
    explicit konfytJackEngine(QObject *parent = 0);

    void panic(bool p);

    // Jack callback functions
    static void jackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void* arg);
    static void jackPortRegistrationCallback(jack_port_id_t port, int registered, void *arg);
    static int jackProcessCallback(jack_nframes_t nframes, void *arg);
    static int jackXrunCallback(void *arg);
    // Helper functions
    bool passMuteSoloActiveCriteria(konfytJackPort* port);
    bool passMuteSoloCriteria(konfytJackPort* port);
    void mixBufferToDestinationPort(konfytJackPort* port, jack_nframes_t nframes, bool applyGain);
    void sendMidiClosureEvents(konfytJackPort* port, int channel);
    void sendMidiClosureEvents_chanZeroOnly(konfytJackPort* port);
    void sendMidiClosureEvents_allChannels(konfytJackPort* port);
    bool passMidiMuteSoloActiveFilterAndModify(konfytJackPort* port, const konfytMidiEvent* ev, konfytMidiEvent* evToSend);

    bool connectCallback;
    bool registerCallback;

    konfytFluidsynthEngine* fluidsynthEngine;

    float *fadeOutValues;
    unsigned int fadeOutValuesCount;
    float fadeOutSecs;

    konfytMidiEvent evAllNotesOff;
    konfytMidiEvent evSustainZero;
    konfytMidiEvent evPitchbendZero;
    void initMidiClosureEvents();

    bool soloFlag;

    bool jack_process_busy;
    int jack_process_disable; // Non-zero value = disabled
    bool timer_busy;
    bool timer_disabled;

    QList<konfytJackPort*> midi_out_ports;
    QList<konfytJackPort*> audio_out_ports;
    QList<konfytJackPort*> audio_in_ports;

    QList<konfytJackPluginPorts*> plugin_ports;
    QList<konfytJackPluginPorts*> soundfont_ports;

    konfytArrayList<konfytJackNoteOnRecord> noteOnList;
    konfytArrayList<konfytJackNoteOnRecord> sustainList;
    konfytArrayList<konfytJackNoteOnRecord> pitchBendList;

    // Our main midi input port
    jack_port_t *midi_input_port;


    bool InitJackClient(QString name);
    void stopJackClient();
    bool clientIsActive();
    QString clientName();
    void pauseJackProcessing(bool pause);

    // JACK helper functions (Not specific to our client)
    QStringList getMidiInputPortsList();
    QStringList getMidiOutputPortsList();
    QStringList getAudioInputPortsList();
    QStringList getAudioOutputPortsList();
    double getSampleRate();

    // Auto connect input port
    void setOurMidiInputPortName(QString newName);
    void addPortToAutoConnectList(QString port);
    void addAutoConnectList(QStringList ports);
    void removePortFromAutoConnectList_andDisconnect(QString port); // TODO: MAYBE MERGE WITH removePortClient_andDisconnect FUNCTION
    void clearAutoConnectList();
    void clearAutoConnectList_andDisconnect();
    void refreshPortConnections();

    // Audio / midi in/out ports
    void dummyFunction(konfytJackPortType type); // Dummy function to copy structure from

    konfytJackPort* addPort(konfytJackPortType type, QString port_name);
    void removePort(konfytJackPortType type, konfytJackPort* port);
    void removeAllAudioInAndOutPorts();
    void removeAllMidiOutPorts();
    void nullDestinationPorts_pointingTo(konfytJackPort* port);
    void nullDestinationPorts_all();

    void setPortClients(konfytJackPortType type, konfytJackPort* port, QStringList newClientList);
    void clearPortClients(konfytJackPortType type, konfytJackPort* port);
    void clearPortClients_andDisconnect(konfytJackPortType type, konfytJackPort* port);
    void addPortClient(konfytJackPortType type, konfytJackPort *port, QString newClient);
    void removePortClient_andDisconnect(konfytJackPortType type, konfytJackPort* port, QString cname);
    QStringList getPortClients(konfytJackPortType type, konfytJackPort* port);
    int getPortCount(konfytJackPortType type);
    void setPortFilter(konfytJackPortType type, konfytJackPort* port, konfytMidiFilter filter);
    void setPortSolo(konfytJackPortType type, konfytJackPort* port, bool solo);
    void setPortMute(konfytJackPortType type, konfytJackPort* port, bool mute);
    void setPortGain(konfytJackPortType type, konfytJackPort* port, float gain);
    void setPortRouting(konfytJackPortType type, konfytJackPort* src, konfytJackPort* dest);

    void setPortActive(konfytJackPortType type, konfytJackPort* port, bool active);
    void setAllPortsActive(konfytJackPortType type, bool active);

    // Carla plugins
    void addPluginPortsAndConnect(layerCarlaPluginStruct plugin);
    void removePlugin(int indexInEngine);
    void setPluginMidiFilter(int indexInEngine, konfytMidiFilter filter);
    void setPluginSolo(int indexInEngine, bool solo);
    void setPluginMute(int indexInEngine, bool mute);
    void setPluginRouting(int indexInEngine, konfytJackPort *port_left, konfytJackPort *port_right);
    void setAllPluginPortsActive(bool active);

    // Fluidsynth
    void addSoundfont(layerSoundfontStruct sf);
    void removeSoundfont(int indexInEngine);
    void setSoundfontMidiFilter(int indexInEngine, konfytMidiFilter filter);
    void setSoundfontSolo(int indexInEngine, bool solo);
    void setSoundfontMute(int indexInEngine, bool mute);
    void setSoundfontRouting(int indexInEngine, konfytJackPort *port_left, konfytJackPort *port_right);
    void setAllSoundfontPortsActive(bool active);

    // Flag indicating one or more ports or plugin ports are solo
    void refreshSoloFlag();
    bool getSoloFlagInternal();
    void setSoloFlagExternal(bool newSolo);

    // Other JACK connections
    QList<konfytJackConPair> otherConsList;
    void addOtherJackConPair(konfytJackConPair p);
    void removeOtherJackConPair(konfytJackConPair p);
    void clearOtherJackConPair();

    void activatePortsForPatch(const konfytPatch *patch, const konfytProject *project);

    void setGlobalTranspose(int transpose);


private:
    jack_client_t* client;
    jack_nframes_t nframes;
    bool clientActive;      // Flag to indicate if the client has been successfully activated
    double samplerate;

    bool panicCmd;  // Panic command from outside
    int panicState; // Internal panic state

    QString ourJackClientName;
    QString ourMidiInputPortName;

    // Timer for handling port register and connect callbacks
    QBasicTimer timer;
    void timerEvent(QTimerEvent *event);
    void startPortTimer();

    int globalTranspose;


    // Auto connection of our main midi input port
    QStringList midiConnectList;

    // Map plugin index in engine to port structure
    QMap<int, konfytJackPluginPorts*> pluginsPortsMap;
    QMap<int, konfytJackPluginPorts*> soundfontPortsMap;

    // JACK helper function for internal use. Used by the public helper functions.
    QStringList getPortsList(QString typePattern, unsigned long flags);

    bool soloFlag_external; // Indicates if another engine as a solo active.

    void error_abort(QString msg);

signals:
    void userMessage(QString msg);
    void jackPortRegisterOrConnectCallback();
    void midiEventSignal(konfytMidiEvent event);
    void xrunSignal();

    
public slots:
    
};

#endif // KONFYT_JACK_ENGINE_H
