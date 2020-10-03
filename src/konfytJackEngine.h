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

#ifndef KONFYT_JACK_ENGINE_H
#define KONFYT_JACK_ENGINE_H

#include "konfytArrayList.h"
#include "konfytDefines.h"
#include "konfytFluidsynthEngine.h"
#include "konfytJackStructs.h"
#include "konfytProject.h"
#include "konfytStructs.h"
#include "ringbufferqmutex.h"

#include <jack/jack.h>
#include <jack/midiport.h>

#include <QBasicTimer>
#include <QObject>
#include <QStringList>
#include <QTimerEvent>


#define KONFYT_JACK_DEFAULT_CLIENT_NAME "Konfyt"   // Default client name. Actual name is set in the Jack client.
#define KONFYT_JACK_SYSTEM_OUT_LEFT "system:playback_1"
#define KONFYT_JACK_SYSTEM_OUT_RIGHT "system:playback_2"

#define KONFYT_JACK_SUSTAIN_THRESH 63

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

    void setFluidsynthEngine(KonfytFluidsynthEngine* e);

    QList<KfJackMidiRxEvent> getEvents();

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
    KfJackMidiPort* addMidiPort(QString name, bool isInput);
    KfJackAudioPort* addAudioPort(QString name, bool isInput);
    void removeMidiPort(KfJackMidiPort* port);
    void removeAudioPort(KfJackAudioPort *port);
    void removeAllAudioInAndOutPorts();
    void removeAllMidiInAndOutPorts();
    void clearPortClients(KfJackMidiPort *port);
    void clearPortClients(KfJackAudioPort *port);
    void addPortClient(KfJackMidiPort *port, QString newClient);
    void addPortClient(KfJackAudioPort *port, QString newClient);
    void removeAndDisconnectPortClient(KfJackMidiPort *port, QString mJackClient);
    void removeAndDisconnectPortClient(KfJackAudioPort *port, QString mJackClient);
    void setPortFilter(KfJackMidiPort *port, KonfytMidiFilter filter);

    // Audio routes
    KfJackAudioRoute* addAudioRoute(KfJackAudioPort* sourcePort, KfJackAudioPort* destPort);
    KfJackAudioRoute* addAudioRoute();
    void setAudioRoute(KfJackAudioRoute *route, KfJackAudioPort* sourcePort, KfJackAudioPort* destPort);
    void removeAudioRoute(KfJackAudioRoute *route);
    void setAudioRouteActive(KfJackAudioRoute *route, bool active);
    void setAudioRouteGain(KfJackAudioRoute *route, float gain);

    // MIDI routes
    KfJackMidiRoute* addMidiRoute(KfJackMidiPort *sourcePort, KfJackMidiPort *destPort);
    KfJackMidiRoute* addMidiRoute();
    void setMidiRoute(KfJackMidiRoute* route, KfJackMidiPort* sourcePort, KfJackMidiPort* destPort);
    void removeMidiRoute(KfJackMidiRoute *route);
    void setMidiRouteActive(KfJackMidiRoute *route, bool active);
    void setRouteMidiFilter(KfJackMidiRoute *route, KonfytMidiFilter filter);
    bool sendMidiEventsOnRoute(KfJackMidiRoute *route, QList<KonfytMidiEvent> events);

    // SFZ plugins
    KfJackPluginPorts* addPluginPortsAndConnect(const KonfytJackPortsSpec &spec);
    void removePlugin(KfJackPluginPorts *p);
    void setPluginMidiFilter(KfJackPluginPorts *p, KonfytMidiFilter filter);
    void setPluginActive(KfJackPluginPorts *p, bool active);
    void setPluginGain(KfJackPluginPorts *p, float gain);
    void setPluginRouting(KfJackPluginPorts *p, KfJackMidiPort *midiInPort,
                                  KfJackAudioPort *leftPort,
                                  KfJackAudioPort *rightPort);

    // Fluidsynth
    KfJackPluginPorts* addSoundfont(KfFluidSynth* fluidSynth);
    void removeSoundfont(KfJackPluginPorts *p);
    void setSoundfontMidiFilter(KfJackPluginPorts *p, KonfytMidiFilter filter);
    void setSoundfontActive(KfJackPluginPorts *p, bool active);
    void setSoundfontRouting(KfJackPluginPorts *p, KfJackMidiPort *midiInPort,
                                     KfJackAudioPort *leftPort,
                                     KfJackAudioPort *rightPort);

    // Other JACK connections
    void addOtherJackConPair(KonfytJackConPair p);
    void removeOtherJackConPair(KonfytJackConPair p);
    void clearOtherJackConPair();

    void setGlobalTranspose(int transpose);

signals:
    void userMessage(QString msg);
    void jackPortRegisteredOrConnected();
    void midiEventsReceived();
    void xrunOccurred();

private:
    jack_client_t* mJackClient;
    jack_nframes_t nframes;
    bool clientActive = false;      // Flag to indicate if the client has been successfully activated
    double samplerate;
    RingbufferQMutex<KfJackMidiRxEvent> eventsRxBuffer{1000};
    QList<KfJackMidiRxEvent> extractedRxEvents;
    bool connectCallback = false;
    bool registerCallback = false;

    KonfytFluidsynthEngine* fluidsynthEngine = nullptr;

    bool panicCmd = false;  // Panic command from outside
    int panicState = 0; // Internal panic state

    QString ourJackClientName;

    float *fadeOutValues;
    unsigned int fadeOutValuesCount = 0;
    float fadeOutSecs = 0;

    KonfytMidiEvent evAllNotesOff;
    KonfytMidiEvent evSustainZero;
    KonfytMidiEvent evPitchbendZero;
    void initMidiClosureEvents();

    QMutex jackProcessMutex;
    int jackProcessLocks = 0;

    // Port data structures
    QList<KfJackMidiPort*> midiInPorts;
    QList<KfJackMidiPort*> midiOutPorts;
    QList<KfJackAudioPort*> audioOutPorts;
    QList<KfJackAudioPort*> audioInPorts;

    QList<KfJackPluginPorts*> pluginPorts;
    QList<KfJackPluginPorts*> fluidsynthPorts;

    // MIDI and audio routes
    QList<KfJackMidiRoute*> midiRoutes;
    QList<KfJackAudioRoute*> audioRoutes;

    KonfytArrayList<KonfytJackNoteOnRecord> noteOnList;
    KonfytArrayList<KonfytJackNoteOnRecord> sustainList;
    KonfytArrayList<KonfytJackNoteOnRecord> pitchBendList;

    jack_port_t* registerJackMidiPort(QString name, bool input);
    jack_port_t* registerJackAudioPort(QString name, bool input);

    void removePortFromAllRoutes(KfJackMidiPort* port);
    void removePortFromAllRoutes(KfJackAudioPort* port);

    // Other JACK connections
    QList<KonfytJackConPair> otherConsList;

    // Timer for communicating data from JACK process to the rest of the app as
    // well as restoring JACK port connections.
    QBasicTimer timer;
    void timerEvent(QTimerEvent *event);
    void startTimer();
    void refreshAllPortsConnections();
    void refreshConnections(jack_port_t* jackPort, QStringList clients, bool inNotOut);

    int globalTranspose = 0;

    QStringList getJackPorts(QString typePattern, unsigned long flags);

    // JACK process callback helper functions
    void handleNoteoffEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KfJackMidiPort* sourcePort);
    void handleSustainoffEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KfJackMidiPort *sourcePort);
    void handlePitchbendZeroEvent(KonfytMidiEvent &ev, jack_midi_event_t inEvent_jack, KfJackMidiPort *sourcePort);
    void mixBufferToDestinationPort(KfJackAudioRoute* route, jack_nframes_t nframes, bool applyGain);
    void sendMidiClosureEvents(KfJackMidiPort* port, int channel);
    void sendMidiClosureEvents_chanZeroOnly(KfJackMidiPort* port);
    void sendMidiClosureEvents_allChannels(KfJackMidiPort* port);

    void error_abort(QString msg);
};



#endif // KONFYT_JACK_ENGINE_H
