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
    ~KonfytBridgeEngine();

    void setKonfytExePath(QString path);
    void initEngine(KonfytJackEngine* jackEngine);
    int addSfz(QString soundfilePath);
    QString pluginName(int id);
    QString midiInJackPortName(int id);
    QStringList audioOutJackPortNames(int id);
    void removeSfz(int id);
    void setGain(int id, float newGain);
    QString getAllStatusInfo();

private:
    KonfytJackEngine* jack;
    QString exePath;
    int idCounter;
    QMap<int, KonfytBridgeItem> items;

    void startProcess(int id);
    QString getStatusInfo(int id);
    void sendAllStatusInfo();
};

#endif // KONFYTBRIDGEENGINE_H


