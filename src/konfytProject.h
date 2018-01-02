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

#ifndef KONFYG_PROJECT_H
#define KONFYG_PROJECT_H

#include <QDir>
#include <QFile>
#include <QObject>
#include <QStringList>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "konfytPatch.h"
#include "konfytProcess.h"
#include "konfytDefines.h"
#include "konfytStructs.h"
#include "konfytJackStructs.h"

#define PROJECT_FILENAME_EXTENSION ".konfytproject"
#define PROJECT_PATCH_DIR "patches"

#define XML_PRJ "konfytProject"
#define XML_PRJ_NAME "name"
#define XML_PRJ_PATCH "patch"
#define XML_PRJ_PATCH_FILENAME "filename"
#define XML_PRJ_MIDI_IN_AUTOCON_LIST "midiAutoConnectList"
#define XML_PRJ_MIDI_IN_AUTOCON_LIST_PORT "port"
#define XML_PRJ_MIDI_OUT_PORTLIST "midiOutPortList"
#define XML_PRJ_MIDI_OUT_PORT "port"
#define XML_PRJ_MIDI_OUT_PORT_ID "portId"
#define XML_PRJ_MIDI_OUT_PORT_NAME "portName"
#define XML_PRJ_MIDI_OUT_PORT_CLIENT "client"
#define XML_PRJ_BUSLIST "audioBusList"
#define XML_PRJ_BUS "bus"
#define XML_PRJ_BUS_ID "busId"
#define XML_PRJ_BUS_NAME "busName"
#define XML_PRJ_BUS_LGAIN "leftGain"
#define XML_PRJ_BUS_RGAIN "rightGain"
#define XML_PRJ_BUS_LCLIENT "leftClient"
#define XML_PRJ_BUS_RCLIENT "rightClient"
#define XML_PRJ_AUDIOINLIST "audioInputPortList"
#define XML_PRJ_AUDIOIN_PORT "port"
#define XML_PRJ_AUDIOIN_PORT_ID "portId"
#define XML_PRJ_AUDIOIN_PORT_NAME "portName"
#define XML_PRJ_AUDIOIN_PORT_LGAIN "leftGain"
#define XML_PRJ_AUDIOIN_PORT_RGAIN "rightGain"
#define XML_PRJ_AUDIOIN_PORT_BUS "destinationBus"
#define XML_PRJ_AUDIOIN_PORT_LCLIENT "leftClient"
#define XML_PRJ_AUDIOIN_PORT_RCLIENT "rightClient"
#define XML_PRJ_PROCESSLIST "processList"
#define XML_PRJ_PROCESS "process"
#define XML_PRJ_PROCESS_APPNAME "appname"
#define XML_PRJ_PROCESS_DIR "dir" // TODO: NOT USED
#define XML_PRJ_PROCESS_ARG "argument" // TODO: NOT USED
#define XML_PRJ_TRIGGERLIST "triggerList"
#define XML_PRJ_TRIGGER "trigger"
#define XML_PRJ_TRIGGER_ACTIONTEXT "actionText"
#define XML_PRJ_TRIGGER_TYPE "type"
#define XML_PRJ_TRIGGER_CHAN "channel"
#define XML_PRJ_TRIGGER_DATA1 "data1"
#define XML_PRJ_TRIGGER_BANKMSB "bankMSB"
#define XML_PRJ_TRIGGER_BANKLSB "bankLSB"
#define XML_PRJ_OTHERJACKCON_LIST "otherJackConList"
#define XML_PRJ_OTHERJACKCON "otherJackCon"
#define XML_PRJ_OTHERJACKCON_SRC "srcPort"
#define XML_PRJ_OTHERJACKCON_DEST "destPort"

struct prjAudioBus {
    QString busName;
    konfytJackPort* leftJackPort;
    float leftGain;
    QStringList leftOutClients;
    konfytJackPort* rightJackPort;
    float rightGain;
    QStringList rightOutClients;
};

struct prjAudioInPort {
    QString portName;
    konfytJackPort* leftJackPort;
    konfytJackPort* rightJackPort;
    float leftGain;
    float rightGain;
    QStringList leftInClients;
    QStringList rightInClients;
    int destinationBus;
};

struct prjMidiOutPort {
    QString portName;
    QStringList clients;
    konfytJackPort* jackPort;
};

struct konfytTrigger {
    QString actionText;
    int type;
    int channel;
    int data1;
    int bankMSB;
    int bankLSB;

    konfytTrigger() : type(-1),
                      channel(0),
                      data1(-1),
                      bankMSB(-1),
                      bankLSB(-1) {}
    int toInt()
    {
        return hashMidiEventToInt(type, channel, data1, bankMSB, bankLSB);
    }
    QString toString()
    {
        return midiEventToString(type, channel, data1, bankMSB, bankLSB);
    }

};


enum portLeftRight { leftPort, rightPort };

class konfytProject : public QObject
{
    Q_OBJECT
public:
    explicit konfytProject(QObject *parent = 0);

    bool loadProject(QString filename);
    bool saveProject();
    bool saveProjectAs(QString dirname);

    void addPatch(konfytPatch* newPatch);
    konfytPatch* removePatch(int i);
    void movePatchUp(int i);
    void movePatchDown(int i);
    konfytPatch* getPatch(int i);
    QList<konfytPatch*> getPatchList();
    int getNumPatches();

    QString getDirname();
    void setDirname(QString newDirname);
    QString getProjectName();
    void setProjectName(QString newName);

    // midiAutoConnectList - list of ports to auto connect to our main midi input
    QStringList midiInPort_getClients();
    void midiInPort_addClient(QString port);
    void midiInPort_removeClient(QString port);
    void midiInPort_clearClients();

    // Midi output ports
    QList<int> midiOutPort_getAllPortIds();  // Get list of port ids
    int midiOutPort_addPort(QString portName);
    void midiOutPort_removePort(int portId);
    bool midiOutPort_exists(int portId);
    prjMidiOutPort midiOutPort_getPort(int portId);
    int midiOutPort_count();
    void midiOutPort_replace(int portId, prjMidiOutPort port);
    void midiOutPort_replace_noModify(int portId, prjMidiOutPort port);
    QStringList midiOutPort_getClients(int portId);  // Get client list of single port
    void midiOutPort_addClient(int portId, QString client);
    void midiOutPort_removeClient(int portId, QString client);


    // Audio input ports
    QList<int> audioInPort_getAllPortIds(); // Get list of port ids
    int audioInPort_add(QString portName);
    void audioInPort_remove(int portId);
    bool audioInPort_exists(int portId);
    prjAudioInPort audioInPort_getPort(int portId);
    int audioInPort_count();
    void audioInPort_replace(int portId, prjAudioInPort newPort);
    void audioInPort_replace_noModify(int portId, prjAudioInPort newPort);
    void audioInPort_addClient(int portId, portLeftRight leftRight, QString client);
    void audioInPort_removeClient(int portId, portLeftRight leftRight, QString client);

    // Audio busses
    int audioBus_add(QString busName, konfytJackPort *leftJackPort, konfytJackPort *rightJackPort);
    void audioBus_remove(int busId);
    int audioBus_count();
    bool audioBus_exists(int busId);
    prjAudioBus audioBus_getBus(int busId);
    int audioBus_getFirstBusId(int skipId);
    QList<int> audioBus_getAllBusIds();
    void audioBus_replace(int busId, prjAudioBus newBus);
    void audioBus_replace_noModify(int busId, prjAudioBus newBus);
    void audioBus_addClient(int busId, portLeftRight leftRight, QString client);
    void audioBus_removeClient(int busId, portLeftRight leftRight, QString client);

    // External Programs
    void addProcess(konfytProcess *process);
    bool isProcessRunning(int index);
    void runProcess(int index);
    void stopProcess(int index);
    void removeProcess(int index);
    QList<konfytProcess*> getProcessList();

    // Used to determine whether patches are loaded. Uses unique patch id.
    void markPatchLoaded(int patch_id);
    bool isPatchLoaded(int patch_id);

    // Triggers
    void addAndReplaceTrigger(konfytTrigger newTrigger);
    void removeTrigger(QString actionText);
    QList<konfytTrigger> getTriggerList();

    // Other JACK connections
    konfytJackConPair addJackCon(QString srcPort, QString destPort);
    QList<konfytJackConPair> getJackConList();
    konfytJackConPair removeJackCon(int i);

    bool isModified(); // Returns whether the project has been modified since last load/save.
    void setModified(bool mod);

    void error_abort(QString msg);
    
private:
    QList<konfytPatch*> patchList;
    QString projectDirname;
    QString projectName;
    QStringList midiAutoConnectList;

    QMap<int, prjMidiOutPort> midiOutPortMap;
    int midiOutPort_getUniqueId();

    QMap<int, prjAudioInPort> audioInPortMap;
    int audioInPort_getUniqueId();

    QMap<int, prjAudioBus> audioBusMap;
    int audioBus_getUniqueId();

    int getUniqueIdHelper(QList<int> ids);

    QList<konfytProcess*> processList;
    QHash<QString, konfytTrigger> triggerHash;

    QList<konfytJackConPair> jackConList;

    bool modified; // Whether project has been modified since last load/save.

    // Used to determine whether patches are loaded. Uses unique patch id.
    int patch_id_counter;
    QList<int> loaded_patch_ids;

signals:
    void userMessage(QString msg);
    void projectModifiedChanged(bool modified); // Emitted when project modified state changes.

    // Signals emitted when signals are recieved from Process objects.
    void processStartedSignal(int index, konfytProcess* process);
    void processFinishedSignal(int index, konfytProcess* process);
    
public slots:

private slots:
    // Slots for signals from Process objects.
    // Will then emit the processStarted/Finished signals.
    void processStartedSlot(konfytProcess* process);
    void processFinishedSlot(konfytProcess* process);

    
};

#endif // KONFYG_PROJECT_H
