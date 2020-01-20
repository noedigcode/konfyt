/******************************************************************************
 *
 * Copyright 2020 Gideon van der Kolf
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

#ifndef KONFYT_PROJECT_H
#define KONFYT_PROJECT_H

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
#define XML_PRJ_PATCH_LIST_NUMBERS "patchListNumbers"
#define XML_PRJ_PATCH_LIST_NOTES "patchListNotes"
#define XML_PRJ_MIDI_IN_PORTLIST "midiInPortList"
#define XML_PRJ_MIDI_IN_PORT "port"
#define XML_PRJ_MIDI_IN_PORT_ID "portId"
#define XML_PRJ_MIDI_IN_PORT_NAME "portName"
#define XML_PRJ_MIDI_IN_PORT_CLIENT "client"
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
#define XML_PRJ_AUDIOIN_PORT_LCLIENT "leftClient"
#define XML_PRJ_AUDIOIN_PORT_RCLIENT "rightClient"
#define XML_PRJ_PROCESSLIST "processList"
#define XML_PRJ_PROCESS "process"
#define XML_PRJ_PROCESS_APPNAME "appname"
#define XML_PRJ_TRIGGERLIST "triggerList"
#define XML_PRJ_TRIGGER "trigger"
#define XML_PRJ_TRIGGER_ACTIONTEXT "actionText"
#define XML_PRJ_TRIGGER_TYPE "type"
#define XML_PRJ_TRIGGER_CHAN "channel"
#define XML_PRJ_TRIGGER_DATA1 "data1"
#define XML_PRJ_TRIGGER_BANKMSB "bankMSB"
#define XML_PRJ_TRIGGER_BANKLSB "bankLSB"
#define XML_PRJ_PROG_CHANGE_SWITCH_PATCHES "programChangeSwitchPatches"
#define XML_PRJ_OTHERJACK_MIDI_CON_LIST "otherJackMidiConList"
#define XML_PRJ_OTHERJACK_AUDIO_CON_LIST "otherJackAudioConList"
#define XML_PRJ_OTHERJACKCON "otherJackCon"
#define XML_PRJ_OTHERJACKCON_SRC "srcPort"
#define XML_PRJ_OTHERJACKCON_DEST "destPort"

struct PrjAudioBus
{
    QString busName;
    int leftJackPortId = -1;
    float leftGain = 1;
    QStringList leftOutClients;
    int rightJackPortId = -1;
    float rightGain = 1;
    QStringList rightOutClients;
};

struct PrjAudioInPort
{
    QString portName;
    int leftJackPortId = -1;
    int rightJackPortId = -1;
    float leftGain = 1;
    float rightGain = 1;
    QStringList leftInClients;
    QStringList rightInClients;
};

struct PrjMidiPort
{
    QString portName;
    QStringList clients;
    int jackPortId = -1;
    KonfytMidiFilter filter;
};

struct KonfytTrigger
{
    QString actionText;
    int type = -1;
    int channel = 0;
    int data1 = -1;
    int bankMSB = -1;
    int bankLSB = -1;

    int toInt() const
    {
        return hashMidiEventToInt(type, channel, data1, bankMSB, bankLSB);
    }
    QString toString()
    {
        return midiEventToString(type, channel, data1, bankMSB, bankLSB);
    }
};

enum portLeftRight { leftPort, rightPort };

class KonfytProject : public QObject
{
    Q_OBJECT
public:
    explicit KonfytProject(QObject *parent = 0);

    bool loadProject(QString filename);
    bool saveProject();
    bool saveProjectAs(QString dirname);

    void addPatch(KonfytPatch* newPatch);
    KonfytPatch* removePatch(int i);
    void movePatchUp(int i);
    void movePatchDown(int i);
    KonfytPatch* getPatch(int i);
    QList<KonfytPatch*> getPatchList();
    int getNumPatches();

    QString getDirname();
    void setDirname(QString newDirname);
    QString getProjectName();
    void setProjectName(QString newName);
    bool getShowPatchListNumbers();
    void setShowPatchListNumbers(bool show);
    bool getShowPatchListNotes();
    void setShowPatchListNotes(bool show);

    // MIDI input ports
    QList<int> midiInPort_getAllPortIds();  // Get list of port ids
    int midiInPort_addPort(QString portName);
    void midiInPort_removePort(int portId);
    bool midiInPort_exists(int portId);
    PrjMidiPort midiInPort_getPort(int portId);
    int midiInPort_getPortIdWithJackId(int jackId);
    int midiInPort_getFirstPortId(int skipId);
    int midiInPort_count();
    void midiInPort_replace(int portId, PrjMidiPort port);
    void midiInPort_replace_noModify(int portId, PrjMidiPort port);
    QStringList midiInPort_getClients(int portId);  // Get client list of single port
    void midiInPort_addClient(int portId, QString client);
    void midiInPort_removeClient(int portId, QString client);
    void midiInPort_setPortFilter(int portId, KonfytMidiFilter filter);

    // MIDI output ports
    QList<int> midiOutPort_getAllPortIds();  // Get list of port ids
    int midiOutPort_addPort(QString portName);
    void midiOutPort_removePort(int portId);
    bool midiOutPort_exists(int portId) const;
    PrjMidiPort midiOutPort_getPort(int portId) const;
    int midiOutPort_count();
    void midiOutPort_replace(int portId, PrjMidiPort port);
    void midiOutPort_replace_noModify(int portId, PrjMidiPort port);
    QStringList midiOutPort_getClients(int portId);  // Get client list of single port
    void midiOutPort_addClient(int portId, QString client);
    void midiOutPort_removeClient(int portId, QString client);

    // Audio input ports
    QList<int> audioInPort_getAllPortIds(); // Get list of port ids
    int audioInPort_add(QString portName);
    void audioInPort_remove(int portId);
    bool audioInPort_exists(int portId) const;
    PrjAudioInPort audioInPort_getPort(int portId) const;
    int audioInPort_count();
    void audioInPort_replace(int portId, PrjAudioInPort newPort);
    void audioInPort_replace_noModify(int portId, PrjAudioInPort newPort);
    void audioInPort_addClient(int portId, portLeftRight leftRight, QString client);
    void audioInPort_removeClient(int portId, portLeftRight leftRight, QString client);

    // Audio buses
    int audioBus_add(QString busName);
    void audioBus_remove(int busId);
    int audioBus_count();
    bool audioBus_exists(int busId);
    PrjAudioBus audioBus_getBus(int busId);
    int audioBus_getFirstBusId(int skipId);
    QList<int> audioBus_getAllBusIds();
    void audioBus_replace(int busId, PrjAudioBus newBus);
    void audioBus_replace_noModify(int busId, PrjAudioBus newBus);
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
    void addAndReplaceTrigger(KonfytTrigger newTrigger);
    void removeTrigger(QString actionText);
    QList<KonfytTrigger> getTriggerList();
    bool isProgramChangeSwitchPatches();
    void setProgramChangeSwitchPatches(bool value);

    // Other JACK MIDI connections
    KonfytJackConPair addJackMidiCon(QString srcPort, QString destPort);
    QList<KonfytJackConPair> getJackMidiConList();
    KonfytJackConPair removeJackMidiCon(int i);
    // Other JACK Audio connections
    KonfytJackConPair addJackAudioCon(QString srcPort, QString destPort);
    QList<KonfytJackConPair> getJackAudioConList();
    KonfytJackConPair removeJackAudioCon(int i);

    bool isModified(); // Returns whether the project has been modified since last load/save.
    void setModified(bool mod);

    void error_abort(QString msg) const;
    
private:
    QList<KonfytPatch*> patchList;
    QString projectDirname;
    QString projectName;

    QMap<int, PrjMidiPort> midiInPortMap;
    int midiInPort_getUniqueId();

    QMap<int, PrjMidiPort> midiOutPortMap;
    int midiOutPort_getUniqueId();

    QMap<int, PrjAudioInPort> audioInPortMap;
    int audioInPort_getUniqueId();

    QMap<int, PrjAudioBus> audioBusMap;
    int audioBus_getUniqueId();

    int getUniqueIdHelper(QList<int> ids);

    QList<konfytProcess*> processList;
    QHash<QString, KonfytTrigger> triggerHash;
    bool programChangeSwitchPatches;
    bool patchListNumbers;
    bool patchListNotes;

    QList<KonfytJackConPair> jackMidiConList;
    QList<KonfytJackConPair> jackAudioConList;

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

private slots:
    // Slots for signals from Process objects.
    // Will then emit the processStarted/Finished signals.
    void processStartedSlot(konfytProcess* process);
    void processFinishedSlot(konfytProcess* process);
};

#endif // KONFYT_PROJECT_H
