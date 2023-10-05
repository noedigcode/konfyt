#ifndef KONFYTJS_H
#define KONFYTJS_H

#include "sleepyRingBuffer.h"
#include "konfytJackEngine.h"

#include <QJSEngine>
#include <QJsonObject>
#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>

/* TODO NOTES:
 *
 * Needs "QT += qml" in project file
 * Needs QML/Qt Declarative development packages, "qtdeclarative5-dev" on Ubuntu.
 *
 *
 * QJSEngine must be created in the same thread it is used in. Otherwise crashes
 * happen as the engine's internal cleanup functionality running in the thread
 * that created the engine conflicts with the use of the engine in the other
 * thread.
 *
 * Related links:
 *
 * https://bugreports.qt.io/browse/QTBUG-83410
 *
 * https://stackoverflow.com/questions/74120887/qjsengine-crashes-when-used-in-multithreading-program
 *
 * https://github.com/nst1911/thread-safe-qjsengine
 *
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
    ~TempParent();
    void makeTemporaryChildIfParentless(QObject* obj);
private:
    QList<QObject*> tempChildren;
    void removeTemporaryChildren();
};

// =============================================================================

class KonfytJs : public QObject
{
    Q_OBJECT
public:
    explicit KonfytJs(QObject *parent = nullptr);

    void setJackEngine(KonfytJackEngine* jackEngine);

    void resetEnvironment();
    void initScript(QString script);
    void enableScript();
    void disableScript();
    void evaluate(QString script);

signals:
    void print(QString msg);
    void exceptionOccurred();

public slots:
    void onNewMidiEventsAvailable();

    // For use from script
    void sendEvent(QJSValue j);

private:
    TempParent tempParent;
    KonfytJackEngine* jack;

    QScopedPointer<QJSEngine> js;
    QJSValue mThisClass; // "Sys" object in script environment
    bool mEnabled = false;

    QSharedPointer<SleepyRingBuffer<KfJackMidiRxEvent>> mRxBuffer;
    KfJackMidiRoute* mLastRoute = nullptr;

    QString mScript;
    void runScriptProcessFunction();
    QJSValue jsMidiRxArray;

    QJSValue midiEventToJSObject(KonfytMidiEvent& ev);
    KonfytMidiEvent jsObjectToMidiEvent(QJSValue j);

    QVariant value(const QJSValue& j, QString key, QVariant defaultValue);
};

#endif // KONFYTJS_H
