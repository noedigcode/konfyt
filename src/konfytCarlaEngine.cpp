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

#include "konfytCarlaEngine.h"

#include <iostream>


KonfytCarlaEngine::KonfytCarlaEngine(QObject *parent) :
    KonfytBaseSoundEngine(parent)
{

}

KonfytCarlaEngine::~KonfytCarlaEngine()
{
    CARLA_FUNC_V(carla_engine_close);
}

QString KonfytCarlaEngine::engineName()
{
    return "CarlaEngine";
}

/* Adds a new plugin and returns the unique ID. Returns -1 on error. */
int KonfytCarlaEngine::addSfz(QString path)
{
    KonfytCarlaPluginData pluginData;
    pluginData.ID = pluginUniqueIDCounter;
    int pluginIdInCarla = pluginList.count();
    pluginData.path = path;
    pluginData.name = "plugin_" + n2s(pluginData.ID) + "_sfz";
    // Load the plugin

    bool returnValue;
    print("Loading sfz: " + pluginData.path + ", " + pluginData.name);
    #if CARLA_VERSION_HEX < 0x01095
    // Old carla used PLUGIN_FILE_SFZ below.
    returnValue = carla_add_plugin(BINARY_NATIVE, PLUGIN_FILE_SFZ,pluginData.path.toLocal8Bit(),
                          pluginData.name.toLocal8Bit(),"sfz",0,NULL);
    #elif CARLA_VERSION_HEX == 0x01095
    // Carla 1.9.5 (2.0-beta3) changed to PLUGIN_SFZ.
    returnValue = carla_add_plugin(BINARY_NATIVE, PLUGIN_SFZ,pluginData.path.toLocal8Bit(),
                          pluginData.name.toLocal8Bit(),"sfz",0,NULL);
    #else
    // 2015-03-28 Updated to Carla 1.9.6 (2.0-beta4). carla_add_plugin now has an additional parameter.
    // 2017-01-06 Tested with Carla 1.9.7 (2.0-beta5) (0x01097) as well.
    returnValue = CARLA_FUNC(carla_add_plugin, BINARY_NATIVE, PLUGIN_SFZ,
                             pluginData.path.toLocal8Bit(),
                             pluginData.name.toLocal8Bit(), "sfz", 0, NULL, 0);
    #endif

    if ( !returnValue ) {
        // Not a success.
        print("Carla failed to load plugin.");
        setErrorString("Carla plugin load error");
        return -1;
    }

    CARLA_FUNC(carla_set_active, pluginIdInCarla, true);
    CARLA_FUNC(carla_set_option, pluginIdInCarla,
               PLUGIN_OPTION_SEND_CONTROL_CHANGES, true);
    CARLA_FUNC(carla_set_option, pluginIdInCarla,
               PLUGIN_OPTION_SEND_PITCHBEND, true);

    // Insert to map of unique IDs (in this class) and plugin data
    pluginDataMap.insert( pluginData.ID, pluginData );
    // Add to list with indexes corresponding to pluginIds in Carla engine
    pluginList.append(pluginData.ID);

    pluginUniqueIDCounter++;

    setErrorString("");
    // Return the unique ID used by this class to distinguish the plugin
    return pluginData.ID;
}

void KonfytCarlaEngine::removeSfz(int ID)
{
    KONFYT_ASSERT( pluginDataMap.contains(ID) );
    KONFYT_ASSERT( pluginList.contains(ID) );

    pluginDataMap.remove(ID);
    int pluginIdInCarla = pluginList.indexOf(ID);
    pluginList.removeAt(pluginIdInCarla);

    bool ret = CARLA_FUNC(carla_remove_plugin, pluginIdInCarla);
    if (!ret) {
        print(QString("ERROR - Failed to remove plugin from Carla. "
                      "ID: %1, pluginIdInCarla: %2")
              .arg(ID).arg(pluginIdInCarla));
    }
}

QString KonfytCarlaEngine::pluginName(int ID)
{
    KONFYT_ASSERT( pluginDataMap.contains(ID) );

    return pluginDataMap.value(ID).name;
}

void KonfytCarlaEngine::setErrorString(QString s)
{
    if (s != mLastErrorString) {
        mLastErrorString = s;
        emit errorStringChanged(mLastErrorString);
    }
}

QString KonfytCarlaEngine::midiInJackPortName(int ID)
{
    KONFYT_ASSERT( pluginDataMap.contains(ID) );

    // TODO: This depends on Carla naming the ports as we expect. A better way
    // would be to get the port names from a Carla callback.

    return QString("%1:%2:%3").arg(mJackClientName, pluginName(ID),
                                   CARLA_MIDI_IN_PORT_POSTFIX);
}

QStringList KonfytCarlaEngine::audioOutJackPortNames(int ID)
{
    KONFYT_ASSERT( pluginDataMap.contains(ID) );

    // TODO: This depends on Carla naming the ports as we expect. A better way
    // would be to get the port names from a Carla callback.

    QStringList ret;

    ret.append(QString("%1:%2:%3").arg(mJackClientName, pluginName(ID),
                                       CARLA_OUT_LEFT_PORT_POSTFIX));
    ret.append(QString("%1:%2:%3").arg(mJackClientName, pluginName(ID),
                                       CARLA_OUT_RIGHT_PORT_POSTFIX));

    return ret;
}

void KonfytCarlaEngine::initEngine(KonfytJackEngine* jackEngine)
{
    jack = jackEngine;
    mJackClientName = jack->clientName() + CARLA_CLIENT_POSTFIX;

    print("Carla version " + QString(CARLA_VERSION_STRING));

    // Initialise Carla Backend
#ifdef CARLA_USE_HANDLE
    carlaHandle = carla_standalone_host_init();
#endif
    CARLA_FUNC(carla_set_engine_option, ENGINE_OPTION_PROCESS_MODE,
               ENGINE_PROCESS_MODE_SINGLE_CLIENT, NULL);
    // Set path to the resource files.
    // Default unset.
    // Must be set for some internal plugins to work
    CARLA_FUNC(carla_set_engine_option, ENGINE_OPTION_PATH_RESOURCES, 0,
               "/usr/lib/lv2/carla.lv2/resources/");
    // Set the engine callback
    //carla_set_engine_callback(KonfytCarlaEngine::carlaEngineCallback, this);
    // TODO: Handle the case where this name is already taken.
    bool ok = CARLA_FUNC(carla_engine_init, "JACK",
                         mJackClientName.toLocal8Bit().constData());

    QString error;
    if (!ok) {
        error = CARLA_FUNC_V(carla_get_last_error);
        print("Carla engine initialisation error:");
        print(error);
        setErrorString("Carla init error");
    } else {
        setErrorString("");
    }
    emit initDone(error);
}

QString KonfytCarlaEngine::jackClientName()
{
    return mJackClientName;
}

void KonfytCarlaEngine::setGain(int ID, float newGain)
{
    KONFYT_ASSERT( pluginDataMap.contains(ID) );
    KONFYT_ASSERT( pluginList.contains(ID) );

    int pluginIdInCarla = pluginList.indexOf(ID);

    CARLA_FUNC(carla_set_volume, pluginIdInCarla, newGain);
}

