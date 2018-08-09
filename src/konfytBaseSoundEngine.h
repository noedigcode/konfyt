#ifndef KONFYTBASESOUNDENGINE_H
#define KONFYTBASESOUNDENGINE_H

#include <QObject>

#include "konfytJackEngine.h"

class KonfytBaseSoundEngine : public QObject
{
    Q_OBJECT
public:
    explicit KonfytBaseSoundEngine(QObject *parent = 0);

    virtual void initEngine(KonfytJackEngine* jackEngine) = 0;
    virtual int addSfz(QString path) = 0;
    virtual QString pluginName(int id) = 0;
    virtual QString midiInJackPortName(int id) = 0;
    virtual QStringList audioOutJackPortNames(int id) = 0;
    virtual void removeSfz(int id) = 0;
    virtual void setGain(int id, float newGain) = 0;

signals:
    void userMessage(QString msg);
    void statusInfo(QString msg);

public slots:
};

#endif // KONFYTBASESOUNDENGINE_H
