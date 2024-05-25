/******************************************************************************
 *
 * Copyright 2024 Gideon van der Kolf
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

#ifndef KONFYTJS_H
#define KONFYTJS_H

#include "sleepyRingBuffer.h"
#include "konfytJackEngine.h"

#include <QJSEngine>
#include <QJsonObject>
#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QTimer>

#include <functional>


// =============================================================================

/* QJSEngine notes
 *
 * - To use QJSEngine:
 *
 *   Add to project (.pro) file:  QT += qml
 *
 *   Install QML/Qt Declarative development packages.
 *   For Ubuntu: qtdeclarative5-dev
 *
 * - QJSEngine must be created in the same thread it is used in.
 *
 *   Otherwise crashes happen as the engine's internal cleanup functionality
 *   running in the thread that created the engine conflicts with the use of the
 *   engine in the other thread.
 *
 *   Related links:
 *
 *   https://bugreports.qt.io/browse/QTBUG-83410
 *
 *   https://stackoverflow.com/questions/74120887/qjsengine-crashes-when-used-in-multithreading-program
 *
 *   https://github.com/nst1911/thread-safe-qjsengine
 *
 * - Objects given to QJSEngine should not be the parent of its parent.
 *
 *   If an object given to QJSEngine (i.e. with newQObject()) has a parent of
 *   which the object is also a parent (i.e. two objects are each other's
 *   parents), QJSEngine hangs during garbage collection.
 *
 * - Destroying a QJSEngine destroys all children.
 *
 *   If an object passed to QJSEngine with newQObject() should outlive QJSEngine,
 *   the object has to already have a parent to prevent QJSEngine from taking
 *   ownership and destroying the object when it is destroyed.
 */


// =============================================================================

/* This class can act as a temporary parent of QObjects to prevent other objects
 * from taking ownership and deleting the QObject. Upon destruction, this class
 * will remove all the temporary children so they aren't destroyed.
 * This allows the lifetimes of the QObjects to be handled manually or with
 * smart pointers, while still assigning a parent to prevent transfer of
 * ownership. */
class TempParent : public QObject
{
    Q_OBJECT
public:
    TempParent(QObject* parent = nullptr) : QObject(parent) {}
    ~TempParent();
    void makeTemporaryChildIfParentless(QObject* obj);
private:
    QList<QObject*> tempChildren;
    void removeTemporaryChildren();
};

// =============================================================================

class KonfytJSEnv; // Forward declaration for use in KonfytJSMidi

/* This is used as the global Midi object in the scripting environment.
 * Signals and slots are accessible from the script. */
class KonfytJSMidi : public QObject
{
    Q_OBJECT
public:
    KonfytJSMidi(KonfytJSEnv* parent = nullptr);

    QJSValue midiEventToJSObject(const KonfytMidiEvent& ev);
    KonfytMidiEvent jsObjectToMidiEvent(QJSValue j);

signals:
    void send(QJSValue event);

public slots:
    QJSValue noteon(int channel, int note, int velocity);
    QJSValue noteoff(int channel, int note, int velocity = 0);
    QJSValue cc(int channel, int cc, int value);
    QJSValue program(int channel, int program, int bankmsb = -1, int banklsb = -1);
    QJSValue pitchbend(int channel, int value);

private:
    KonfytJSEnv* jsEnv;

    /* Convenience function to get a property of a QJSValue or a default value
     * if the property does not exist (or is undefined). */
    QVariant value(const QJSValue& j, QString key, QVariant defaultValue);
};

// =============================================================================

/* Javascript environment for a script. */
class KonfytJSEnv : public QObject
{
    Q_OBJECT
public:
    explicit KonfytJSEnv(QObject* parent = nullptr);

    void setScriptAndInitIfEnabled(QString script, bool enable);
    void setEnabledAndInitIfNeeded(bool enable);
    bool isEnabled();
    void runProcess();
    QString script();

    void addEvent(KonfytMidiEvent ev);
    int eventCount();

    QJSEngine* jsEngine();
    float getAverageProcessTimeMs();
    QString errorString();

    QString uri;

    QStringList takePrints();
    QList<KonfytMidiEvent> takeMidiToSend();

signals:
    void errorStatusChanged(QString errorString);

public slots:
    void sendMidi(QJSValue j);
    // For use from script:
    void print(QString msg);

private:
    // NB: JS garbage collector doesn't like it if tempParent's parent is this
    // class, and this class is tempParent's parent (i.e. circular reference).
    TempParent tempParent;

    KonfytJSMidi midi {this};
    QScopedPointer<QJSEngine> js;
    QJSValue jsSys; // "Sys" object in script environment
    QJSValue jsMidi; // "Midi" object in script environment
    QJSValue jsMidiEventFunction;

    bool mEnabled = false;
    QString mErrorString;

    static const int processTimesSize = 10;
    qint64 processTimesNs[processTimesSize] = {0};
    int iProcessTimes = 0;
    int processTimesCount = 0;
    qint64 sumProcessTimesNs = 0;
    void addProcessTime(qint64 timeNs);

    void resetEnvironment();
    bool evaluate(QString script);
    bool handleJsResult(QJSValue result);

    QString mScript;
    bool scriptInitialisationDone = false;
    void runScriptInitialisation();

    QList<KonfytMidiEvent> midiEvents;
    QJSValue jsMidiRxArray;

    static const int PRINT_MAX = 1000;
    QStringList mToPrint;
    static const int MIDI_SEND_MAX = 1000;
    QList<KonfytMidiEvent> mMidiToSend;

    // Constants
    const QString TYPE_NOTEON = "noteon";
    const QString TYPE_NOTEOFF = "noteoff";
    const QString TYPE_POLY_AFTERTOUCH = "polyaftertouch";
    const QString TYPE_CC = "cc";
    const QString TYPE_PROGRAM = "program";
    const QString TYPE_AFTERTOUCH = "aftertouch";
    const QString TYPE_PITCHBEND = "pitchbend";
    const QString TYPE_SYSEX = "sysex";
};

// =============================================================================

/* Engine that manages all the scripts and routes MIDI events between scripts
 * and the JackEngine.
 * Patch layers (and their scripts) are added from the GUI. The JackEngine
 * communicates received MIDI events through a shared thread-safe ringbuffer and
 * wakes this thread up with a signal. The received MIDI events are paired up
 * with the patch layers using the JackEngine routes. For each received MIDI
 * event, the appropriate patch layer script is then run. */
class KonfytJSEngine : public QObject
{
    Q_OBJECT
public:
    KonfytJSEngine(QObject* parent= nullptr);
    void setJackEngine(KonfytJackEngine* jackEngine);

    void addOrUpdateLayerScript(KfPatchLayerSharedPtr patchLayer);
    void addJackPortScript(PrjMidiPortPtr prjPort);
    void removeLayerScript(KfPatchLayerSharedPtr patchLayer);
    void removeJackPortScript(PrjMidiPortPtr prjPort);
    QString script(KfPatchLayerSharedPtr patchLayer);
    QString script(PrjMidiPortPtr prjPort);
    void setScriptEnabled(KfPatchLayerSharedPtr patchLayer, bool enable);
    void setScriptEnabled(PrjMidiPortPtr prjPort, bool enable);
    float scriptAverageProcessTimeMs(KfPatchLayerSharedPtr patchLayer);
    float scriptAverageProcessTimeMs(PrjMidiPortPtr prjPort);
    QString scriptErrorString(KfPatchLayerSharedPtr patchLayer);
    QString scriptErrorString(PrjMidiPortPtr prjPort);

    void updatePatchLayerURIs();
    void updateProjectPortURIs();

signals:
    void print(QString msg);
    void layerScriptErrorStatusChanged(KfPatchLayerSharedPtr patchLayer,
                                       QString errorString);
    void portScriptErrorStatusChanged(PrjMidiPortPtr prjPort, QString errorString);

private:
    void runInThisThread(std::function<void()> func);

    KonfytJackEngine* jack = nullptr;
    QSharedPointer<SleepyRingBuffer<KfJackMidiRxEvent>> mRxBuffer;

    struct ScriptEnv {
        KonfytJSEnv env;
        // The script is either for a port or layer. Whichever is applicable
        // will not be null.
        PrjMidiPortPtr prjPort;
        KfJackMidiRoute* route = nullptr;
    };
    typedef QSharedPointer<ScriptEnv> ScriptEnvPtr;

    QMap<KfJackMidiRoute*, ScriptEnvPtr> routeEnvMap;
    QMap<KfPatchLayerSharedPtr, ScriptEnvPtr> layerEnvMap;

    QMap<KfJackMidiPort*, ScriptEnvPtr> jackPortEnvMap;
    QMap<PrjMidiPortPtr, ScriptEnvPtr> prjPortEnvMap;

    // Lists for use in GUI thread. These are kept in sync with the maps above
    // used in the JS thread. Separate containers are used in different threads
    // to prevent race conditions.
    QSet<KfPatchLayerSharedPtr> layers_guiThread;
    QSet<PrjMidiPortPtr> ports_guiThread;

    ScriptEnvPtr runningScript;
    const int watchdogMax = 4;
    int watchdog = watchdogMax;
    void beforeScriptRun(ScriptEnvPtr s);
    void afterScriptRun(ScriptEnvPtr s);

    KfJackMidiRoute* jackMidiRouteFromLayer(KfPatchLayerSharedPtr layer);

private slots:
    void onNewMidiEventsAvailable();
};

#endif // KONFYTJS_H
