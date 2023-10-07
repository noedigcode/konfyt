/******************************************************************************
 *
 * Copyright 2022 Gideon van der Kolf
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
#include "sleepyRingBuffer.h"

#include <jack/jack.h>
#include <jack/midiport.h>

#include <QBasicTimer>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimerEvent>


// Default client name. Actual name is set in the JACK client.
#define KONFYT_JACK_DEFAULT_CLIENT_NAME "Konfyt"
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

    QList<KfJackMidiRxEvent> getMidiRxEvents();
    QList<KfJackAudioRxEvent> getAudioRxEvents();

    bool initJackClient(QString name);
    void stopJackClient();
    bool clientIsActive();
    QString clientName(); // Actual client name as assigned by JACK
    QString clientBaseName(); // Client name requested from JACK, before change for uniqueness
    void pauseJackProcessing(bool pause);
    uint32_t getSampleRate();
    uint32_t getBufferSize();

    // JACK helper functions (Not specific to our client)
    QStringList getMidiInputPortsList();
    QStringList getMidiOutputPortsList();
    QStringList getAudioInputPortsList();
    QStringList getAudioOutputPortsList();
    QSet<QString> getJackClientsList();

    // Audio / MIDI in/out ports
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
    void setPortGain(KfJackAudioPort *port, float gain);

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
    void setRouteMidiPreFilter(KfJackMidiRoute *route, KonfytMidiFilter filter);
    bool sendMidiEventsOnRoute(KfJackMidiRoute *route, QList<KonfytMidiEvent> events);
    void setRouteBlockMidiDirectThrough(KfJackMidiRoute* route, bool block);

    // SFZ plugins
    KfJackPluginPorts* addPluginPortsAndConnect(const KonfytJackPortsSpec &spec);
    void removePlugin(KfJackPluginPorts *p);
    void setPluginMidiFilter(KfJackPluginPorts *p, KonfytMidiFilter filter);
    void setPluginMidiPreFilter(KfJackPluginPorts *p, KonfytMidiFilter filter);
    void setPluginActive(KfJackPluginPorts *p, bool active);
    void setPluginGain(KfJackPluginPorts *p, float gain);
    void setPluginRouting(KfJackPluginPorts *p, KfJackMidiPort *midiInPort,
                                  KfJackAudioPort *leftPort,
                                  KfJackAudioPort *rightPort);
    KfJackMidiRoute* getPluginMidiRoute(KfJackPluginPorts* p);
    QList<KfJackAudioRoute*> getPluginAudioRoutes(KfJackPluginPorts* p);
    void setPluginBlockMidiDirectThrough(KfJackPluginPorts* p, bool block);

    // Fluidsynth
    KfJackPluginPorts* addSoundfont(KfFluidSynth* fluidSynth);
    void removeSoundfont(KfJackPluginPorts *p);
    void setSoundfontMidiFilter(KfJackPluginPorts *p, KonfytMidiFilter filter);
    void setSoundfontMidiPreFilter(KfJackPluginPorts *p, KonfytMidiFilter filter);
    void setSoundfontActive(KfJackPluginPorts *p, bool active);
    void setSoundfontRouting(KfJackPluginPorts *p, KfJackMidiPort *midiInPort,
                                     KfJackAudioPort *leftPort,
                                     KfJackAudioPort *rightPort);
    void setSoundfontBlockMidiDirectThrough(KfJackPluginPorts* p, bool block);

    // Other JACK connections
    void addOtherJackConPair(KonfytJackConPair p);
    void removeOtherJackConPair(KonfytJackConPair p);
    void clearOtherJackConPair();

    void setGlobalTranspose(int transpose);

    QSharedPointer<SleepyRingBuffer<KfJackMidiRxEvent>> getMidiRxBufferForJs();

signals:
    void print(QString msg);
    void jackPortRegisteredOrConnected();
    void midiEventsReceived();
    void audioEventsReceived();
    void xrunOccurred();

    void newMidiEventsAvailable();

private:
    jack_client_t* mJackClient;
    jack_nframes_t mJackBufferSize; // TODO THIS MIGHT CHANGE, REGISTER BUFSIZE CALLBACK TO UPDATE
    bool mClientActive = false; // True when the Jack client has been successfully activated
    uint32_t mJackSampleRate;
    bool mConnectCallback = false;
    bool mRegisterCallback = false;
    uint32_t mLastSentMidiEventTime = 0;

    // MIDI data received from JACK thread
    RingbufferQMutex<KfJackMidiRxEvent> midiRxBuffer{1000};
    QList<KfJackMidiRxEvent> extractedMidiRx;

    QSharedPointer<SleepyRingBuffer<KfJackMidiRxEvent>> midiRxBufferForJs {
        new SleepyRingBuffer<KfJackMidiRxEvent>(1000) };
    bool midiForJsWritten = false;

    // Audio data received from JACK thread
    int mAudioBufferSumCycleCount = 100;
    RingbufferQMutex<KfJackAudioRxEvent> audioRxBuffer{1000};
    QList<KfJackAudioRxEvent> extractedAudioRx;

    KonfytFluidsynthEngine* fluidsynthEngine = nullptr;

    bool panicCmd = false;  // Panic command from outside
    enum PanicState { NoPanic, EnterPanicState, InPanicState };
    PanicState panicState = NoPanic;

    QString mJackClientName; // Actual client name as assigned by JACK
    QString mJackClientBaseName; // Requested JACK client name before change for uniqueness

    float* fadeOutValues = nullptr;
    unsigned int fadeOutValuesCount = 0;
    float fadeOutSecs = 1.0;

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
    enum PortDirection { INPUT_PORT, OUTPUT_PORT };
    void refreshConnections(jack_port_t* jackPort, QStringList clients,
                            PortDirection dir);

    int mGlobalTranspose = 0;

    QStringList getJackPorts(QString typePattern, unsigned long flags);

    // JACK process callback helper functions
    void writeRouteMidi(KfJackMidiRoute* route, KonfytMidiEvent &ev, jack_nframes_t time);
    bool handleNoteoffEvent(const KonfytMidiEvent& ev, KfJackMidiRoute* route, jack_nframes_t time);
    void mixBufferToDestinationPort(KfJackAudioRoute* route, jack_nframes_t nframes, bool applyGain);
    void sendMidiClosureEvents(KfJackMidiPort* port, int channel);
    void sendMidiClosureEvents_chanZeroOnly(KfJackMidiPort* port);
    void sendMidiClosureEvents_allChannels(KfJackMidiPort* port);
    void handleBankSelect(int bankMSB[16], int bankLSB[16], KonfytMidiEvent* ev);
    void* getJackPortBuffer(jack_port_t *port, jack_nframes_t nframes) const;
    jack_midi_data_t* reserveJackMidiEvent(void *portBuffer,
                                           jack_nframes_t time,
                                           size_t size);
    void jackProcess_prepareAudioPortBuffers(jack_nframes_t nframes);
    void jackProcess_processAudioRoutes(jack_nframes_t nframes);
    void jackProcess_prepareMidiOutBuffers(jack_nframes_t nframes);
    void jackProcess_midiPanicOutput();
    void jackProcess_processMidiInPorts(jack_nframes_t nframes);
    void jackProcess_sendMidiRouteTxEvents(jack_nframes_t nframes);
};



#endif // KONFYT_JACK_ENGINE_H
