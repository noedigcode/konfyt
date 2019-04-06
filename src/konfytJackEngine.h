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
#include <QBasicTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QStringList>
#include <QTimer>
#include <QTimerEvent>

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
#include "ringbufferqmutex.h"

#define KONFYT_JACK_DEFAULT_CLIENT_NAME "Konfyt"   // Default client name. Actual name is set in the Jack client.
#define KONFYT_JACK_SYSTEM_OUT_LEFT "system:playback_1"
#define KONFYT_JACK_SYSTEM_OUT_RIGHT "system:playback_2"

#define KONFYT_JACK_SUSTAIN_THRESH 63

#define KONFYT_JACK_PORT_ERROR -1


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
    void handleNoteoffEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort* sourcePort);
    void handleSustainoffEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort *sourcePort);
    void handlePitchbendZeroEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort *sourcePort);
    void processMidiEventForFluidsynth(KonfytJackPort *sourcePort, KonfytMidiEvent &ev, bool recordNoteon, bool recordSustain, bool recordPitchbend);
    void processMidiEventForMidiOutPorts(KonfytJackPort *sourcePort, jack_midi_event_t inEvent_jack, KonfytMidiEvent &ev, bool recordNoteon, bool recordSustain, bool recordPitchbend);
    void processMidiEventForPlugins(KonfytJackPort *sourcePort, jack_midi_event_t inEvent_jack, KonfytMidiEvent &ev, bool recordNoteon, bool recordSustain, bool recordPitchbend);
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

    RingbufferQMutex<KonfytMidiEvent> eventsBuffer;

    KonfytMidiEvent evAllNotesOff;
    KonfytMidiEvent evSustainZero;
    KonfytMidiEvent evPitchbendZero;
    void initMidiClosureEvents();

    bool soloFlag;

    volatile bool jack_process_busy;
    int jack_process_disable; // Non-zero value = disabled
    bool timer_busy;
    bool timer_disabled;

    // Port data structures (public for Jack process access)
    QList<KonfytJackPort*> midi_in_ports;
    QList<KonfytJackPort*> midi_out_ports;
    QList<KonfytJackPort*> audio_out_ports;
    QList<KonfytJackPort*> audio_in_ports;

    QList<KonfytJackPluginPorts*> plugin_ports;
    QList<KonfytJackPluginPorts*> soundfont_ports;

    KonfytArrayList<KonfytJackNoteOnRecord> noteOnList;
    KonfytArrayList<KonfytJackNoteOnRecord> sustainList;
    KonfytArrayList<KonfytJackNoteOnRecord> pitchBendList;


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

    // Audio / midi in/out ports
    void dummyFunction(KonfytJackPortType type); // Dummy function to copy structure from

    void refreshPortConnections();

    int addPort(KonfytJackPortType type, QString port_name);
    void removePort(int portId);
    void removeAllAudioInAndOutPorts();
    void removeAllMidiInPorts();
    void removeAllMidiOutPorts();
    void nullDestinationPorts_pointingTo(KonfytJackPort* port);
    void nullDestinationPorts_all();
    QList<KonfytJackPort*>* getListContainingPort(int portId);
    void setPortClients(int portId, QStringList newClientList);
    void clearPortClients(int portId);
    void clearPortClients_andDisconnect(int portId);
    void addPortClient(int portId, QString newClient);
    void removePortClient_andDisconnect(int portId, QString cname);
    QStringList getPortClients(int portId);
    int getPortCount(KonfytJackPortType type);
    void setPortFilter(int portId, KonfytMidiFilter filter);
    void setPortSolo(int portId, bool solo);
    void setPortMute(int portId, bool mute);
    void setPortGain(int portId, float gain);
    void setPortRouting(int basePortId, int routePortId);

    void setPortActive(int portId, bool active);
    void setAllPortsActive(KonfytJackPortType type, bool active);

    // Carla plugins
    int addPluginPortsAndConnect(const KonfytJackPortsSpec &spec);
    void removePlugin(int id);
    void setPluginMidiFilter(int id, KonfytMidiFilter filter);
    void setPluginSolo(int id, bool solo);
    void setPluginMute(int id, bool mute);
    void setPluginGain(int id, float gain);
    void setPluginRouting(int pluginId, int midiInPortId,
                                  int leftPortId,
                                  int rightPortId);
    void setAllPluginPortsActive(bool active);

    // Fluidsynth
    int addSoundfont(LayerSoundfontStruct sf);
    void removeSoundfont(int id);
    void setSoundfontMidiFilter(int id, KonfytMidiFilter filter);
    void setSoundfontSolo(int id, bool solo);
    void setSoundfontMute(int id, bool mute);
    void setSoundfontRouting(int id, int midiInPortId,
                                     int leftPortId,
                                     int rightPortId);
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
    int idCounter;

    // Private port data structures
    QMap<int, KonfytJackPort*> portIdMap;

    jack_port_t* registerJackPort(KonfytJackPort* portStruct,
                                  jack_client_t *client,
                                  QString port_name,
                                  QString port_type,
                                  unsigned long flags,
                                  unsigned long buffer_size);

    // Timer for handling port register and connect callbacks
    QBasicTimer timer;
    void timerEvent(QTimerEvent *event);
    void startPortTimer();

    int globalTranspose;

    // Map plugin id to port structure
    QMap<int, KonfytJackPluginPorts*> pluginsPortsMap;
    QMap<int, KonfytJackPluginPorts*> soundfontPortsMap;

    // JACK helper function for internal use. Used by the public helper functions.
    QStringList getPortsList(QString typePattern, unsigned long flags);

    bool soloFlag_external; // Indicates if another engine as a solo active.

    void error_abort(QString msg);

signals:
    void userMessage(QString msg);
    void jackPortRegisterOrConnectCallback();
    void midiEventSignal();
    void xrunSignal();

    
public slots:
    
};

#endif // KONFYT_JACK_ENGINE_H
