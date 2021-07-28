/******************************************************************************
 *
 * Copyright 2021 Gideon van der Kolf
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

#include "konfytBaseSoundEngine.h"
#include "konfytDefines.h"

#include <carla/CarlaHost.h>

#include <QObject>
#include <QTime>

CARLA_BACKEND_USE_NAMESPACE

#if CARLA_VERSION_HEX >= 0x020200
#define CARLA_USE_HANDLE
#define CARLA_FUNC(x, ...) x(carlaHandle, __VA_ARGS__)
#define CARLA_FUNC_V(x) x(carlaHandle)
#else
#define CARLA_FUNC(x, ...) x(__VA_ARGS__)
#define CARLA_FUNC_V(x) x()
#endif

class KonfytCarlaEngine : public KonfytBaseSoundEngine
{
    Q_OBJECT
public:

    struct KonfytCarlaPluginData
    {
        int ID; // ID in konfytCarlaEngine (this class)
        QString path;
        QString name;
    };

    explicit KonfytCarlaEngine(QObject *parent = 0);
    ~KonfytCarlaEngine();

    // KonfytBaseSoundEngine interface
    QString engineName();
    void initEngine(KonfytJackEngine *jackEngine);
    QString jackClientName();
    int addSfz(QString path);
    QString midiInJackPortName(int id);
    QStringList audioOutJackPortNames(int id);
    void removeSfz(int id);
    void setGain(int ID, float newGain);
    QString pluginName(int ID);

private:
#ifdef CARLA_USE_HANDLE
    CarlaHostHandle carlaHandle;
#endif
    int pluginUniqueIDCounter = 10;
    QString mJackClientName;
    KonfytJackEngine* jack = nullptr;
    QMap<int, KonfytCarlaPluginData> pluginDataMap;
    QList<int> pluginList; // List with indexes matching id's in Carla engine, i.e. maps this class' unique IDs to pluginIds in Carla engine.

    const QString CARLA_CLIENT_POSTFIX = "_plugins";
    const QString CARLA_MIDI_IN_PORT_POSTFIX = "events-in";
    const QString CARLA_OUT_LEFT_PORT_POSTFIX = "out-left";
    const QString CARLA_OUT_RIGHT_PORT_POSTFIX = "out-right";
};

#endif // KONFYT_CARLA_ENGINE_H
