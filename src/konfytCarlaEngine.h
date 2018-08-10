/******************************************************************************
 *
 * Copyright 2017 Gideon van der Kolf
 *
 * This file is part of Konfyt.
 *
 *     Konfyt is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Konfyt is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with Konfyt.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef KONFYT_CARLA_ENGINE_H
#define KONFYT_CARLA_ENGINE_H

#include <QObject>
#include <QTime>

#include <carla/CarlaHost.h>

#include "konfytBaseSoundEngine.h"
#include "konfytDefines.h"

#define CARLA_CLIENT_POSTFIX "_plugins"
#define CARLA_MIDI_IN_PORT_POSTFIX "events-in"
#define CARLA_OUT_LEFT_PORT_POSTFIX "out-left"
#define CARLA_OUT_RIGHT_PORT_POSTFIX "out-right"

CARLA_BACKEND_USE_NAMESPACE

class konfytCarlaEngine : public KonfytBaseSoundEngine
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
    ~konfytCarlaEngine();

    // KonfytBaseSoundEngine interface
    void initEngine(KonfytJackEngine *jackEngine);
    int addSfz(QString path);
    QString midiInJackPortName(int id);
    QStringList audioOutJackPortNames(int id);
    void removeSfz(int id);
    void setGain(int ID, float newGain);
    QString pluginName(int ID);

    static void carlaEngineCallback(void* ptr, EngineCallbackOpcode action,
                                    uint pluginId, int value1, int value2,
                                    float value3, const char* valueStr);

    void error_abort(QString msg);

private:
    int pluginUniqueIDCounter;
    QString jackClientName;
    KonfytJackEngine* jack;
    QMap<int, konfytCarlaPluginData> pluginDataMap;
    QList<int> pluginList; // List with indexes matching id's in Carla engine, i.e. maps this class' unique IDs to pluginIds in Carla engine.
};

#endif // KONFYT_CARLA_ENGINE_H
