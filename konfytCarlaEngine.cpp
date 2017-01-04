#include "konfytCarlaEngine.h"
#include <stdio.h>
#include <iostream>


konfytCarlaEngine::konfytCarlaEngine(QObject *parent) :
    QObject(parent)
{
    pluginUniqueIDCounter = 0;
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
int konfytCarlaEngine::addSFZ(QString path)
{
    konfytCarlaPluginData pluginData;
    pluginData.ID = pluginUniqueIDCounter;
    int pluginIdInCarla = pluginList.count();
    pluginData.path = path;
    pluginData.name = "plugin_" + n2s(pluginData.ID) + "_sfz";
    // Load the plugin

    bool returnValue;
    userMessage("Loading sfz: " + pluginData.path + ", " + pluginData.name);
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
    returnValue = carla_add_plugin(BINARY_NATIVE, PLUGIN_SFZ,pluginData.path.toLocal8Bit(),
                                      pluginData.name.toLocal8Bit(),"sfz",0,NULL,0);
    #endif

    if ( !returnValue ) {
        // Not a success.
        userMessage("Carla failed to load plugin.");
        return -1;
    }

    carla_set_active(pluginIdInCarla, true);
    carla_set_option(pluginIdInCarla, PLUGIN_OPTION_SEND_CONTROL_CHANGES, true);

    // Insert to map of unique IDs (in this class) and plugin data
    pluginDataMap.insert( pluginData.ID, pluginData );
    // Add to list with indexes corresponding to pluginIds in Carla engine
    pluginList.append(pluginData.ID);

    pluginUniqueIDCounter++;

    // Please don't look at the following section.
    // Ugly workaround: for some reason, when adding the first plugin in the engine and then
    // one or more directly afterwards, the first plugin is soundless.
    // So, I'm not proud of it, but for now we just pause a short while after loading the
    // first plugin.
    if (pluginIdInCarla == 0) {
        QTime t;
        t.start();
        while (t.elapsed() < 500) { }
    }

    return pluginData.ID; // Return the unique ID used by this class to distinguish the plugin
}

void konfytCarlaEngine::removeSFZ(int ID)
{
    Q_ASSERT( pluginDataMap.contains(ID) );
    Q_ASSERT( pluginList.contains(ID) );

    konfytCarlaPluginData pluginData = pluginDataMap.value(ID);
    pluginDataMap.remove(ID);
    int pluginIdInCarla = pluginList.indexOf(ID);
    pluginList.removeAt(pluginIdInCarla);

    bool ret = carla_remove_plugin( pluginIdInCarla );
    if (!ret) {
        userMessage("ERROR - Failed to remove plugin from Carla. ID: " + n2s(ID) + ", pluginIdInCarla: " + n2s(pluginIdInCarla));
    }
}

QString konfytCarlaEngine::pluginName(int ID)
{
    Q_ASSERT( pluginDataMap.contains(ID) );

    return pluginDataMap[ID].name;
}

QString konfytCarlaEngine::carlaJackClientName()
{
    return jackClientName;
}

QString konfytCarlaEngine::midiInPort(int ID)
{
    Q_ASSERT( pluginDataMap.contains(ID) );
// todo carla: not sure why we're searching like this. Find out and fix if need be or explain in a comment.
    QString ret;
    QStringList pmidi = jack->getMidiInputPortsList();
    QString search = jackClientName;
    search = search + ":" + pluginName(ID);
    for (int i=0; i<pmidi.count(); i++) {
        if (pmidi.at(i).contains(search)) {
            ret = pmidi.at(i);
        }
    }
    return ret;
}

QStringList konfytCarlaEngine::audioOutPorts(int ID)
{
    Q_ASSERT( pluginDataMap.contains(ID) );
// todo carla: not sure why we're searching like this. Find out and fix if need be or explain in a comment.
    QStringList ret;
    ret << "" << "";

    QStringList paudio = jack->getAudioOutputPortsList();
    QString search_left = jackClientName;
    search_left = search_left + ":" + pluginName(ID) + ":";
    QString search_right = search_left + QString::fromLocal8Bit(CARLA_OUT_RIGHT);
    search_left = search_left + QString::fromLocal8Bit(CARLA_OUT_LEFT);
    for (int i=0; i<paudio.count(); i++) {
        if (paudio.at(i).contains(search_left)) {
            ret.replace(0, paudio.at(i));
        } else if (paudio.at(i).contains(search_right)) {
            ret.replace(1, paudio.at(i));
        }
    }
    return ret;
}

void konfytCarlaEngine::InitCarlaEngine(konfytJackEngine* jackEngine, QString carlaJackClientName)
{
    userMessage("Carla version " + QString(CARLA_VERSION_STRING));
    this->jack = jackEngine;
    this->jackClientName = carlaJackClientName;
    // Initialise Carla Backend
    carla_set_engine_option(ENGINE_OPTION_PROCESS_MODE, ENGINE_PROCESS_MODE_SINGLE_CLIENT, NULL);
    // Set path to the resource files.
    // Default unset.
    // Must be set for some internal plugins to work
    carla_set_engine_option(ENGINE_OPTION_PATH_RESOURCES, 0, "/usr/lib/lv2/carla.lv2/resources/");
    // Set the engine callback
    carla_set_engine_callback(konfytCarlaEngine::carlaEngineCallback, this);
    // TODO: Handle the case where this name is already taken.
    carla_engine_init("JACK", carlaJackClientName.toLocal8Bit().constData());
}

void konfytCarlaEngine::carlaEngineCallback(void *ptr, EngineCallbackOpcode action, uint pluginId, int value1, int value2, float value3, const char *valueStr)
{
    return;

    konfytCarlaEngine* e = (konfytCarlaEngine*)ptr;

    e->userMessage("Carla callback, action " + n2s(action) + ", plugin " + n2s(pluginId));

    QString msg = "CARLA CALLBACK: ";

    switch (action) {
    case ENGINE_CALLBACK_PLUGIN_ADDED:
        e->userMessage(msg + "Plugin added, pluginId " + n2s(pluginId));
        break;
    case ENGINE_CALLBACK_PLUGIN_REMOVED:
        e->userMessage(msg + "Plugin removed, pluginId " + n2s(pluginId));
        break;
    case ENGINE_CALLBACK_PARAMETER_VALUE_CHANGED:
        e->userMessage(msg + "Plugin parameter value changed, pluginId " + n2s(pluginId) + ", param " + n2s(value1) + ", value " + n2s(value3));
    }


}


void konfytCarlaEngine::setGain(int ID, float newGain)
{
    Q_ASSERT( pluginDataMap.contains(ID) );
    Q_ASSERT( pluginList.contains(ID) );

    int pluginIdInCarla = pluginList.indexOf(ID);

    carla_set_volume( pluginIdInCarla, newGain );
}




// Print error message to stdout, and abort app.
void konfytCarlaEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: sfengine:" << msg.toLocal8Bit().constData();
    abort();
}
