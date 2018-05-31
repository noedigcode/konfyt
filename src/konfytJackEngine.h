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

class KonfytJackEngine : public QObject
{
    Q_OBJECT
public:
    explicit KonfytJackEngine(QObject *parent = 0);

    void panic(bool p);

    // Jack callback functions
    static void jackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void* arg);
    static void jackPortRegistrationCallback(jack_port_id_t port, int registered, void *arg);
    static int jackProcessCallback(jack_nframes_t nframes, void *arg);
    static int jackXrunCallback(void *arg);
    // Helper functions
    void handleNoteoffEvent(KonfytMidiEvent &ev, KonfytJackPort* sourcePort);
    void handleSustainoffEvent(KonfytMidiEvent &ev, KonfytJackPort *sourcePort);
    void handlePitchbendZeroEvent(KonfytMidiEvent &ev, KonfytJackPort *sourcePort);
    void processMidiEventForFluidsynth(KonfytJackPort *sourcePort, KonfytMidiEvent &ev, bool recordNoteon, bool recordSustain, bool recordPitchbend);
    void processMidiEventForMidiOutPorts(KonfytJackPort *sourcePort, KonfytMidiEvent &ev, bool recordNoteon, bool recordSustain, bool recordPitchbend);
    void processMidiEventForPlugins(KonfytJackPort *sourcePort, KonfytMidiEvent &ev, bool recordNoteon, bool recordSustain, bool recordPitchbend);
    bool passMuteSoloActiveCriteria(KonfytJackPort* port);
    bool passMuteSoloCriteria(KonfytJackPort* port);
    void mixBufferToDestinationPort(KonfytJackPort* port, jack_nframes_t nframes, bool applyGain);
    void sendMidiClosureEvents(KonfytJackPort* port, int channel);
    void sendMidiClosureEvents_chanZeroOnly(KonfytJackPort* port);
    void sendMidiClosureEvents_allChannels(KonfytJackPort* port);
    bool passMidiMuteSoloActiveFilterAndModify(KonfytJackPort* port, const KonfytMidiEvent* ev, KonfytMidiEvent* evToSend);

    bool connectCallback;
    bool registerCallback;

    konfytFluidsynthEngine* fluidsynthEngine;

    float *fadeOutValues;
    unsigned int fadeOutValuesCount;
    float fadeOutSecs;

    KonfytMidiEvent evAllNotesOff;
    KonfytMidiEvent evSustainZero;
    KonfytMidiEvent evPitchbendZero;
    void initMidiClosureEvents();

    bool soloFlag;

    bool jack_process_busy;
    int jack_process_disable; // Non-zero value = disabled
    bool timer_busy;
    bool timer_disabled;

    QList<KonfytJackPort*> midi_in_ports;
    QList<KonfytJackPort*> midi_out_ports;
    QList<KonfytJackPort*> audio_out_ports;
    QList<KonfytJackPort*> audio_in_ports;

    QList<KonfytJackPluginPorts*> plugin_ports;
    QList<KonfytJackPluginPorts*> soundfont_ports;

    konfytArrayList<KonfytJackNoteOnRecord> noteOnList;
    konfytArrayList<KonfytJackNoteOnRecord> sustainList;
    konfytArrayList<KonfytJackNoteOnRecord> pitchBendList;

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
    void dummyFunction(KonfytJackPortType type); // Dummy function to copy structure from

    KonfytJackPort* addPort(KonfytJackPortType type, QString port_name);
    void removePort(KonfytJackPortType type, KonfytJackPort* port);
    void removeAllAudioInAndOutPorts();
    void removeAllMidiInPorts();
    void removeAllMidiOutPorts();
    void nullDestinationPorts_pointingTo(KonfytJackPort* port);
    void nullDestinationPorts_all();

    void setPortClients(KonfytJackPortType type, KonfytJackPort* port, QStringList newClientList);
    void clearPortClients(KonfytJackPortType type, KonfytJackPort* port);
    void clearPortClients_andDisconnect(KonfytJackPortType type, KonfytJackPort* port);
    void addPortClient(KonfytJackPortType type, KonfytJackPort *port, QString newClient);
    void removePortClient_andDisconnect(KonfytJackPortType type, KonfytJackPort* port, QString cname);
    QStringList getPortClients(KonfytJackPortType type, KonfytJackPort* port);
    int getPortCount(KonfytJackPortType type);
    void setPortFilter(KonfytJackPortType type, KonfytJackPort* port, konfytMidiFilter filter);
    void setPortSolo(KonfytJackPortType type, KonfytJackPort* port, bool solo);
    void setPortMute(KonfytJackPortType type, KonfytJackPort* port, bool mute);
    void setPortGain(KonfytJackPortType type, KonfytJackPort* port, float gain);
    void setPortRouting(KonfytJackPortType type, KonfytJackPort* port, KonfytJackPort* route);

    void setPortActive(KonfytJackPortType type, KonfytJackPort* port, bool active);
    void setAllPortsActive(KonfytJackPortType type, bool active);

    // Carla plugins
    void addPluginPortsAndConnect(LayerCarlaPluginStruct plugin);
    void removePlugin(int indexInEngine);
    void setPluginMidiFilter(int indexInEngine, konfytMidiFilter filter);
    void setPluginSolo(int indexInEngine, bool solo);
    void setPluginMute(int indexInEngine, bool mute);
    void setPluginRouting(int indexInEngine, KonfytJackPort *midi_in,
                                             KonfytJackPort *port_left,
                                             KonfytJackPort *port_right);
    void setAllPluginPortsActive(bool active);

    // Fluidsynth
    void addSoundfont(LayerSoundfontStruct sf);
    void removeSoundfont(int indexInEngine);
    void setSoundfontMidiFilter(int indexInEngine, konfytMidiFilter filter);
    void setSoundfontSolo(int indexInEngine, bool solo);
    void setSoundfontMute(int indexInEngine, bool mute);
    void setSoundfontRouting(int indexInEngine, KonfytJackPort *midi_in,
                                                KonfytJackPort *port_left,
                                                KonfytJackPort *port_right);
    void setAllSoundfontPortsActive(bool active);

    // Flag indicating one or more ports or plugin ports are solo
    void refreshSoloFlag();
    bool getSoloFlagInternal();
    void setSoloFlagExternal(bool newSolo);

    // Other JACK connections
    QList<KonfytJackConPair> otherConsList;
    void addOtherJackConPair(KonfytJackConPair p);
    void removeOtherJackConPair(KonfytJackConPair p);
    void clearOtherJackConPair();

    void activatePortsForPatch(const konfytPatch *patch, const KonfytProject *project);

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
    QMap<int, KonfytJackPluginPorts*> pluginsPortsMap;
    QMap<int, KonfytJackPluginPorts*> soundfontPortsMap;

    // JACK helper function for internal use. Used by the public helper functions.
    QStringList getPortsList(QString typePattern, unsigned long flags);

    bool soloFlag_external; // Indicates if another engine as a solo active.

    void error_abort(QString msg);

signals:
    void userMessage(QString msg);
    void jackPortRegisterOrConnectCallback();
    void midiEventSignal(KonfytMidiEvent event);
    void xrunSignal();

    
public slots:
    
};

#endif // KONFYT_JACK_ENGINE_H
