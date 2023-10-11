#ifndef KONFYTJS_H
#define KONFYTJS_H

#include "sleepyRingBuffer.h"
#include "konfytJackEngine.h"

#include <QJSEngine>
#include <QJsonObject>
#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>


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

/* Javascript environment for a script. */
class KonfytJSEnv : public QObject
{
    Q_OBJECT
public:
    explicit KonfytJSEnv(QObject *parent = nullptr);

    void initScript(QString script);
    void setEnabled(bool enable);
    bool isEnabled();
    void runProcess();

    void addEvent(KonfytMidiEvent ev);
    int eventCount();

signals:
    void print(QString msg);
    void exceptionOccurred();
    void sendMidiEvent(KonfytMidiEvent ev);

public slots:
    // For use from script
    void send(QJSValue j);

private:
    TempParent tempParent; // TODO DEBUG: JS garbage collector doesn't like it if
                           // tempParent's parent is his class, and this class is
                           // tempParent's parent (i.e. circular reference).


    QScopedPointer<QJSEngine> js;
    QJSValue mThisClass; // "Sys" object in script environment
    bool mEnabled = false;

    void resetEnvironment();
    void evaluate(QString script);

    QString mScript;
    bool scriptInitialisationDone = false;
    void runScriptInitialisation();
    void runScriptProcessFunction();

    QList<KonfytMidiEvent> midiEvents;
    QJSValue jsMidiRxArray;

    QJSValue midiEventToJSObject(const KonfytMidiEvent& ev);
    KonfytMidiEvent jsObjectToMidiEvent(QJSValue j);

    QVariant value(const QJSValue& j, QString key, QVariant defaultValue);

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
 * and Jack. */
class KonfytJSEngine : public QObject
{
    Q_OBJECT
public:
    void setJackEngine(KonfytJackEngine* jackEngine);

    void addLayerScript(KfPatchLayerSharedPtr patchLayer);
    void removeLayerScript(KfPatchLayerSharedPtr patchLayer)
    {
        print("TODO removeLayerScript");
    }

    void setScriptEnabled(KfPatchLayerSharedPtr patchLayer, bool enable);

signals:
    void print(QString msg);

private:
    KonfytJackEngine* jack = nullptr;
    QSharedPointer<SleepyRingBuffer<KfJackMidiRxEvent>> mRxBuffer;

    struct ScriptEnv {
        KonfytJSEnv env;
    };
    typedef QSharedPointer<ScriptEnv> ScriptEnvPtr;

    QMap<KfJackMidiRoute*, ScriptEnvPtr> routeEnvMap;
    QMap<KfPatchLayerSharedPtr, ScriptEnvPtr> layerEnvMap;

    KfJackMidiRoute* jackMidiRouteFromLayer(KfPatchLayerSharedPtr layer);

private slots:
    void onNewMidiEventsAvailable();
};

#endif // KONFYTJS_H
