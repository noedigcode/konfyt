#ifndef KONFYTLSCPENGINE_H
#define KONFYTLSCPENGINE_H

#include <QObject>

#include "konfytBaseSoundEngine.h"

#include "gidls.h"

class KonfytLscpEngine : public KonfytBaseSoundEngine
{
public:
    KonfytLscpEngine(QObject *parent = 0);
    ~KonfytLscpEngine();

    void initEngine(KonfytJackEngine *jackEngine);
    QString jackClientName();
    int addSfz(QString path);
    QString pluginName(int id);
    QString midiInJackPortName(int id);
    QStringList audioOutJackPortNames(int id);
    void removeSfz(int id);
    void setGain(int id, float newGain);

private:
    GidLs ls;

};

#endif // KONFYTLSCPENGINE_H
