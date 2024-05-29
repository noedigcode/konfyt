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

#include "konfytJs.h"


void AverageTimer::start()
{
    elapsedTimer.start();
}

void AverageTimer::recordTime()
{
    qint64 elapsedNs = elapsedTimer.nsecsElapsed();

    qint64 sum = sumProcessTimesNs;
    sum -= processTimesNs[iProcessTimes];
    processTimesNs[iProcessTimes] = elapsedNs;
    sum += elapsedNs;
    iProcessTimes = (iProcessTimes + 1) % processTimesSize;
    if (processTimesCount < processTimesSize) { processTimesCount++; }
    sumProcessTimesNs = sum;
}

float AverageTimer::getAverageProcessTimeMs()
{
    if (processTimesCount) {
        return sumProcessTimesNs / processTimesCount / 1000000.0;
    } else {
        return 0;
    }
}

KonfytJSMidi::KonfytJSMidi(KonfytJSEnv* parent) : QObject(parent)
{
    jsEnv = parent;
}

QJSValue KonfytJSMidi::midiEventToJSObject(const KonfytMidiEvent& ev)
{
    QJSEngine* js = jsEnv->jsEngine();
    QJSValue j = js->newObject();

    QString type;
    switch (ev.type()) {
    case MIDI_EVENT_TYPE_NOTEON:
        type = "noteon";
        j.setProperty("note", ev.data1());
        j.setProperty("velocity", ev.data2());
        break;
    case MIDI_EVENT_TYPE_NOTEOFF:
        type = "noteoff";
        j.setProperty("note", ev.data1());
        j.setProperty("velocity", ev.data2());
        break;
    case MIDI_EVENT_TYPE_POLY_AFTERTOUCH:
        type = "polyaftertouch";
        j.setProperty("note", ev.data1());
        j.setProperty("pressure", ev.data2());
        break;
    case MIDI_EVENT_TYPE_CC:
        type = "cc";
        j.setProperty("cc", ev.data1());
        j.setProperty("value", ev.data2());
        break;
    case MIDI_EVENT_TYPE_PROGRAM:
        type = "program";
        j.setProperty("program", ev.data1());
        j.setProperty("bankmsb", ev.bankMSB);
        j.setProperty("banklsb", ev.bankLSB);
        break;
    case MIDI_EVENT_TYPE_AFTERTOUCH:
        type = "aftertouch";
        j.setProperty("pressure", ev.data1());
        break;
    case MIDI_EVENT_TYPE_PITCHBEND:
        type = "pitchbend";
        j.setProperty("value", ev.pitchbendValueSigned());;
        break;
    case MIDI_EVENT_TYPE_SYSTEM:
        type = "sysex";
        j.setProperty("data", ev.dataToHexString());
        break;
    }
    j.setProperty("type", type);
    // Scripting channels are 1-16, KonfytMidiEvent uses 0-15
    j.setProperty("channel", ev.channel + 1);

    return j;
}

KonfytMidiEvent KonfytJSMidi::jsObjectToMidiEvent(QJSValue j)
{
    KonfytMidiEvent ev;

    // Scripting channels are 1-16, KonfytMidiEvent uses 0-15
    ev.channel = value(j, "channel", 0).toInt() - 1;
    QString type = value(j, "type", "noteon").toString();
    if (type == "noteon") {
        int note = value(j, "note", 0).toInt();
        int velocity = value(j, "velocity", 0).toInt();
        ev.setNoteOn(note, velocity);
    } else if (type == "noteoff") {
        int note = value(j, "note", 0).toInt();
        int velocity = value(j, "velocity", 0).toInt();
        ev.setNoteOff(note, velocity);
    } else if (type == "cc") {
        int cc = value(j, "cc", 0).toInt();
        int val = value(j, "value", 0).toInt();
        ev.setCC(cc, val);
    } else if (type == "program") {
        int program = value(j, "program", 0).toInt();
        ev.setProgram(program);
        ev.bankLSB = value(j, "banklsb", -1).toInt();
        ev.bankMSB = value(j, "bankmsb", -1).toInt();
    } else if (type == "pitchbend") {
        int pb = value(j, "value", MIDI_PITCHBEND_ZERO).toInt();
        ev.setPitchbend(pb);
    } else if (type == "polyaftertouch") {
        int note = value(j, "note", 0).toInt();
        int pres = value(j, "pressure", 0).toInt();
        ev.setPolyAftertouch(note, pres);
    } else if (type == "aftertouch") {
        int pres = value(j, "pressure", 0).toInt();
        ev.setAftertouch(pres);
    } else if (type == "sysex") {
        QString data = value(j, "data", "").toString();
        ev.setDataFromHexString(data);
        ev.setType(MIDI_EVENT_TYPE_SYSTEM);
    }

    return ev;
}

QJSValue KonfytJSMidi::noteon(int channel, int note, int velocity)
{
    KonfytMidiEvent ev;
    ev.channel = channel - 1;
    ev.setNoteOn(note, velocity);
    return midiEventToJSObject(ev);
}

QJSValue KonfytJSMidi::noteoff(int channel, int note, int velocity)
{
    KonfytMidiEvent ev;
    ev.channel = channel - 1;
    ev.setNoteOff(note, velocity);
    return midiEventToJSObject(ev);
}

QJSValue KonfytJSMidi::cc(int channel, int cc, int value)
{
    KonfytMidiEvent ev;
    ev.channel = channel - 1;
    ev.setCC(cc, value);
    return midiEventToJSObject(ev);
}

QJSValue KonfytJSMidi::program(int channel, int program, int bankmsb, int banklsb)
{
    KonfytMidiEvent ev;
    ev.channel = channel - 1;
    ev.setProgram(program);
    ev.bankMSB = bankmsb;
    ev.bankLSB = banklsb;
    return midiEventToJSObject(ev);
}

QJSValue KonfytJSMidi::pitchbend(int channel, int value)
{
    KonfytMidiEvent ev;
    ev.channel = channel - 1;
    ev.setPitchbend(value);
    return midiEventToJSObject(ev);
}

QVariant KonfytJSMidi::value(const QJSValue& j, QString key, QVariant defaultValue)
{
    QJSValue jv = j.property(key);
    if (jv.isUndefined()) { return defaultValue; }
    else { return jv.toVariant(); }
}

KonfytJSEnv::KonfytJSEnv(QObject* parent) : QObject{parent}
{
    connect(&midi, &KonfytJSMidi::send, this, &KonfytJSEnv::sendMidi);
}

void KonfytJSEnv::setScriptAndInitIfEnabled(QString script, bool enable)
{
    scriptInitialisationDone = false;
    resetEnvironment();
    mScript = script;

    setEnabledAndInitIfNeeded(enable);
}

void KonfytJSEnv::setEnabledAndInitIfNeeded(bool enable)
{
    if (enable) {
        if (js && !scriptInitialisationDone) {
            runScriptInitialisation();
            scriptInitialisationDone = true;
        }
    }

    // Enabled state will be taken into account in runProcess()
    mEnabled = enable;
}

bool KonfytJSEnv::isEnabled()
{
    return mEnabled;
}

void KonfytJSEnv::runProcess()
{
    if (!mEnabled) { return; }

    totalProcessTimer.start();

    foreach (const KonfytMidiEvent& ev, midiEvents) {
        eventProcessTimer.start();
        QJSValue j = midi.midiEventToJSObject(ev);
        if (!handleJsResult( jsMidiEventFunction.call({j}) )) {
            break;
        }
        eventProcessTimer.recordTime();
    }
    midiEvents.clear();

    totalProcessTimer.recordTime();
}

QString KonfytJSEnv::script()
{
    return mScript;
}

void KonfytJSEnv::addEvent(KonfytMidiEvent ev)
{
    midiEvents.append(ev);
}

int KonfytJSEnv::eventCount()
{
    return midiEvents.count();
}

QJSEngine* KonfytJSEnv::jsEngine()
{
    return js.data();
}

float KonfytJSEnv::getAverageTotalProcessTimeMs()
{
    return totalProcessTimer.getAverageProcessTimeMs();
}

float KonfytJSEnv::getAveragePerEventProcessTimeMs()
{
    return eventProcessTimer.getAverageProcessTimeMs();
}

QString KonfytJSEnv::errorString()
{
    return mErrorString;
}

QStringList KonfytJSEnv::takePrints()
{
    QStringList ret = mToPrint;
    mToPrint.clear();
    return ret;
}

QList<KonfytMidiEvent> KonfytJSEnv::takeMidiToSend()
{
    QList<KonfytMidiEvent> ret = mMidiToSend;
    mMidiToSend.clear();
    return ret;
}

void KonfytJSEnv::sendMidi(QJSValue j)
{
    // Add MIDI message to the queue, to be handled later outside this class,
    // obtained with takeMidiToSend().
    // A queue with a max limit is used to prevent infinite loops clogging up
    // the signals system and stalling the application.

    KonfytMidiEvent ev = midi.jsObjectToMidiEvent(j);
    if (mMidiToSend.count() < MIDI_SEND_MAX) {
        mMidiToSend.append(ev);
        if (mMidiToSend.count() == MIDI_SEND_MAX) {
            mToPrint.append(QString("Maximum MIDI send count of %1 reached.")
                            .arg(MIDI_SEND_MAX));
        }
    }
}

void KonfytJSEnv::print(QString msg)
{
    // Add print message to the queue, to be handled later outside this class,
    // obtained with takePrints().
    // A queue with a max limit is used to prevent infinite loops clogging up
    // the signals system and stalling the application.

    if (mToPrint.count() < PRINT_MAX) {
        mToPrint.append(msg);
        if (mToPrint.count() == PRINT_MAX) {
            mToPrint.append(QString("Maximum print count of %1 reached.")
                            .arg(PRINT_MAX));
        }
    }
}

void KonfytJSEnv::resetEnvironment()
{
    // This function must only be run from the KonfytJS thread as it creates the
    // QJSEngine which must be created in the KonfytJS thread.

    // NB: Destroying QJSEngine destroys all children. Ensure all objects passed
    // to it using newQObject had parents already so QJSEngine doesn't destroy them.

    js.reset(new QJSEngine());

    // Add this class as global system object
    // NB: Ensure object passed to newQObject has a parent so QJSEngine doesn't
    // take ownership.
    tempParent.makeTemporaryChildIfParentless(this);
    jsSys = js->newQObject(this);
    js->globalObject().setProperty("Sys", jsSys);
    js->globalObject().setProperty("print", jsSys.property("print"));

    jsMidi = js->newQObject(&midi);
    js->globalObject().setProperty("Midi", jsMidi);

    // Add MIDI event type constants as global objects
    js->globalObject().setProperty("NOTEON", TYPE_NOTEON);
    js->globalObject().setProperty("NOTEOFF", TYPE_NOTEOFF);
    js->globalObject().setProperty("POLYAFTERTOUCH", TYPE_POLY_AFTERTOUCH);
    js->globalObject().setProperty("CC", TYPE_CC);
    js->globalObject().setProperty("PROGRAM", TYPE_PROGRAM);
    js->globalObject().setProperty("AFTERTOUCH", TYPE_AFTERTOUCH);
    js->globalObject().setProperty("PITCHBEND", TYPE_PITCHBEND);
    js->globalObject().setProperty("SYSEX", TYPE_SYSEX);

    // Other constants
    js->globalObject().setProperty("PITCHBEND_MAX", MIDI_PITCHBEND_SIGNED_MAX);
    js->globalObject().setProperty("PITCHBEND_MIN", MIDI_PITCHBEND_SIGNED_MIN);
}

bool KonfytJSEnv::evaluate(QString script)
{
    return handleJsResult( js->evaluate(script) );
}

bool KonfytJSEnv::handleJsResult(QJSValue result)
{
    if (result.isError()) {

        if (js->isInterrupted()) {
            mErrorString = "Script took too long to execute";
        } else {
            mErrorString = QString("Line %1: %2")
                    .arg(result.property("lineNumber").toInt())
                    .arg(result.toString());
        }

        print("*****************************");
        print("Script stopped due to error:");
        print(mErrorString);
        print("*****************************");

        setEnabledAndInitIfNeeded(false);
        emit errorStatusChanged(mErrorString);
        return false;

    } else {

        if (!mErrorString.isEmpty()) {
            mErrorString = "";
            emit errorStatusChanged(mErrorString);
        }

        return true;
    }
}

void KonfytJSEnv::runScriptInitialisation()
{
    evaluate(mScript);
    jsMidiEventFunction = js->globalObject().property("midiEvent");
    if (evaluate("init()")) {
        print("Script initialised.");
    }
}

TempParent::~TempParent()
{
    removeTemporaryChildren();
}

void TempParent::makeTemporaryChildIfParentless(QObject *obj)
{
    /* Set this class as the specified QObject's parent if it does not have a parent.
     * Call removeTemporaryChildren() in this class' destructor to remove it again,
     * so it isn't destroyed with this class. */
    if (obj->parent() == nullptr) {
        obj->setParent(this);
        tempChildren.append(obj);
    }
}

void TempParent::removeTemporaryChildren()
{
    /* Remove all QObjects as children of this class that were added using the
     * makeTemporaryChildIfParentless() function. */
    foreach (QObject* obj, tempChildren) {
        if (this->children().contains(obj)) {
            obj->setParent(nullptr);
        }
    }
}

KonfytJSEngine::KonfytJSEngine(QObject *parent) : QObject(parent)
{
    // For signal scriptErrorStatusChanged()
    qRegisterMetaType<KfPatchLayerSharedPtr>("KfPatchLayerSharedPtr");
    qRegisterMetaType<PrjMidiPortPtr>("PrjMidiPortPtr");

    // Start script watchdog timer in calling thread (i.e. before this
    // object has been moved to another thread, so not the scripting thread).

    QTimer* t = new QTimer();
    connect(t, &QTimer::timeout, [=]()
    {
        if (runningScript) {
            watchdog--;
            print(QString("Watchdog strike %1").arg(watchdogMax - watchdog));
            if (watchdog <= 0) {
                runningScript->env.jsEngine()->setInterrupted(true);
                print("Watchdog stopping script");
            }
        }
    });
    t->start(500);
}

void KonfytJSEngine::setJackEngine(KonfytJackEngine *jackEngine)
{
    jack = jackEngine;
    mRxBuffer = jack->getMidiRxBufferForJs();

    connect(jack, &KonfytJackEngine::newMidiEventsAvailable,
            this, &KonfytJSEngine::onNewMidiEventsAvailable,
            Qt::QueuedConnection);
}

void KonfytJSEngine::addOrUpdateLayerScript(KfPatchLayerSharedPtr patchLayer)
{
    if (!patchLayer) {
        print("Error: addLayerScript: null patchLayer");
        return;
    }

    // Extract required info as we don't want to access patchLayer members in
    // the other thread to prevent race conditions
    QString script = patchLayer->script();
    bool isScriptEnabled = patchLayer->isScriptEnabled();
    KfJackMidiRoute* route = jackMidiRouteFromLayer(patchLayer);
    if (!route) {
        print("Error: addLayerScript: null Jack MIDI route");
        return;
    }

    // Cache script in GUI (caller) thread for quick access
    layerScriptMap_guiThread.insert(patchLayer, patchLayer->script());

    runInThread(this, [=]()
    {
        ScriptEnvPtr s = layerEnvMap.value(patchLayer);
        if (!s) {
            s.reset(new ScriptEnv());
            s->patchLayer = patchLayer;
            s->route = route;

            connect(&(s->env), &KonfytJSEnv::errorStatusChanged,
                    this, [=](QString errorString)
            {
                emit layerScriptErrorStatusChanged(patchLayer, errorString);
            });

            routeEnvMap.insert(route, s);
            layerEnvMap.insert(patchLayer, s);
        }

        beforeScriptRun(s);
        s->env.setScriptAndInitIfEnabled(script, isScriptEnabled);
        afterScriptRun(s);
    });
}

void KonfytJSEngine::addOrUpdateJackPortScript(PrjMidiPortPtr prjPort)
{
    if (!prjPort) {
        print("Error: addJackPortScript: null port");
        return;
    }

    // Extract required info as we don't want to access prjPort members in
    // the other thread to prevent race conditions
    QString script = prjPort->script;
    bool isScriptEnabled = prjPort->scriptEnabled;
    KfJackMidiPort* jackPort = prjPort->jackPort;

    // Cache script in GUI (caller) thread for quick access
    prjPortScriptMap_guiThread.insert(prjPort, prjPort->script);

    runInThread(this, [=]()
    {
        ScriptEnvPtr s = prjPortEnvMap.value(prjPort);
        if (!s) {
            s.reset(new ScriptEnv());
            s->prjPort = prjPort;

            connect(&(s->env), &KonfytJSEnv::errorStatusChanged,
                    this, [=](QString errorString)
            {
                emit portScriptErrorStatusChanged(prjPort, errorString);
            });

            jackPortEnvMap.insert(jackPort, s);
            prjPortEnvMap.insert(prjPort, s);
        }

        beforeScriptRun(s);
        s->env.setScriptAndInitIfEnabled(script, isScriptEnabled);
        afterScriptRun(s);
    });
}

void KonfytJSEngine::removeLayerScript(KfPatchLayerSharedPtr patchLayer)
{
    layerScriptMap_guiThread.remove(patchLayer);

    runInThread(this, [=]()
    {
        ScriptEnvPtr s = layerEnvMap.value(patchLayer);
        if (!s) {
            print("Error: removeLayerScript: invalid patch layer");
            return;
        }

        routeEnvMap.remove(routeEnvMap.key(s));
        layerEnvMap.remove(patchLayer);
    });
}

void KonfytJSEngine::removeJackPortScript(PrjMidiPortPtr prjPort)
{
    prjPortScriptMap_guiThread.remove(prjPort);

    runInThread(this, [=]()
    {
        ScriptEnvPtr s = prjPortEnvMap.value(prjPort);
        if (!s) {
            print("Error: removeJackPortScript: invalid project port");
            return;
        }

        jackPortEnvMap.remove(jackPortEnvMap.key(s));
        prjPortEnvMap.remove(prjPort);
    });
}

QString KonfytJSEngine::script(KfPatchLayerSharedPtr patchLayer)
{
    // Get from cached values in GUI script map, no need to cross thread boundary
    QString ret;
    if (!layerScriptMap_guiThread.contains(patchLayer)) {
        print("Error: script: invalid patch layer");
    } else {
        ret = layerScriptMap_guiThread.value(patchLayer);
    }
    return ret;
}

QString KonfytJSEngine::script(PrjMidiPortPtr prjPort)
{
    // Get from cached values in GUI script map, no need to cross thread boundary
    QString ret;
    if (!prjPortScriptMap_guiThread.contains(prjPort)) {
        print("Error: script: invalid project port");
    } else {
        ret = prjPortScriptMap_guiThread.value(prjPort);
    }
    return ret;
}

void KonfytJSEngine::setScriptEnabled(KfPatchLayerSharedPtr patchLayer,
                                      bool enable)
{
    runInThread(this, [=]()
    {
        ScriptEnvPtr s = layerEnvMap.value(patchLayer);
        if (!s) {
            print("Error: setScriptEnabled: invalid patch layer");
            return;
        }

        beforeScriptRun(s);
        s->env.setEnabledAndInitIfNeeded(enable);
        afterScriptRun(s);
    });
}

void KonfytJSEngine::setScriptEnabled(PrjMidiPortPtr prjPort, bool enable)
{
    runInThread(this, [=]()
    {
        ScriptEnvPtr s = prjPortEnvMap.value(prjPort);
        if (!s) {
            print("Error: setScriptEnabled: invalid project port");
            return;
        }

        beforeScriptRun(s);
        s->env.setEnabledAndInitIfNeeded(enable);
        afterScriptRun(s);
    });
}

void KonfytJSEngine::scriptAverageProcessTimeMs(KfPatchLayerSharedPtr patchLayer,
                QObject* context, std::function<void(float, float)> callback)
{
    runInThread(this, [=]()
    {
        ScriptEnvPtr s = layerEnvMap.value(patchLayer);
        float perEventTime = 0;
        float totalProcessTime = 0;
        if (s) {
            perEventTime = s->env.getAveragePerEventProcessTimeMs();
            totalProcessTime = s->env.getAverageTotalProcessTimeMs();
        } else {
            print("Error: scriptAverageProcessTimeMs: invalid patch layer");
        }

        // Call callback with result in original caller's thread
        runInThread(context, [=]() { callback(perEventTime, totalProcessTime); });
    });
}

void KonfytJSEngine::scriptAverageProcessTimeMs(PrjMidiPortPtr prjPort,
                QObject* context, std::function<void(float, float)> callback)
{
    runInThread(this, [=]()
    {
        ScriptEnvPtr s = prjPortEnvMap.value(prjPort);
        float perEventTime = 0;
        float totalProcessTime = 0;
        if (s) {
            perEventTime = s->env.getAveragePerEventProcessTimeMs();
            totalProcessTime = s->env.getAverageTotalProcessTimeMs();
        } else {
            print("Error: scriptAverageProcessTimeMs: invalid project port");
        }

        // Call callback with result in original caller's thread
        runInThread(context, [=]() { callback(perEventTime, totalProcessTime); });
    });
}

void KonfytJSEngine::scriptErrorString(KfPatchLayerSharedPtr patchLayer,
                                       QObject* context,
                                       std::function<void(QString)> callback)
{
    runInThread(this, [=]()
    {
        ScriptEnvPtr s = layerEnvMap.value(patchLayer);
        QString ret;
        if (s) {
            ret = s->env.errorString();
        } else {
            print("Error: scriptErrorString: invalid patch layer");
            ret = "KonfytJSEngine error: invalid patch layer";
        }

        // Call callback with result in original caller's thread
        runInThread(context, [=]() { callback(ret); });
    });
}

void KonfytJSEngine::scriptErrorString(PrjMidiPortPtr prjPort,
                                          QObject* context,
                                          std::function<void(QString)> callback)
{
    runInThread(this, [=]()
    {
        ScriptEnvPtr s = prjPortEnvMap.value(prjPort);
        QString ret;
        if (s) {
            ret = s->env.errorString();
        } else {
            print("Error: scriptErrorString: invalid project port");
            ret = "KonfytJSEngine error: invalid project port";
        }

        // Call callback with result in original caller's thread
        runInThread(context, [=]() { callback(ret); });
    });
}

void KonfytJSEngine::runInThread(QObject* context, std::function<void()> func)
{
    QMetaObject::invokeMethod(context, func, Qt::QueuedConnection);
}

void KonfytJSEngine::beforeScriptRun(ScriptEnvPtr s)
{
    watchdog = watchdogMax;
    runningScript = s;
}

void KonfytJSEngine::afterScriptRun(ScriptEnvPtr s)
{
    runningScript.reset();
    watchdog = watchdogMax;

    // Send all queued MIDI events
    if (s->prjPort) {
        jack->sendMidiEventsOnPort(s->prjPort->jackPort, s->env.takeMidiToSend());
    } else if (s->route) {
        jack->sendMidiEventsOnRoute(s->route, s->env.takeMidiToSend());
    }

    // Print all queued messages
    foreach (QString msg, s->env.takePrints()) {
        if (s->patchLayer) {
            emit layerScriptPrint(s->patchLayer, msg);
        } else {
            emit portScriptPrint(s->prjPort, msg);
        }
    }
}

KfJackMidiRoute *KonfytJSEngine::jackMidiRouteFromLayer(KfPatchLayerSharedPtr layer)
{
    KfJackMidiRoute* ret = nullptr;
    if (layer->layerType() == KonfytPatchLayer::TypeMidiOut) {
        ret = layer->midiOutputPortData.jackRoute;
    } else if (layer->layerType() == KonfytPatchLayer::TypeSfz) {
        ret = jack->getPluginMidiRoute(layer->sfzData.portsInJackEngine);
    } else if (layer->layerType() == KonfytPatchLayer::TypeSoundfontProgram) {
        ret = jack->getPluginMidiRoute(layer->soundfontData.portsInJackEngine);
    }
    return ret;
}

void KonfytJSEngine::onNewMidiEventsAvailable()
{
    // Read MIDI rx events from inter-thread buffer, and distribute to script
    // environments based on the route or port
    int n = mRxBuffer->availableToRead();
    for (int i=0; i < n; i++) {
        KfJackMidiRxEvent rxev = mRxBuffer->read();
        ScriptEnvPtr s;
        if (rxev.midiRoute) {
            s = routeEnvMap.value(rxev.midiRoute);
        } else {
            s = jackPortEnvMap.value(rxev.sourcePort);
        }
        if (!s) { continue; }
        if (!s->env.isEnabled()) { continue; }
        s->env.addEvent(rxev.midiEvent);
    }

    // Run all script environment process functions if enabled and they have
    // MIDI events to process.
    foreach (ScriptEnvPtr s, routeEnvMap.values()) {
        if (!s->env.isEnabled()) { continue; }
        if (s->env.eventCount() == 0) { continue; }
        beforeScriptRun(s);
        s->env.runProcess();
        afterScriptRun(s);
    }
    foreach (ScriptEnvPtr s, jackPortEnvMap.values()) {
        if (!s->env.isEnabled()) { continue; }
        if (s->env.eventCount() == 0) { continue; }
        beforeScriptRun(s);
        s->env.runProcess();
        afterScriptRun(s);
    }
}
