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
    ~KonfytJackEngine();

    void panic(bool p);

    // Static JACK callback functions
    static void jackPortConnectCallback(jack_port_id_t a, jack_port_id_t b, int connect, void* arg);
    static void jackPortRegistrationCallback(jack_port_id_t port, int registered, void *arg);
    static int jackProcessCallback(jack_nframes_t nframes, void *arg);
    static int jackXrunCallback(void *arg);

    // Non-static JACK callback functions
    int jackProcessCallback(jack_nframes_t nframes);
    void jackPortConnectCallback();
    void jackPortRegistrationCallback();

    void setFluidsynthEngine(konfytFluidsynthEngine* e);

    QList<KonfytMidiEvent> getEvents();

    bool initJackClient(QString name);
    void stopJackClient();
    bool clientIsActive();
    QString clientName();
    void pauseJackProcessing(bool pause);
    double getSampleRate();

    // JACK helper functions (Not specific to our client)
    QStringList getMidiInputPortsList();
    QStringList getMidiOutputPortsList();
    QStringList getAudioInputPortsList();
    QStringList getAudioOutputPortsList();

    // Audio / midi in/out ports
    int addPort(KonfytJackPortType type, QString portName);
    void removePort(int portId);
    void removeAllAudioInAndOutPorts();
    void removeAllMidiInPorts();
    void removeAllMidiOutPorts();
    void setPortClients(int portId, QStringList newClientList);
    void clearPortClients(int portId);
    void clearPortClients_andDisconnect(int portId);
    void addPortClient(int portId, QString newClient);
    void removePortClient_andDisconnect(int portId, QString cname);
    QStringList getPortClients(int portId);
    int getPortCount(KonfytJackPortType type);
    void setPortFilter(int portId, KonfytMidiFilter filter);

    int addAudioRoute(int sourcePortId, int destPortId);
    int addAudioRoute();
    void setAudioRoute(int routeId, int sourcePortId, int destPortId);
    void removeAudioRoute(int routeId);
    void setAudioRouteActive(int routeId, bool active);
    void setAudioRouteGain(int routeId, float gain);

    int addMidiRoute(int sourcePortId, int destPortId);
    int addMidiRoute();
    void setMidiRoute(int routeId, int sourcePortId, int destPortId);
    void removeMidiRoute(int routeId);
    void setMidiRouteActive(int routeId, bool active);
    void setRouteMidiFilter(int routeId, KonfytMidiFilter filter);

    // Carla plugins
    int addPluginPortsAndConnect(const KonfytJackPortsSpec &spec);
    void removePlugin(int id);
    void setPluginMidiFilter(int id, KonfytMidiFilter filter);
    void setPluginActive(int id, bool active);
    void setPluginGain(int id, float gain);
    void setPluginRouting(int pluginId, int midiInPortId,
                                  int leftPortId,
                                  int rightPortId);

    // Fluidsynth
    int addSoundfont(const LayerSoundfontStruct &sf);
    void removeSoundfont(int id);
    void setSoundfontMidiFilter(int id, KonfytMidiFilter filter);
    void setSoundfontActive(int id, bool active);
    void setSoundfontRouting(int id, int midiInPortId,
                                     int leftPortId,
                                     int rightPortId);

    // Other JACK connections
    void addOtherJackConPair(KonfytJackConPair p);
    void removeOtherJackConPair(KonfytJackConPair p);
    void clearOtherJackConPair();

    void activateRoutesForPatch(const KonfytPatch *patch, bool active);

    void setGlobalTranspose(int transpose);


private:
    jack_client_t* client;
    jack_nframes_t nframes;
    bool clientActive = false;      // Flag to indicate if the client has been successfully activated
    double samplerate;
    RingbufferQMutex<KonfytMidiEvent> eventsBuffer;
    bool connectCallback = false;
    bool registerCallback = false;

    konfytFluidsynthEngine* fluidsynthEngine = NULL;

    bool panicCmd = false;  // Panic command from outside
    int panicState = 0; // Internal panic state

    QString ourJackClientName;
    int idCounter = 150;

    float *fadeOutValues;
    unsigned int fadeOutValuesCount = 0;
    float fadeOutSecs = 0;

    KonfytMidiEvent evAllNotesOff;
    KonfytMidiEvent evSustainZero;
    KonfytMidiEvent evPitchbendZero;
    void initMidiClosureEvents();

    volatile bool jack_process_busy = false;
    int jack_process_disable = 0; // Non-zero value = disabled
    bool timer_busy = false;
    bool timer_disabled = false;

    // Port data structures
    QList<KonfytJackPort*> midi_in_ports;
    QList<KonfytJackPort*> midi_out_ports;
    QList<KonfytJackPort*> audio_out_ports;
    QList<KonfytJackPort*> audio_in_ports;

    QList<KonfytJackPluginPorts*> plugin_ports;
    QList<KonfytJackPluginPorts*> fluidsynth_ports;

    // MIDI and audio routes (excluding plugin/fluidsynth routes)
    QList<KonfytJackMidiRoute*> midi_routes;
    QList<KonfytJackAudioRoute*> audio_routes;

    KonfytArrayList<KonfytJackNoteOnRecord> noteOnList;
    KonfytArrayList<KonfytJackNoteOnRecord> sustainList;
    KonfytArrayList<KonfytJackNoteOnRecord> pitchBendList;

    QMap<int, KonfytJackPort*> portIdMap;
    QMap<int, KonfytJackMidiRoute*> midiRouteIdMap;
    QMap<int, KonfytJackAudioRoute*> audioRouteIdMap;

    KonfytJackAudioRoute* audioRouteFromId(int routeId);
    KonfytJackMidiRoute* midiRouteFromId(int routeId);
    KonfytJackPort* audioInPortFromId(int portId);
    KonfytJackPort* audioOutPortFromId(int portId);
    KonfytJackPort* midiInPortFromId(int portId);
    KonfytJackPort* midiOutPortFromId(int portId);

    jack_port_t* registerJackPort(KonfytJackPort* portStruct,
                                  jack_client_t *client,
                                  QString port_name,
                                  QString port_type,
                                  unsigned long flags,
                                  unsigned long buffer_size);

    void removePortFromAllRoutes(KonfytJackPort* port);

    // Other JACK connections
    QList<KonfytJackConPair> otherConsList;

    // Timer for handling port register and connect callbacks
    QBasicTimer timer;
    void timerEvent(QTimerEvent *event);
    void startPortTimer();
    void refreshPortConnections();

    int globalTranspose = 0;

    // Map plugin id to port structure
    QMap<int, KonfytJackPluginPorts*> pluginsPortsMap;
    QMap<int, KonfytJackPluginPorts*> soundfontPortsMap;

    KonfytJackPluginPorts* pluginPortsFromId(int portId);
    KonfytJackPluginPorts* fluidsynthPortFromId(int portId);

    QStringList getPortsList(QString typePattern, unsigned long flags);
    QList<KonfytJackPort*>* getListContainingPort(int portId);

    // JACK process callback helper functions
    void handleNoteoffEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort* sourcePort);
    void handleSustainoffEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort *sourcePort);
    void handlePitchbendZeroEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KonfytJackPort *sourcePort);
    void mixBufferToDestinationPort(KonfytJackAudioRoute* route, jack_nframes_t nframes, bool applyGain);
    void sendMidiClosureEvents(KonfytJackPort* port, int channel);
    void sendMidiClosureEvents_chanZeroOnly(KonfytJackPort* port);
    void sendMidiClosureEvents_allChannels(KonfytJackPort* port);

    void error_abort(QString msg);

signals:
    void userMessage(QString msg);
    void jackPortRegisterOrConnectCallback();
    void midiEventSignal();
    void xrunSignal();
    
};

#endif // KONFYT_JACK_ENGINE_H
