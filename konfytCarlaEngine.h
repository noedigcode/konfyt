#ifndef KONFYT_CARLA_ENGINE_H
#define KONFYT_CARLA_ENGINE_H

#include <QObject>
#include <carla/CarlaHost.h>
#include "konfytDefines.h"
#include "konfytDatabase.h"
#include "konfytJackEngine.h"
#include <QTime>

#define CARLA_MIDI_IN_PORT_POSTFIX "events-in"
#define CARLA_OUT_LEFT_PORT_POSTFIX "out-left"
#define CARLA_OUT_RIGHT_PORT_POSTFIX "out-right"

CARLA_BACKEND_USE_NAMESPACE

class konfytCarlaEngine : public QObject
{
    Q_OBJECT
public:

    struct konfytCarlaPluginData
    {
        int ID; // ID in konfytCarlaEngine (this class)
        QString path;
        QString name;
    };

    explicit konfytCarlaEngine(QObject *parent = 0);

    static void carlaEngineCallback(void* ptr, EngineCallbackOpcode action, uint pluginId, int value1, int value2, float value3, const char* valueStr);
    void InitCarlaEngine(konfytJackEngine *jackEngine, QString carlaJackClientName);

    int addSFZ(QString path);
    void removeSFZ(int ID);
    QString pluginName(int ID);
    QString carlaJackClientName();
    QString midiInPort(int ID);
    QStringList audioOutPorts(int ID);

    void setGain(int ID, float newGain);

    void error_abort(QString msg);

private:
    int pluginUniqueIDCounter;
    QString jackClientName;
    konfytJackEngine* jack;
    QMap<int, konfytCarlaPluginData> pluginDataMap;
    QList<int> pluginList; // List with indexes matching id's in Carla engine, i.e. maps this class' unique IDs to pluginIds in Carla engine.
    
signals:
    void userMessage(QString msg);
    
public slots:
    
};

#endif // KONFYT_CARLA_ENGINE_H
