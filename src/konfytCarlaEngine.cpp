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

/************************************************************************
 * OLD CODE FOR LOADING OTHER TYPES OF PLUGINS
 *
            if ( plugin.carlaPluginData.pluginType == KonfytCarlaPluginType_LV2 ) {
                #if CARLA_VERSION_HEX > 0x01095


                //plugin.carlaPluginData.name = "plugin_" + n2s(index) + "_lv2";
                //returnValue = carla_add_plugin( BINARY_NATIVE,  // Binary type
                //                                PLUGIN_LV2,     // Plugin type
                //                                plugin.carlaPluginData.path.toLocal8Bit(),  // Filename
                //                                plugin.carlaPluginData.name.toLocal8Bit(),  // Name of plugin
                //                                "http://kxstudio.sf.net/carla/plugins/zynaddsubfx",  // Plugin label
                //                                0,      // Plugin unique id, if applicable
                //                                NULL,   // Extra pointer (defined per plugin type)
                //                                0       // Initial plugin options
                //                                );

                // BINARY_NATIVE, PLUGIN_LV2, "/usr/lib/lv2/amsynth.lv2/", NULL, "http://code.google.com/p/amsynth/amsynth", 0, NULL
                plugin.carlaPluginData.name = "plugin_" + n2s(index) + "_lv2";
                returnValue = carla_add_plugin( BINARY_NATIVE,  // Binary type
                                                PLUGIN_LV2,     // Plugin type
                                                NULL,  // Filename
                                                plugin.carlaPluginData.name.toLocal8Bit(),  // Name of plugin
                                                plugin.carlaPluginData.path.toLocal8Bit(),  // Plugin label
                                                0,      // Plugin unique id, if applicable
                                                NULL,   // Extra pointer (defined per plugin type)
                                                0       // Initial plugin options
                                                );

                #endif

            } else if (plugin.carlaPluginData.pluginType == KonfytCarlaPluginType_SFZ ) {

                plugin.carlaPluginData.name = "plugin_" + n2s(index) + "_sfz";

                // Old carla used PLUGIN_FILE_SFZ below.
                #if CARLA_VERSION_HEX < 0x01095
                returnValue = carla_add_plugin(BINARY_NATIVE, PLUGIN_FILE_SFZ,plugin.carlaPluginData.path.toLocal8Bit(),
                                      plugin.carlaPluginData.name.toLocal8Bit(),"sfz",0,NULL);
                #elif CARLA_VERSION_HEX == 0x01095
                // Carla 1.9.5 (2.0-beta3) changed to PLUGIN_SFZ.
                returnValue = carla_add_plugin(BINARY_NATIVE, PLUGIN_SFZ,plugin.carlaPluginData.path.toLocal8Bit(),
                                      plugin.carlaPluginData.name.toLocal8Bit(),"sfz",0,NULL);
                #else
                // 2015-03-28 Updated to Carla 1.9.6 (2.0-beta4). carla_add_plugin now has an additional parameter.
                returnValue = carla_add_plugin(BINARY_NATIVE, PLUGIN_SFZ,plugin.carlaPluginData.path.toLocal8Bit(),
                                                  plugin.carlaPluginData.name.toLocal8Bit(),"sfz",0,NULL,0);

                userMessage("Loading sfz: " + plugin.carlaPluginData.path + ", " + plugin.carlaPluginData.name);
                #endif
            } else if ( plugin.carlaPluginData.pluginType == KonfytCarlaPluginType_Internal ) {

                #if CARLA_VERSION_HEX > 0x01095

                plugin.carlaPluginData.name.prepend("plugin_" + n2s(index));
                returnValue = carla_add_plugin( BINARY_NATIVE,      // Binary type
                                                PLUGIN_INTERNAL,    // Plugin type
                                                NULL,               // Filename
                                                plugin.carlaPluginData.name.toLocal8Bit(),  // Name of plugin
                                                plugin.carlaPluginData.path.toLocal8Bit(),  // Plugin label
                                                0,      // Plugin unique id, if applicable
                                                NULL,   // Extra pointer (defined per plugin type)
                                                0       // Initial plugin options
                                                );

                #endif

            }
*******************************************************************************/


// Adds a new plugin and returns the unique ID. Returns -1 on error.
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
        return -1;
    }

    CARLA_FUNC(carla_set_active, pluginIdInCarla, true);
    CARLA_FUNC(carla_set_option, pluginIdInCarla, PLUGIN_OPTION_SEND_CONTROL_CHANGES, true);

    // Insert to map of unique IDs (in this class) and plugin data
    pluginDataMap.insert( pluginData.ID, pluginData );
    // Add to list with indexes corresponding to pluginIds in Carla engine
    pluginList.append(pluginData.ID);

    pluginUniqueIDCounter++;

    return pluginData.ID; // Return the unique ID used by this class to distinguish the plugin
}

void KonfytCarlaEngine::removeSfz(int ID)
{
    Q_ASSERT( pluginDataMap.contains(ID) );
    Q_ASSERT( pluginList.contains(ID) );

    KonfytCarlaPluginData pluginData = pluginDataMap.value(ID);
    pluginDataMap.remove(ID);
    int pluginIdInCarla = pluginList.indexOf(ID);
    pluginList.removeAt(pluginIdInCarla);

    bool ret = CARLA_FUNC(carla_remove_plugin, pluginIdInCarla);
    if (!ret) {
        print("ERROR - Failed to remove plugin from Carla. ID: " + n2s(ID) + ", pluginIdInCarla: " + n2s(pluginIdInCarla));
    }
}

QString KonfytCarlaEngine::pluginName(int ID)
{
    Q_ASSERT( pluginDataMap.contains(ID) );

    return pluginDataMap[ID].name;
}

QString KonfytCarlaEngine::midiInJackPortName(int ID)
{
    Q_ASSERT( pluginDataMap.contains(ID) );

    // TODO: This depends on Carla naming the ports as we expect. A better way would be
    // to get the port names from a Carla callback.

    return jack_client_name + ":" + pluginName(ID) + ":" + QString::fromLocal8Bit(CARLA_MIDI_IN_PORT_POSTFIX);
}

QStringList KonfytCarlaEngine::audioOutJackPortNames(int ID)
{
    Q_ASSERT( pluginDataMap.contains(ID) );

    // TODO: This depends on Carla naming the ports as we expect. A better way would be
    // to get the port names from a Carla callback.

    QStringList ret;

    ret.append( jack_client_name + ":" + pluginName(ID) + ":" + QString::fromLocal8Bit(CARLA_OUT_LEFT_PORT_POSTFIX) );
    ret.append( jack_client_name + ":" + pluginName(ID) + ":" + QString::fromLocal8Bit(CARLA_OUT_RIGHT_PORT_POSTFIX) );

    return ret;
}

void KonfytCarlaEngine::initEngine(KonfytJackEngine* jackEngine)
{
    jack = jackEngine;
    jack_client_name = jack->clientName() + CARLA_CLIENT_POSTFIX;

    print("Carla version " + QString(CARLA_VERSION_STRING));

    // Initialise Carla Backend
#ifdef CARLA_USE_HANDLE
    carlaHandle = carla_standalone_host_init();
#endif
    CARLA_FUNC(carla_set_engine_option, ENGINE_OPTION_PROCESS_MODE, ENGINE_PROCESS_MODE_SINGLE_CLIENT, NULL);
    // Set path to the resource files.
    // Default unset.
    // Must be set for some internal plugins to work
    CARLA_FUNC(carla_set_engine_option, ENGINE_OPTION_PATH_RESOURCES, 0, "/usr/lib/lv2/carla.lv2/resources/");
    // Set the engine callback
    //carla_set_engine_callback(KonfytCarlaEngine::carlaEngineCallback, this);
    // TODO: Handle the case where this name is already taken.
    bool ok = CARLA_FUNC(carla_engine_init, "JACK", jack_client_name.toLocal8Bit().constData());

    QString error;
    if (!ok) {
        error = CARLA_FUNC_V(carla_get_last_error);
        print("Carla engine initialisation error:");
        print(error);
    }
    emit initDone(error);
}

QString KonfytCarlaEngine::jackClientName()
{
    return jack_client_name;
}

void KonfytCarlaEngine::setGain(int ID, float newGain)
{
    Q_ASSERT( pluginDataMap.contains(ID) );
    Q_ASSERT( pluginList.contains(ID) );

    int pluginIdInCarla = pluginList.indexOf(ID);

    CARLA_FUNC(carla_set_volume, pluginIdInCarla, newGain);
}

// Print error message to stdout, and abort app.
void KonfytCarlaEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: sfengine:" << msg.toLocal8Bit().constData();
    abort();
}
