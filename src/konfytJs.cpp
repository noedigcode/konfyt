#include "konfytJs.h"

KonfytJs::KonfytJs(QObject *parent)
    : QObject{parent}
{

}

void KonfytJs::setJackEngine(KonfytJackEngine *jackEngine)
{
    jack = jackEngine;
    mRxBuffer = jack->getMidiRxBufferForJs();

    connect(jack, &KonfytJackEngine::newMidiEventsAvailable,
            this, &KonfytJs::onNewMidiEventsAvailable);
}

void KonfytJs::resetEnvironment()
{
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

void KonfytJs::initScript(QString script)
{
    if (!js) { resetEnvironment(); }

    mScript = script;

    evaluate(script);
    evaluate("init()");
}

void KonfytJs::enableScript()
{
    if (!js) { resetEnvironment(); }

    // Clear buffer before starting
    int n = mRxBuffer->availableToRead();
    for (int i=0; i < n; i++) {
        mRxBuffer->read();
    }

    // Set enabled so script will run in onNewMidiEventsAvailable()
    mEnabled = true;
}

void KonfytJs::disableScript()
{
    // Set disabled so script will not run in onNewMidiEventsAvailable()
    mEnabled = false;
}

void KonfytJs::evaluate(QString script)
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

void KonfytJs::onNewMidiEventsAvailable()
{
    if (!mEnabled) { return; }

    int n = mRxBuffer->availableToRead();
    print(QString("DEBUG available to read: %1").arg(n));

    jsMidiRxArray = js->newArray(n);

    for (int i=0; i < n; i++) {
        KfJackMidiRxEvent rxev = mRxBuffer->read();
        mLastRoute = rxev.midiRoute;
        QJSValue j = midiEventToJSObject(rxev.midiEvent);
        jsMidiRxArray.setProperty(i, j);
    }

    mThisClass.setProperty("events", jsMidiRxArray);

    runScriptProcessFunction();
}

void KonfytJs::sendEvent(QJSValue j)
{
    KonfytMidiEvent ev = jsObjectToMidiEvent(j);
    jack->sendMidiEventsOnRoute(mLastRoute, {ev});
}

void KonfytJs::runScriptProcessFunction()
{
    evaluate("process()");
}

QJSValue KonfytJs::midiEventToJSObject(KonfytMidiEvent &ev)
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

KonfytMidiEvent KonfytJs::jsObjectToMidiEvent(QJSValue j)
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

QVariant KonfytJs::value(const QJSValue &j, QString key, QVariant defaultValue)
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
