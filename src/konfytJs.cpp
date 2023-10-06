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
        if (!js) { resetEnvironment(); }

        mScript = script;

        evaluate(script);
        evaluate("init()");
    }, Qt::QueuedConnection);
}

void KonfytJSEnv::enableScript()
{
    // Use invoke method to ensure code runs in KonfytJS thread.
    // QJSEngine must be created in KonfytJS thread.
    QMetaObject::invokeMethod(this, [=]()
    {
        if (!js) { resetEnvironment(); }

        // Set enabled so script will run in onNewMidiEventsAvailable()
        mEnabled = true;
    }, Qt::QueuedConnection);
}

bool KonfytJSEnv::isEnabled()
{
    return mEnabled;
}

void KonfytJSEnv::disableScript()
{
    // Set disabled so script will not run in onNewMidiEventsAvailable()
    mEnabled = false;
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
        disableScript();
        emit exceptionOccurred();
    }
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
        break;
    case MIDI_EVENT_TYPE_NOTEOFF:
        type = "noteoff";
        break;
    case MIDI_EVENT_TYPE_POLY_AFTERTOUCH:
        type = "polyaftertouch";
        break;
    case MIDI_EVENT_TYPE_CC:
        type = "cc";
        break;
    case MIDI_EVENT_TYPE_PROGRAM:
        type = "program";
        break;
    case MIDI_EVENT_TYPE_AFTERTOUCH:
        type = "aftertouch";
        break;
    case MIDI_EVENT_TYPE_PITCHBEND:
        type = "pitchbend";
        break;
    case MIDI_EVENT_TYPE_SYSTEM:
        type = "sysex";
        break;
    }
    j.setProperty("type", type);

    j.setProperty("channel", ev.channel);
    j.setProperty("bankmsb", ev.bankMSB);
    j.setProperty("banklsb", ev.bankLSB);
    j.setProperty("data1", ev.data1());
    j.setProperty("data2", ev.data2());

    return j;
}

KonfytMidiEvent KonfytJSEnv::jsObjectToMidiEvent(QJSValue j)
{
    KonfytMidiEvent ev;

    int channel = value(j, "channel", 0).toInt();
    int bankLSB = value(j, "banklsb", -1).toInt();
    int bankMSB = value(j, "bankmsb", -1).toInt();
    int data1 = value(j, "data1", 0).toInt();
    int data2 = value(j, "data2", 0).toInt();
    QString type = value(j, "type", "noteon").toString();

    ev.channel = channel;
    if (type == "noteon") {
        ev.setNoteOn(data1, data2);
    } else if (type == "noteoff") {
        ev.setNoteOff(data1, data2);
    } else if (type == "cc") {
        ev.setCC(data1, data2);
    } else if (type == "program") {
        ev.setProgram(data1);
        ev.bankLSB = bankLSB;
        ev.bankMSB = bankMSB;
    } else if (type == "pitchbend") {
        ev.setPitchbend(data1);
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

    ScriptEnvPtr s(new ScriptEnv());
    connect(&(s->env), &KonfytJSEnv::sendMidiEvent, this, [=](KonfytMidiEvent ev)
    {
        jack->sendMidiEventsOnRoute(route, {ev});
    });
    connect(&(s->env), &KonfytJSEnv::print, this, [=](QString msg)
    {
        emit print(QString("script: %1").arg(msg));
    });

    s->env.initScript(patchLayer->script());

    routeEnvMap.insert(route, s);
    layerEnvMap.insert(patchLayer, s);

    s->env.enableScript();
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
