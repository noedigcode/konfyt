/******************************************************************************
 *
 * Copyright 2023 Gideon van der Kolf
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

#include <QElapsedTimer>

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

    QElapsedTimer t;
    t.start();

    foreach (const KonfytMidiEvent& ev, midiEvents) {
        QJSValue j = midi.midiEventToJSObject(ev);
        if (!handleJsResult( jsMidiEventFunction.call({j}) )) {
            break;
        }
    }
    midiEvents.clear();

    addProcessTime(t.nsecsElapsed());
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

float KonfytJSEnv::getAverageProcessTimeMs()
{
    if (processTimesCount) {
        return sumProcessTimesNs / processTimesCount / 1000000.0;
    } else {
        return 0;
    }
}

void KonfytJSEnv::sendMidi(QJSValue j)
{
    KonfytMidiEvent ev = midi.jsObjectToMidiEvent(j);
    emit sendMidiEvent(ev);
}

void KonfytJSEnv::addProcessTime(qint64 timeNs)
{
    qint64 sum = sumProcessTimesNs;
    sum -= processTimesNs[iProcessTimes];
    processTimesNs[iProcessTimes] = timeNs;
    sum += timeNs;
    iProcessTimes = (iProcessTimes + 1) % processTimesSize;
    if (processTimesCount < processTimesSize) { processTimesCount++; }
    sumProcessTimesNs = sum;
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
        print("*****************************");
        if (js->isInterrupted()) {
            print("Script took too long to execute and has been stopped.");
        } else {
            print(QString("Evaluate error at line: %1").arg(
                      result.property("lineNumber").toInt()));
            print(result.toString());
        }
        print("*****************************");
        setEnabledAndInitIfNeeded(false);
        emit exceptionOccurred();
        return false;
    } else {
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

void KonfytJSEngine::addLayerScript(KfPatchLayerSharedPtr patchLayer)
{
    runInThisThread([=]()
    {
        if (!patchLayer) {
            print("Error: addLayerScript: null patchLayer");
            return;
        }
        KfJackMidiRoute* route = jackMidiRouteFromLayer(patchLayer);
        if (!route) {
            print("Error: addLayerScript: null Jack MIDI route");
            return;
        }

        ScriptEnvPtr s = layerEnvMap.value(patchLayer);
        if (!s) {
            s.reset(new ScriptEnv());
            connect(&(s->env), &KonfytJSEnv::sendMidiEvent, this, [=](KonfytMidiEvent ev)
            {
                jack->sendMidiEventsOnRoute(route, {ev});
            });
            connect(&(s->env), &KonfytJSEnv::print, this, [=](QString msg)
            {
                emit print(QString("script: [%1] %2").arg(s->env.uri, msg));
            });
            s->env.uri = patchLayer->uri;
        }

        beforeScriptRun(s);
        s->env.setScriptAndInitIfEnabled(patchLayer->script(),
                                         patchLayer->isScriptEnabled());
        afterScriptRun(s);

        routeEnvMap.insert(route, s);
        layerEnvMap.insert(patchLayer, s);
    });
}

void KonfytJSEngine::removeLayerScript(KfPatchLayerSharedPtr patchLayer)
{
    // Capture weak pointer so lambda doesn't hog shared pointer
    KfPatchLayerWeakPtr wp(patchLayer);

    runInThisThread([=]()
    {
        KfPatchLayerSharedPtr layer(wp);
        ScriptEnvPtr s = layerEnvMap.value(layer);
        if (!s) {
            print("Error: removeLayerScript: invalid patch layer");
            return;
        }

        routeEnvMap.remove(routeEnvMap.key(s));
        layerEnvMap.remove(layer);

    });
}

QString KonfytJSEngine::script(KfPatchLayerSharedPtr patchLayer)
{
    QString ret;

    ScriptEnvPtr s = layerEnvMap.value(patchLayer);
    if (!s) {
        print("Error: script: invalid patch layer");
    } else {
        ret = s->env.script();
    }

    return ret;
}

void KonfytJSEngine::setScriptEnabled(KfPatchLayerSharedPtr patchLayer, bool enable)
{
    runInThisThread([=]()
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

float KonfytJSEngine::scriptAverageProcessTimeMs(KfPatchLayerSharedPtr patchLayer)
{
    ScriptEnvPtr s = layerEnvMap.value(patchLayer);
    if (!s) {
        print("Error: scriptAverageProcessTimeMs: invalid patch layer");
        return 0;
    }

    return s->env.getAverageProcessTimeMs();
}

void KonfytJSEngine::updatePatchLayerURIs()
{
    runInThisThread([=]()
    {
        foreach (KfPatchLayerSharedPtr layer, layerEnvMap.keys()) {
            ScriptEnvPtr s = layerEnvMap[layer];
            s->env.uri = layer->uri;
        }
    });
}

void KonfytJSEngine::runInThisThread(std::function<void ()> func)
{
    QMetaObject::invokeMethod(this, func, Qt::QueuedConnection);
}

void KonfytJSEngine::beforeScriptRun(ScriptEnvPtr s)
{
    watchdog = watchdogMax;
    runningScript = s;
}

void KonfytJSEngine::afterScriptRun(ScriptEnvPtr /*s*/)
{
    runningScript.reset();
    watchdog = watchdogMax;
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
    // environments based on the route.
    int n = mRxBuffer->availableToRead();
    for (int i=0; i < n; i++) {
        KfJackMidiRxEvent rxev = mRxBuffer->read();
        ScriptEnvPtr s = routeEnvMap.value(rxev.midiRoute);
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
}
