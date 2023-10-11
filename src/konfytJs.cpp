#include "konfytJs.h"

KonfytJSEnv::KonfytJSEnv(QObject *parent)
    : QObject{parent}
{

}

void KonfytJSEnv::initScript(QString script)
{
    // Use invoke method to ensure code runs in KonfytJS thread.
    // QJSEngine must be created in KonfytJS thread.
    QMetaObject::invokeMethod(this, [=]()
    {
        scriptInitialisationDone = false;
        resetEnvironment();
        mScript = script;

        if (mEnabled) {
            runScriptInitialisation();
            scriptInitialisationDone = true;
        }

    }, Qt::QueuedConnection);
}

void KonfytJSEnv::setEnabled(bool enable)
{
    // Use invoke method to ensure code runs in KonfytJS thread.
    // QJSEngine must be created in KonfytJS thread.
    QMetaObject::invokeMethod(this, [=]()
    {
        if (enable) {
            if (js && !scriptInitialisationDone) {
                runScriptInitialisation();
                scriptInitialisationDone = true;
            }
        }

        // Enabled state will be taken into account in in onNewMidiEventsAvailable()
        mEnabled = enable;

    }, Qt::QueuedConnection);
}

bool KonfytJSEnv::isEnabled()
{
    return mEnabled;
}

void KonfytJSEnv::runProcess()
{
    if (!mEnabled) { return; }

    // Prepare array of events
    jsMidiRxArray = js->newArray(midiEvents.count());
    int i = 0;
    foreach (const KonfytMidiEvent& ev, midiEvents) {
        QJSValue j = midiEventToJSObject(ev);
        jsMidiRxArray.setProperty(i, j);
        i++;
    }
    midiEvents.clear();

    mThisClass.setProperty("events", jsMidiRxArray);

    runScriptProcessFunction();
}

void KonfytJSEnv::addEvent(KonfytMidiEvent ev)
{
    midiEvents.append(ev);
}

int KonfytJSEnv::eventCount()
{
    return midiEvents.count();
}

void KonfytJSEnv::send(QJSValue j)
{
    KonfytMidiEvent ev = jsObjectToMidiEvent(j);
    emit sendMidiEvent(ev);
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
    mThisClass = js->newQObject(this);
    js->globalObject().setProperty("Sys", mThisClass);

    // Add MIDI event type constants as global objects
    js->globalObject().setProperty("NOTEON", TYPE_NOTEON);
    js->globalObject().setProperty("NOTEOFF", TYPE_NOTEOFF);
    js->globalObject().setProperty("POLYAFTERTOUCH", TYPE_POLY_AFTERTOUCH);
    js->globalObject().setProperty("CC", TYPE_CC);
    js->globalObject().setProperty("PROGRAM", TYPE_PROGRAM);
    js->globalObject().setProperty("AFTERTOUCH", TYPE_AFTERTOUCH);
    js->globalObject().setProperty("PITCHBEND", TYPE_PITCHBEND);
    js->globalObject().setProperty("SYSEX", TYPE_SYSEX);
}

void KonfytJSEnv::evaluate(QString script)
{
    QJSValue result = js->evaluate(script);
    if (result.isError()) {
        print("-----------------------------");
        print(QString("Evaluate error at line: %1").arg(
                  result.property("lineNumber").toInt()));
        print(result.toString());
        print("-----------------------------");
        setEnabled(false);
        emit exceptionOccurred();
    }
}

void KonfytJSEnv::runScriptInitialisation()
{
    evaluate(mScript);
    evaluate("init()");
}

void KonfytJSEnv::runScriptProcessFunction()
{
    evaluate("process()");
}

QJSValue KonfytJSEnv::midiEventToJSObject(const KonfytMidiEvent &ev)
{
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
    j.setProperty("channel", ev.channel);

    return j;
}

KonfytMidiEvent KonfytJSEnv::jsObjectToMidiEvent(QJSValue j)
{
    KonfytMidiEvent ev;

    ev.channel = value(j, "channel", 0).toInt();
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

QVariant KonfytJSEnv::value(const QJSValue &j, QString key, QVariant defaultValue)
{
    QJSValue jv = j.property(key);
    if (jv.isUndefined()) { return defaultValue; }
    else { return jv.toVariant(); }
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
    if (!patchLayer) {
        print("Error: addLayerScript: null patchLayer");
        return;
    }
    KfJackMidiRoute* route = jackMidiRouteFromLayer(patchLayer);
    if (!route) {
        print("Error: addLayerScript: null Jack MIDI route");
        return;
    }

    QMetaObject::invokeMethod(this, [=]()
    {
        ScriptEnvPtr s(new ScriptEnv());
        connect(&(s->env), &KonfytJSEnv::sendMidiEvent, this, [=](KonfytMidiEvent ev)
        {
            jack->sendMidiEventsOnRoute(route, {ev});
        });
        connect(&(s->env), &KonfytJSEnv::print, this, [=](QString msg)
        {
            emit print(QString("script: %1").arg(msg));
        });

        s->env.setEnabled(patchLayer->isScriptEnabled());
        s->env.initScript(patchLayer->script());

        routeEnvMap.insert(route, s);
        layerEnvMap.insert(patchLayer, s);
    }, Qt::QueuedConnection);
}

void KonfytJSEngine::setScriptEnabled(KfPatchLayerSharedPtr patchLayer, bool enable)
{
    ScriptEnvPtr s = layerEnvMap.value(patchLayer);
    if (!s) {
        print("Error: setScriptEnabled: null patch layer");
        return;
    }

    s->env.setEnabled(enable);
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
        s->env.runProcess();
    }
}
