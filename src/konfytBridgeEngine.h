#ifndef KONFYTBRIDGEENGINE_H
#define KONFYTBRIDGEENGINE_H

#include <QObject>
#include <QProcess>

#include "konfytBaseSoundEngine.h"

class KonfytBridgeEngine : public KonfytBaseSoundEngine
{
    Q_OBJECT
public:

    struct KonfytBridgeItem
    {
        QString state;
        int startCount;

        QProcess* process;
        QString soundfilePath;
        QString jackname;
        QString cmd;

        KonfytBridgeItem() : startCount(0) {}
    };

    explicit KonfytBridgeEngine(QObject *parent = 0);

    void setKonfytExePath(QString path);
    void initEngine(KonfytJackEngine* jackEngine);
    int addSfz(QString soundfilePath)
    {
        KonfytBridgeItem item;

        item.soundfilePath = soundfilePath;

        int id = idCounter++;
        item.jackname = jack->clientName() + "_subclient_" + n2s(id);

        item.cmd = exePath + " -j " + item.jackname + " \"" + soundfilePath + "\"";

        item.process = new QProcess();
        connect(item.process, &QProcess::started, [this, id](){
            KonfytBridgeItem &item = items[id];
            item.state = "Started";
            sendAllStatusInfo();
            userMessage("Bridge client " + n2s(id) + " started.");
        });
        connect(item.process, static_cast<void (QProcess::*)(int)>(&QProcess::finished),
                [this, id](int returnCode){
            // Process has finished. Restart.
            userMessage("Bridge client " + n2s(id) + " stopped: " + n2s(returnCode) + ". Restarting...");
            startProcess(id);
        });

        items.insert(id, item);
        startProcess(id);

        return id;
    }
    QString pluginName(int id)
    {
        if (!items.contains(id)) { return "Invalid id " + n2s(id); }
        KonfytBridgeItem &item = items[id];
        return item.soundfilePath;
    }
    QString midiInJackPortName(int id)
    {
        if (!items.contains(id)) {
            // TODO BRIDGE error_abort()
            return "Invalid id " + n2s(id);
        }
        KonfytBridgeItem &item = items[id];
        return item.jackname + ":" + "midi_in_0"; // TODO BRIDGE Detect ports with less fragility
    }

    QStringList audioOutJackPortNames(int id)
    {
        QStringList ret;

        if (!items.contains(id)) {
            // TODO BRIDGE error_abort()
            return ret;
        }
        KonfytBridgeItem &item = items[id];
        ret.append( item.jackname + ":" + "bus_0_L" ); // TODO BRIDGE Detect ports with less fragility
        ret.append( item.jackname + ":" + "bus_0_R" ); // TODO BRIDGE Detect ports with less fragility

        return ret;
    }

    void removeSfz(int id)
    {
        // TODO BRIDGE
        userMessage("TODO BRIDGE: removeSfz " + n2s(id));
    }

    void setGain(int id, float newGain)
    {
        // TODO BRIDGE
        userMessage("TODO BRIDGE: setGain of " + n2s(id) + " to " + n2s(newGain));
    }

    QString getAllStatusInfo()
    {
        QString ret;
        QList<int> ids = items.keys();
        for (int i=0; i < ids.count(); i++) {
            ret += getStatusInfo(ids[i]);
        }
        return ret;
    }

private:
    KonfytJackEngine* jack;
    QString exePath;
    int idCounter;
    QMap<int, KonfytBridgeItem> items;

    void startProcess(int id)
    {
        KonfytBridgeItem &item = items[id];

        userMessage("Bridge client " + n2s(id) + " starting:");
        userMessage(   item.cmd);

        item.startCount++;
        item.state = "Starting...";
        sendAllStatusInfo();

        item.process->start(item.cmd);
    }

    QString getStatusInfo(int id)
    {
        if (!items.contains(id)) { return "Invalid id " + n2s(id); }
        KonfytBridgeItem &item = items[id];

        QString ret;
        ret += "Subclient " + n2s(id) + "\n";
        ret += "   State: " + item.state + "\n";
        ret += "   StartCount: " + n2s(item.startCount) + "\n";
        ret += "   Soundfile: " + item.soundfilePath + "\n";

        return ret;
    }

    void sendAllStatusInfo()
    {
        emit statusInfo( getAllStatusInfo() );
    }
};

#endif // KONFYTBRIDGEENGINE_H


