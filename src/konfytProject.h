/******************************************************************************
 *
 * Copyright 2024 Gideon van der Kolf
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

#include "konfytUtils.h"
#include "konfytJackStructs.h"
#include "konfytPatch.h"
#include "konfytStructs.h"
#include "xml.h"

#include <QDir>
#include <QFile>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>


class Project;
typedef QSharedPointer<Project> ProjectPtr;


class Project : public QObject
{
    Q_OBJECT
public:

    // -----------------------------------------------------------------------

    struct AudioPort
    {
        QString name;
        KfJackAudioPort* leftJackPort = nullptr;
        KfJackAudioPort* rightJackPort = nullptr;
        float leftGain = 1;
        float rightGain = 1;
        QStringList leftClients;
        QStringList rightClients;
        bool ignoreMasterGain = false; // Only applicable for output ports (buses)
    };
    typedef QSharedPointer<AudioPort> AudioPortPtr;

    // -----------------------------------------------------------------------

    struct MidiPort
    {
        QString name;
        QStringList clients;
        KfJackMidiPort* jackPort = nullptr; // TODO THIS MUST NOT BE IN PROJECT
        MidiFilter filter = MidiFilter::allPassFilter();
        QString script;
        bool scriptEnabled = false;
        // True to pass original events through in addition to script processing.
        bool passMidiThrough = true;
    };
    typedef QSharedPointer<MidiPort> MidiPortPtr;

    // -----------------------------------------------------------------------

    struct Trigger
    {
        QString actionText;
        int type = -1;
        int channel = 0;
        int data1 = -1;
        int bankMSB = -1;
        int bankLSB = -1;

        int toInt() const;
        QString toString();
    };

    // -----------------------------------------------------------------------

    struct ExternalApp
    {
        ExternalApp() {}
        ExternalApp(QString name, QString cmd);

        QString friendlyName;
        QString command;
        bool runAtStartup = false;
        bool autoRestart = false;
        bool warnBeforeClosing = true;
        bool startDetached = false;

        QString displayName();
    };

    // -----------------------------------------------------------------------

    enum PortLeftRight { LeftPort, RightPort };

    static const QString NEW_PROJECT_DEFAULT_NAME;
    static const QString PROJECT_FILE_EXTENSION_WITHDOT;
    static const QString PROJECT_PATCH_DIR;
    static const QString PROJECT_BACKUP_DIR;

    explicit Project(QObject *parent = 0);

    Result loadProject(QString filepath);
    Result saveProject();
    Result saveProjectAs(QString dirpath);
    bool isModified();
    void setModified(bool mod);

    void addPatch(PatchPtr newPatch);
    void insertPatch(PatchPtr newPatch, int index);
    PatchPtr removePatch(int index);
    void movePatch(int indexFrom, int indexTo);
    PatchPtr getPatch(int index);
    int getPatchIndex(PatchPtr patch);
    QList<PatchPtr> getPatchList();
    int getNumPatches();

    QString getDirPath();
    void setDirPath(QString path);
    QString getProjectName();
    void setProjectName(QString newName);
    QString getProjectFilePath();
    QString getProjectFileName();
    void setProjectFileName(QString newName);
    bool getShowPatchListNumbers();
    void setShowPatchListNumbers(bool show);
    bool getShowPatchListNotes();
    void setShowPatchListNotes(bool show);
    void setMidiPickupRange(int range);
    int getMidiPickupRange();
    KonfytReset getResetOption();
    void setResetOption(KonfytReset option);

    // -----------------------------------------------------------------------
    // MIDI input ports

    QList<int> midiInPort_getAllPortIds() const;  // Get list of port ids
    QList<MidiPortPtr> midiInPort_getAllPorts();
    int midiInPort_addPort(QString portName);
    void midiInPort_removePort(int portId);
    bool midiInPort_exists(int portId) const;
    MidiPortPtr midiInPort_getPort(int portId) const;
    int midiInPort_idFromPtr(MidiPortPtr p);
    int midiInPort_getPortIdWithJackId(KfJackMidiPort *jackPort);
    int midiInPort_getFirstPortId(int skipId);
    int midiInPort_count();
    void midiInPort_setName(int portId, QString name);
    void midiInPort_setJackPort(int portId, KfJackMidiPort* jackport);
    QStringList midiInPort_getClients(int portId);  // Get client list of single port
    void midiInPort_addClient(int portId, QString client);
    void midiInPort_removeClient(int portId, QString client);
    void midiInPort_setPortFilter(int portId, MidiFilter filter);

    // -----------------------------------------------------------------------
    // MIDI output ports

    QList<int> midiOutPort_getAllPortIds() const;  // Get list of port ids
    QList<MidiPortPtr> midiOutPort_getAllPorts();
    int midiOutPort_addPort(QString portName);
    void midiOutPort_removePort(int portId);
    bool midiOutPort_exists(int portId) const;
    MidiPortPtr midiOutPort_getPort(int portId) const;
    int midiOutPort_idFromPtr(MidiPortPtr p);
    int midiOutPort_count();
    void midiOutPort_setName(int portId, QString name);
    void midiOutPort_setJackPort(int portId, KfJackMidiPort* jackport);
    QStringList midiOutPort_getClients(int portId);  // Get client list of single port
    void midiOutPort_addClient(int portId, QString client);
    void midiOutPort_removeClient(int portId, QString client);

    // -----------------------------------------------------------------------
    // Audio input ports

    QList<int> audioInPort_getAllPortIds() const; // Get list of port ids
    QList<AudioPortPtr> audioInPort_getAllPorts();
    int audioInPort_add(QString portName);
    void audioInPort_remove(int portId);
    bool audioInPort_exists(int portId) const;
    AudioPortPtr audioInPort_getPort(int portId) const;
    int audioInPort_idFromPtr(AudioPortPtr p);
    int audioInPort_count();
    void audioInPort_setName(int portId, QString name);
    void audioInPort_setJackPorts(int portId, KfJackAudioPort* left, KfJackAudioPort* right);
    void audioInPort_addClient(int portId, PortLeftRight leftRight, QString client);
    void audioInPort_removeClient(int portId, PortLeftRight leftRight, QString client);

    // -----------------------------------------------------------------------
    // Audio buses (output ports)

    int audioBus_add(QString busName);
    void audioBus_remove(int busId);
    int audioBus_count();
    bool audioBus_exists(int busId);
    AudioPortPtr audioBus_getBus(int busId) const;
    int audioBus_idFromPtr(AudioPortPtr p);
    int audioBus_getFirstBusId(int skipId);
    QList<int> audioBus_getAllBusIds() const;
    QList<AudioPortPtr> audioBus_getAllBuses();
    void audioBus_setName(int busId, QString name);
    void audioBus_replace(int busId, AudioPortPtr newBus);
    void audioBus_replace_noModify(int busId, AudioPortPtr newBus);
    void audioBus_addClient(int busId, PortLeftRight leftRight, QString client);
    void audioBus_removeClient(int busId, PortLeftRight leftRight, QString client);

    // -----------------------------------------------------------------------
    // External Apps

    int addExternalApp(ExternalApp app);
    void removeExternalApp(int id);
    ExternalApp getExternalApp(int id);
    QList<int> getExternalAppIds();
    bool hasExternalAppWithId(int id);
    void modifyExternalApp(int id, ExternalApp app);

    // -----------------------------------------------------------------------
    // Triggers

    void addAndReplaceTrigger(Trigger newTrigger);
    void removeTrigger(QString actionText);
    QList<Trigger> getTriggerList();
    bool isProgramChangeSwitchPatches();
    void setProgramChangeSwitchPatches(bool value);

    // -----------------------------------------------------------------------
    // Other JACK MIDI connections

    KonfytJackConPair addJackMidiConMake(QString srcPort, QString destPort);
    KonfytJackConPair addJackMidiConBreak(QString srcPort, QString destPort);
    QList<KonfytJackConPair> getJackMidiConList();
    KonfytJackConPair removeJackMidiCon(int i);

    // -----------------------------------------------------------------------
    // Other JACK Audio connections

    KonfytJackConPair addJackAudioConMake(QString srcPort, QString destPort);
    KonfytJackConPair addJackAudioConBreak(QString srcPort, QString destPort);
    QList<KonfytJackConPair> getJackAudioConList();
    KonfytJackConPair removeJackAudioCon(int i);

signals:
    void print(QString msg) const;
    void projectModifiedStateChanged(bool modified);
    void projectNameChanged();
    void patchURIsNeedUpdating();
    void audioBusNameChanged(int busId);
    void midiInPortNameChanged(int portId);
    void midiOutPortNameChanged(int portId);
    void audioInPortNameChanged(int portId);
    void midiPickupRangeChanged(int range);

    void externalAppAdded(int id);
    void externalAppRemoved(int id);
    void externalAppModified(int id);
    
private:
    QString mProjectDirpath;
    QString mProjectName = NEW_PROJECT_DEFAULT_NAME;
    QString mProjectFilename;
    KonfytReset mResetOption = KonfytReset::NoReset;

    QString filenameFromProjectName();
    void backupProject(QString dirpath, QString projectFilename, QString tag);
    Xml toXml() const;

    QList<PatchPtr> mPatches;
    void readPatchesFromProjectXml(Xml projectXml);
    void addPatchesToProjectXml(Xml* projectXml) const;

    QMap<int, MidiPortPtr> midiInPortMap;
    int midiInPort_getUniqueId();
    void readMidiInPortsFromProjectXml(Xml projectXml);
    void addMidiInPortsToProjectXml(Xml* projectXml) const;

    QMap<int, MidiPortPtr> midiOutPortMap;
    int midiOutPort_getUniqueId();
    void readMidiOutPortsFromProjectXml(Xml projectXml);
    void addMidiOutPortsToProjectXml(Xml* projectXml) const;

    QMap<int, AudioPortPtr> audioInPortMap;
    int audioInPort_getUniqueId();
    void readAudioInPortsFromProjectXml(Xml projectXml);
    void addAudioInPortsToProjectXml(Xml* projectXml) const;

    QMap<int, AudioPortPtr> audioBusMap;
    int audioBus_getUniqueId();
    void readBusesFromProjectXml(Xml projectXml);
    void addBusesToProjectXml(Xml* projectXml) const;

    int getUniqueIdNotInList(QList<int> ids);

    struct PortScriptData {
        QString content;
        bool enabled = false;
        bool passMidiThrough = true;
    };
    Xml portScriptToXml(MidiPortPtr port) const;
    PortScriptData readPortScript(Xml xml);

    QMap<int, ExternalApp> mExternalApps;
    int getUniqueExternalAppId();
    void clearExternalApps();
    void addExternalAppsToXml(Xml* xml) const;
    void readExternalApps(Xml xml);

    QHash<QString, Trigger> mTriggerHash;
    bool mProgramChangeSwitchPatches = true;
    bool mShowPatchListNumbers = true;
    bool mShowPatchListNotes = false;
    int mMidiPickupRange = 127;
    void readTriggersFromProjectXml(Xml projectXml);
    void addTriggersToProjectXml(Xml* projectXml) const;

    QList<KonfytJackConPair> mJackMidiConList;
    void readJackMidiConList(Xml xml, bool makeNotBreak);
    QList<KonfytJackConPair> mJackAudioConList;
    void readJackAudioConList(Xml xml, bool makeNotBreak);

    bool mModified = false; // Whether project has been modified since last load/save.

private:
    static constexpr const char* XML_PROJECT = "konfytProject";
    static constexpr const char* XML_PROJECT_NAME = "name";
    static constexpr const char* XML_KONFYT_VERSION = "KonfytVersion";
    static constexpr const char* XML_RESET_OPTION = "resetOption";
    static constexpr const char* XML_PATCH = "patch";
    static constexpr const char* XML_PATCH_FILENAME = "filename";
    static constexpr const char* XML_PATCH_LIST_NUMBERS = "patchListNumbers";
    static constexpr const char* XML_PATCH_LIST_NOTES = "patchListNotes";
    static constexpr const char* XML_MIDI_PICKUP_RANGE = "midiPickupRange";
    static constexpr const char* XML_MIDI_IN_PORTLIST = "midiInPortList";
    static constexpr const char* XML_MIDI_IN_PORT = "port";
    static constexpr const char* XML_MIDI_IN_PORT_ID = "portId";
    static constexpr const char* XML_MIDI_IN_PORT_NAME = "portName";
    static constexpr const char* XML_MIDI_IN_PORT_CLIENT = "client";
    static constexpr const char* XML_MIDI_OUT_PORTLIST = "midiOutPortList";
    static constexpr const char* XML_MIDI_OUT_PORT = "port";
    static constexpr const char* XML_MIDI_OUT_PORT_ID = "portId";
    static constexpr const char* XML_MIDI_OUT_PORT_NAME = "portName";
    static constexpr const char* XML_MIDI_OUT_PORT_CLIENT = "client";
    static constexpr const char* XML_BUSLIST = "audioBusList";
    static constexpr const char* XML_BUS = "bus";
    static constexpr const char* XML_BUS_ID = "busId";
    static constexpr const char* XML_BUS_NAME = "busName";
    static constexpr const char* XML_BUS_LGAIN = "leftGain";
    static constexpr const char* XML_BUS_RGAIN = "rightGain";
    static constexpr const char* XML_BUS_IGNORE_GLOBAL_VOLUME = "ignoreGlobalVolume";
    static constexpr const char* XML_BUS_LCLIENT = "leftClient";
    static constexpr const char* XML_BUS_RCLIENT = "rightClient";
    static constexpr const char* XML_AUDIOINLIST = "audioInputPortList";
    static constexpr const char* XML_AUDIOIN_PORT = "port";
    static constexpr const char* XML_AUDIOIN_PORT_ID = "portId";
    static constexpr const char* XML_AUDIOIN_PORT_NAME = "portName";
    static constexpr const char* XML_AUDIOIN_PORT_LGAIN = "leftGain";
    static constexpr const char* XML_AUDIOIN_PORT_RGAIN = "rightGain";
    static constexpr const char* XML_AUDIOIN_PORT_LCLIENT = "leftClient";
    static constexpr const char* XML_AUDIOIN_PORT_RCLIENT = "rightClient";

    static constexpr const char* XML_PORT_SCRIPT = "script";
    static constexpr const char* XML_PORT_SCRIPT_CONTENT = "content";
    static constexpr const char* XML_PORT_SCRIPT_ENABLED = "enabled";
    static constexpr const char* XML_PORT_SCRIPT_PASS_MIDI_THROUGH = "passMidiThrough";

    static constexpr const char* XML_EXT_APP_LIST = "externalAppList";
    static constexpr const char* XML_EXT_APP = "externalApp";
    static constexpr const char* XML_EXT_APP_NAME = "friendlyName";
    static constexpr const char* XML_EXT_APP_CMD = "command";
    static constexpr const char* XML_EXT_APP_RUNATSTARTUP = "runAtStartup";
    static constexpr const char* XML_EXT_APP_RESTART = "autoRestart";
    static constexpr const char* XML_EXT_APP_WARN_BEFORE_CLOSING = "warnBeforeClosing";
    static constexpr const char* XML_EXT_APP_START_DETACHED = "startDetached";

    static constexpr const char* XML_TRIGGERLIST = "triggerList";
    static constexpr const char* XML_TRIGGER = "trigger";
    static constexpr const char* XML_TRIGGER_ACTIONTEXT = "actionText";
    static constexpr const char* XML_TRIGGER_TYPE = "type";
    static constexpr const char* XML_TRIGGER_CHAN = "channel";
    static constexpr const char* XML_TRIGGER_DATA1 = "data1";
    static constexpr const char* XML_TRIGGER_BANKMSB = "bankMSB";
    static constexpr const char* XML_TRIGGER_BANKLSB = "bankLSB";
    static constexpr const char* XML_PROG_CHANGE_SWITCH_PATCHES = "programChangeSwitchPatches";
    static constexpr const char* XML_OTHERJACK_MIDI_CON_LIST = "otherJackMidiConList";
    static constexpr const char* XML_OTHERJACK_MIDI_CON_BREAK_LIST = "otherJackMidiConBreakList";
    static constexpr const char* XML_OTHERJACK_AUDIO_CON_LIST = "otherJackAudioConList";
    static constexpr const char* XML_OTHERJACK_AUDIO_CON_BREAK_LIST = "otherJackAudioConBreakList";
    static constexpr const char* XML_OTHERJACKCON = "otherJackCon";
    static constexpr const char* XML_OTHERJACKCON_SRC = "srcPort";
    static constexpr const char* XML_OTHERJACKCON_DEST = "destPort";
};

#endif // KONFYT_PROJECT_H
