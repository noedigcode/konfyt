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

#include "konfytProject.h"

#include <QDateTime>

#include <QDomDocument>
#include <iostream>

// ============================================================================

const QString Project::NEW_PROJECT_DEFAULT_NAME = "New Project";
const QString Project::PROJECT_FILE_EXTENSION_WITHDOT = ".konfytproject";
const QString Project::PROJECT_PATCH_DIR = "patches";
const QString Project::PROJECT_BACKUP_DIR = "backup";

// ============================================================================

Project::Project(QObject *parent) :
    QObject(parent)
{
    // Project has to have a minimum 1 bus
    // Ports will be assigned later when loading project
    this->audioBus_add("Master Bus");
    // Add at least 1 MIDI input port also
    this->midiInPort_addPort("MIDI In");
}

Result Project::saveProject()
{
    return saveProjectAs(mProjectDirpath);
}

/* Save project XML file, as well as the accompanying files, to the specified
 * directory.
 * If the project already has a filename (i.e. set when loaded) that filename
 * will be used for the project XML file. If the filename is empty, a new
 * filename will be derived from the project name. */
Result Project::saveProjectAs(QString dirpath)
{
    // Abort if directory does not exist
    QDir dir(dirpath);
    if (!dir.exists()) {
        print("saveProjectAs: Directory does not exist.");
        return Result::failure("Direcory does not exist: " + dirpath);
    }
    mProjectDirpath = dirpath;

    // Make a filename from the project name if one is not yet specified.
    // Note: once a project has a filename, the name will not be changed when
    // the project name changes. This makes things simpler and eliminates
    // side effects where other programs rely on the project filename.
    if (mProjectFilename.isEmpty()) {
        mProjectFilename = filenameFromProjectName();
    }

    // Create a backup of the original files before saving
    backupProject(dirpath, mProjectFilename, "a_presave");

    // Create patches dir
    QString patchesPath = dirpath + "/" + PROJECT_PATCH_DIR;
    QDir patchesDir(patchesPath);
    if (!patchesDir.exists()) {
        if (patchesDir.mkdir(patchesPath)) {
            print("saveProjectAs: Created patches directory: " + patchesPath);
        } else {
            print("ERROR: saveProjectAs: Could not create patches directory.");
            return Result::failure("Could not create patches directory: " + patchesPath);
        }
    }

    // Project filename from project name.
    // If different from old filename, first rename the old file.
    QString newFilename = filenameFromProjectName();
    if (newFilename != mProjectFilename) {
        // Rename old file to new name
        QFile file(mProjectFilename);
        if (!file.rename(newFilename)) {
            print(QString("saveProjectAs: Could not rename old project file "
                          "\"%1\" to \"%2\". Error: %3")
                  .arg(mProjectFilename, newFilename, file.errorString()));
        }
    }

    // Open project file for writing
    QString filepath = dirpath + "/" + mProjectFilename;
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        print("saveProjectAs: Could not open file for writing: " + filepath);
        return Result::failure(
            QString("Could not open file for writing. File: %1. Error: %2")
                    .arg(filepath).arg(file.errorString()));
    }

    print("saveProjectAs: Project directory: " + mProjectDirpath);
    print("saveProjectAs: Project filename: " + mProjectFilename);

    Xml xml = toXml();
    QByteArray data = xml.toByteArray();
    qint64 nwritten = file.write(data);
    if (nwritten != data.count()) {
        return Result::failure(QString("Only %1 of %2 bytes written while saving project. "
                                       "File: %3. Error: %4")
                               .arg(filepath).arg(file.errorString()));
    }

    file.close();

    setModified(false);

    // Create another backup of the saved files. This will ensure that if this
    // project is now opened with an earlier version of Konfyt which does not
    // create backups and does not retain data related to newer features, that
    // this version will still be backed up.
    backupProject(dirpath, mProjectFilename, "b_postsave");

    return Result::success();
}

/* Returns whether the project has been mModified since the last load/save. */
bool Project::isModified()
{
    return this->mModified;
}

void Project::setModified(bool mod)
{
    this->mModified = mod;
    emit projectModifiedStateChanged(mod);
}

/* Load project xml file. Patches listed in the project are also loaded from
 * the patches subdirectory. */
Result Project::loadProject(QString filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        return Result::failure(QString("Error opening file for reading. File error: %1")
                               .arg(file.errorString()));
    }
    QByteArray data = file.readAll();
    file.close();

    Xml projectXml;
    Xml::Result xmlResult = projectXml.loadFromData(data);
    if (!xmlResult.ok) {
        return Result::failure(QString("Error loading XML: %1")
                               .arg(xmlResult.toString()));
    }

    QFileInfo fi(file);
    mProjectFilename = fi.fileName();
    mProjectDirpath = fi.dir().path();

    // Pre xml load

    mPatches.clear();
    mMidiInPortMap.clear();
    mMidiOutPortMap.clear();
    clearExternalApps();
    mAudioBusMap.clear();
    mAudioInPortMap.clear();
    mJackMidiConList.clear();
    mJackAudioConList.clear();

    // Load from xml

    // General project settings

    mProjectName = projectXml.attribute(XML_PROJECT_NAME);

    mShowPatchListNumbers = Qstr2bool(projectXml.childText(XML_PATCH_LIST_NUMBERS));
    mShowPatchListNotes = Qstr2bool(projectXml.childText(XML_PATCH_LIST_NOTES));
    mResetOption = konfytResetFromString(
                projectXml.childText(XML_RESET_OPTION),
                KonfytReset::Inherit);
    setMidiPickupRange(projectXml.childInt(XML_MIDI_PICKUP_RANGE));

    // Patches
    readPatchesFromProjectXml(projectXml);

    // MIDI input ports
    readMidiInPortsFromProjectXml(projectXml);

    // MIDI output ports
    readMidiOutPortsFromProjectXml(projectXml);

    // Buses
    readBusesFromProjectXml(projectXml);

    // Audio input ports
    readAudioInPortsFromProjectXml(projectXml);

    // External apps

    Xml externalAppsList = projectXml.child(XML_EXT_APP_LIST);
    readExternalApps(externalAppsList);

    // Triggers
    readTriggersFromProjectXml(projectXml);

    // Other JACK connections

    Xml list = projectXml.child(XML_OTHERJACK_MIDI_CON_LIST);
    readJackMidiConList(list, true);

    list = projectXml.child(XML_OTHERJACK_MIDI_CON_BREAK_LIST);
    readJackMidiConList(list, false);

    list = projectXml.child(XML_OTHERJACK_AUDIO_CON_LIST);
    readJackAudioConList(list, true);

    list = projectXml.child(XML_OTHERJACK_AUDIO_CON_BREAK_LIST);
    readJackAudioConList(list, false);

    // Post xml load

    // Create an audio output bus if there are none, so there is at least one.
    if (audioBus_count() == 0) {
        audioBus_add("Master Bus");
    }

    // Create a MIDI input port if there are none, so there is at least one.
    if (midiInPort_count() == 0) {
        midiInPort_addPort("MIDI In");
    }

    setModified(false);

    return Result::success();
}

bool Project::getShowPatchListNumbers()
{
    return mShowPatchListNumbers;
}

void Project::setShowPatchListNumbers(bool show)
{
    mShowPatchListNumbers = show;
    setModified(true);
}

bool Project::getShowPatchListNotes()
{
    return mShowPatchListNotes;
}

void Project::setShowPatchListNotes(bool show)
{
    mShowPatchListNotes = show;
    setModified(true);
}

void Project::setMidiPickupRange(int range)
{
    if (mMidiPickupRange != range) {
        mMidiPickupRange = range;
        setModified(true);
        emit midiPickupRangeChanged(range);
    }
}

int Project::getMidiPickupRange()
{
    return mMidiPickupRange;
}

KonfytReset Project::getResetOption()
{
    return mResetOption;
}

void Project::setResetOption(KonfytReset option)
{
    if (option != mResetOption) {
        mResetOption = option;
        setModified(true);
    }
}

void Project::addPatch(PatchPtr newPatch)
{
    mPatches.append(newPatch);
    setModified(true);
    patchURIsNeedUpdating();
}

void Project::insertPatch(PatchPtr newPatch, int index)
{
    mPatches.insert(index, newPatch);
    setModified(true);
    patchURIsNeedUpdating();
}

/* Removes the patch with the specified index from project and returns the
 * patch, or a null pointer if the specified index is out of bounds. */
PatchPtr Project::removePatch(int index)
{
    PatchPtr ret;
    if ( (index >= 0) && (index < mPatches.count())) {
        ret = mPatches[index];
        mPatches.removeAt(index);
        setModified(true);
        patchURIsNeedUpdating();
    }
    return ret;
}

void Project::movePatch(int indexFrom, int indexTo)
{
    KONFYT_ASSERT_RETURN( (indexFrom >= 0) && (indexFrom < mPatches.count()) );
    KONFYT_ASSERT_RETURN( (indexTo >= 0) && (indexTo < mPatches.count()) );

    mPatches.move(indexFrom, indexTo);
    setModified(true);
    patchURIsNeedUpdating();
}

PatchPtr Project::getPatch(int index)
{
    return mPatches.value(index);
}

/* Returns the index of the specified patch and -1 if invalid. */
int Project::getPatchIndex(PatchPtr patch)
{
    return mPatches.indexOf(patch);
}

QList<PatchPtr> Project::getPatchList()
{
    return mPatches;
}

int Project::getNumPatches()
{
    return mPatches.count();
}

QString Project::getDirPath()
{
    return mProjectDirpath;
}

void Project::setDirPath(QString path)
{
    mProjectDirpath = path;
    setModified(true);
}

QString Project::getProjectName()
{
    return mProjectName;
}

void Project::setProjectName(QString newName)
{
    mProjectName = newName;
    emit projectNameChanged();
    setModified(true);
}

QString Project::getProjectFilePath()
{
    QString ret = mProjectDirpath;
    if (!ret.isEmpty()) { ret += "/"; }
    ret += mProjectFilename;
    return ret;
}

QString Project::getProjectFileName()
{
    return mProjectFilename;
}

void Project::setProjectFileName(QString newName)
{
    mProjectFilename = newName;
}

QList<int> Project::midiInPort_getAllPortIds() const
{
    return mMidiInPortMap.keys();
}

QList<Project::MidiPortPtr> Project::midiInPort_getAllPorts()
{
    return mMidiInPortMap.values();
}

int Project::midiInPort_addPort(QString portName)
{
    MidiPortPtr p(new MidiPort());

    p->name = portName;

    int portId = midiInPort_getUniqueId();
    mMidiInPortMap.insert(portId, p);

    setModified(true);

    return portId;
}

void Project::midiInPort_removePort(int portId)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    mMidiInPortMap.remove(portId);
    setModified(true);
}

bool Project::midiInPort_exists(int portId) const
{
    return mMidiInPortMap.contains(portId);
}

Project::MidiPortPtr Project::midiInPort_getPort(int portId) const
{
    KONFYT_ASSERT_RETURN_VAL(midiInPort_exists(portId), MidiPortPtr());

    return mMidiInPortMap.value(portId);
}

/* Returns the port id corresponding to the specified port pointer, or -1 if the
 * specified port does not exist. */
int Project::midiInPort_idFromPtr(MidiPortPtr p)
{
    return mMidiInPortMap.key(p, -1);
}

int Project::midiInPort_getPortIdWithJackId(KfJackMidiPort *jackPort)
{
    int ret = -1;

    QList<int> ids = mMidiInPortMap.keys();
    foreach (int id, ids) {
        MidiPortPtr p = mMidiInPortMap[id];
        if (p->jackPort == jackPort) {
            ret = id;
            break;
        }
    }

    return ret;
}

/* Gets the first MIDI Input Port Id that is not skipId. */
int Project::midiInPort_getFirstPortId(int skipId)
{
    int ret = -1;
    QList<int> l = mMidiInPortMap.keys();
    KONFYT_ASSERT_RETURN_VAL(l.count(), ret);

    for (int i=0; i < l.count(); i++) {
        if (l[i] != skipId) {
            ret = l[i];
            break;
        }
    }

    return ret;
}

int Project::midiInPort_count()
{
    return mMidiInPortMap.count();
}

void Project::midiInPort_setName(int portId, QString name)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    mMidiInPortMap[portId]->name = name;
    setModified(true);
    emit midiInPortNameChanged(portId);
}

void Project::midiInPort_setJackPort(int portId, KfJackMidiPort *jackport)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    mMidiInPortMap[portId]->jackPort = jackport;
    // Do not set the project modified
}

QStringList Project::midiInPort_getClients(int portId)
{
    QStringList ret;
    KONFYT_ASSERT_RETURN_VAL(midiInPort_exists(portId), ret);

    ret = mMidiInPortMap.value(portId)->clients;

    return ret;
}

void Project::midiInPort_addClient(int portId, QString client)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    MidiPortPtr p = mMidiInPortMap.value(portId);
    if (p) {
        p->clients.append(client);
        setModified(true);
    }
}

void Project::midiInPort_removeClient(int portId, QString client)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    MidiPortPtr p = mMidiInPortMap.value(portId);
    if (p) {
        p->clients.removeAll(client);
        setModified(true);
    }
}

void Project::midiInPort_setPortFilter(int portId, MidiFilter filter)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    MidiPortPtr p = mMidiInPortMap.value(portId);
    if (p) {
        p->filter = filter;
        setModified(true);
    }
}

int Project::midiOutPort_addPort(QString portName)
{
    MidiPortPtr p(new MidiPort());
    p->name = portName;

    int portId = midiOutPort_getUniqueId();
    mMidiOutPortMap.insert(portId, p);

    setModified(true);

    return portId;
}

int Project::audioBus_getUniqueId()
{
    QList<int> l = mAudioBusMap.keys();
    return getUniqueIdNotInList( l );
}

void Project::readBusesFromProjectXml(Xml projectXml)
{
    Xml buslist = projectXml.child(XML_BUSLIST);
    foreach (Xml busXml, buslist.childrenNamed(XML_BUS)) {

        AudioPortPtr bus(new AudioPort());
        bus->name = busXml.childText(XML_BUS_NAME);
        bus->leftGain = busXml.childFloat(XML_BUS_LGAIN, bus->leftGain);
        bus->rightGain = busXml.childFloat(XML_BUS_RGAIN, bus->rightGain);
        foreach (Xml clientXml, busXml.childrenNamed(XML_BUS_LCLIENT)) {
            bus->leftClients.append(clientXml.text());
        }
        foreach (Xml clientXml, busXml.childrenNamed(XML_BUS_RCLIENT)) {
            bus->rightClients.append(clientXml.text());
        }
        bus->ignoreMasterGain = Qstr2bool(busXml.childText(
                                            XML_BUS_IGNORE_GLOBAL_VOLUME));

        int id = busXml.childInt(XML_BUS_ID, audioBus_getUniqueId());
        if (mAudioBusMap.contains(id)) {
            print(QString("loadProject: Duplicate bus id detected: %1").arg(id));
        }
        this->mAudioBusMap.insert(id, bus);
    }
}

void Project::addBusesToProjectXml(Xml *projectXml) const
{
    Xml audioOutListXml(XML_BUSLIST);
    foreach (int id, audioBus_getAllBusIds()) {
        AudioPortPtr b = audioBus_getBus(id);
        Xml busXml(XML_BUS);
        busXml.addTextChild(XML_BUS_ID, n2s(id));
        busXml.addTextChild(XML_BUS_NAME, b->name);
        busXml.addTextChild(XML_BUS_LGAIN, n2s(b->leftGain));
        busXml.addTextChild(XML_BUS_RGAIN, n2s(b->rightGain));
        busXml.addTextChild(XML_BUS_IGNORE_GLOBAL_VOLUME,
                            bool2str(b->ignoreMasterGain));
        foreach (QString client, b->leftClients) {
            busXml.addTextChild(XML_BUS_LCLIENT, client);
        }
        foreach (QString client, b->rightClients) {
            busXml.addTextChild(XML_BUS_RCLIENT, client);
        }
        audioOutListXml.addChild(busXml);
    }
    projectXml->addChild(audioOutListXml);
}

int Project::midiInPort_getUniqueId()
{
    QList<int> l = mMidiInPortMap.keys();
    return getUniqueIdNotInList( l );
}

void Project::readMidiInPortsFromProjectXml(Xml projectXml)
{
    Xml midiInPortList = projectXml.child(XML_MIDI_IN_PORTLIST);
    foreach (Xml portXml, midiInPortList.childrenNamed(XML_MIDI_IN_PORT)) {

        MidiPortPtr port(new MidiPort());
        port->name = portXml.childText(XML_MIDI_IN_PORT_NAME);
        foreach (Xml clientXml, portXml.childrenNamed(XML_MIDI_IN_PORT_CLIENT)) {
            port->clients.append(clientXml.text());
        }
        port->filter.readFromXml(portXml.child(MidiFilter::XML_MIDIFILTER));
        PortScriptData s = readPortScript(portXml.child(XML_PORT_SCRIPT));
        port->script = s.content;
        port->scriptEnabled = s.enabled;
        port->passMidiThrough = s.passMidiThrough;

        int id = portXml.childInt(XML_MIDI_IN_PORT_ID,
                                  midiInPort_getUniqueId());
        if (mMidiInPortMap.contains(id)) {
            print(QString("loadProject: Duplicate midi in port id detected: %1")
                  .arg(id));
        }
        this->mMidiInPortMap.insert(id, port);
    }
}

void Project::addMidiInPortsToProjectXml(Xml *projectXml) const
{
    Xml midiInListXml(XML_MIDI_IN_PORTLIST);
    foreach (int id, midiInPort_getAllPortIds()) {
        MidiPortPtr p = midiInPort_getPort(id);
        Xml portXml(XML_MIDI_IN_PORT);
        portXml.addTextChild(XML_MIDI_IN_PORT_ID, n2s(id));
        portXml.addTextChild(XML_MIDI_IN_PORT_NAME, p->name);
        portXml.addChild(p->filter.toXml());
        foreach (QString client, p->clients) {
            portXml.addTextChild(XML_MIDI_IN_PORT_CLIENT, client);
        }
        portXml.addChild(portScriptToXml(p));
        midiInListXml.addChild(portXml);
    }
    projectXml->addChild(midiInListXml);
}

int Project::midiOutPort_getUniqueId()
{
    QList<int> l = mMidiOutPortMap.keys();
    return getUniqueIdNotInList( l );
}

void Project::readMidiOutPortsFromProjectXml(Xml projectXml)
{
    Xml midiOutPortList = projectXml.child(XML_MIDI_OUT_PORTLIST);
    foreach (Xml portXml, midiOutPortList.childrenNamed(XML_MIDI_OUT_PORT)) {

        MidiPortPtr port(new MidiPort());
        port->name = portXml.childText(XML_MIDI_OUT_PORT_NAME);
        foreach (Xml clientXml, portXml.childrenNamed(XML_MIDI_OUT_PORT_CLIENT)) {
            port->clients.append(clientXml.text());
        }

        int id = portXml.childInt(XML_MIDI_OUT_PORT_ID,
                                  midiOutPort_getUniqueId());
        if (mMidiOutPortMap.contains(id)) {
            print(QString("loadProject: Duplicate midi out port id detected: %1")
                  .arg(id));
        }
        this->mMidiOutPortMap.insert(id, port);
    }
}

void Project::addMidiOutPortsToProjectXml(Xml *projectXml) const
{
    Xml midiOutListXml(XML_MIDI_OUT_PORTLIST);
    foreach (int id, midiOutPort_getAllPortIds()) {
        MidiPortPtr p = midiOutPort_getPort(id);
        Xml portXml(XML_MIDI_OUT_PORT);
        portXml.addTextChild(XML_MIDI_OUT_PORT_ID, n2s(id));
        portXml.addTextChild(XML_MIDI_OUT_PORT_NAME, p->name);
        foreach (QString client, p->clients) {
            portXml.addTextChild(XML_MIDI_OUT_PORT_CLIENT, client);
        }
        midiOutListXml.addChild(portXml);
    }
    projectXml->addChild(midiOutListXml);
}

int Project::audioInPort_getUniqueId()
{
    QList<int> l = mAudioInPortMap.keys();
    return getUniqueIdNotInList( l );
}

void Project::readAudioInPortsFromProjectXml(Xml projectXml)
{
    Xml audioInPortList = projectXml.child(XML_AUDIOINLIST);
    foreach (Xml portXml, audioInPortList.childrenNamed(XML_AUDIOIN_PORT)) {

        AudioPortPtr port(new AudioPort());
        port->name = portXml.childText(XML_AUDIOIN_PORT_NAME);
        port->leftGain = portXml.childFloat(XML_AUDIOIN_PORT_LGAIN,
                                            port->leftGain);
        port->rightGain = portXml.childFloat(XML_AUDIOIN_PORT_RGAIN,
                                             port->rightGain);
        foreach (Xml clientXml, portXml.childrenNamed(XML_AUDIOIN_PORT_LCLIENT)) {
            port->leftClients.append(clientXml.text());
        }
        foreach (Xml clientXml, portXml.childrenNamed(XML_AUDIOIN_PORT_RCLIENT)) {
            port->rightClients.append(clientXml.text());
        }

        int id = portXml.childInt(XML_AUDIOIN_PORT_ID,
                                  audioInPort_getUniqueId());
        if (mAudioInPortMap.contains(id)) {
            print("loadProject: Duplicate audio in port id detected: "
                  + n2s(id));
        }
        this->mAudioInPortMap.insert(id, port);
    }
}

void Project::addAudioInPortsToProjectXml(Xml *projectXml) const
{
    Xml audioInListXml(XML_AUDIOINLIST);
    foreach (int id, audioInPort_getAllPortIds()) {
        AudioPortPtr p = audioInPort_getPort(id);
        Xml portXml(XML_AUDIOIN_PORT);
        portXml.addTextChild(XML_AUDIOIN_PORT_ID, n2s(id));
        portXml.addTextChild(XML_AUDIOIN_PORT_NAME, p->name);
        portXml.addTextChild(XML_AUDIOIN_PORT_LGAIN, n2s(p->leftGain));
        portXml.addTextChild(XML_AUDIOIN_PORT_RGAIN, n2s(p->rightGain));
        foreach (QString client, p->leftClients) {
            portXml.addTextChild(XML_AUDIOIN_PORT_LCLIENT, client);
        }
        foreach (QString client, p->rightClients) {
            portXml.addTextChild(XML_AUDIOIN_PORT_RCLIENT, client);
        }
        audioInListXml.addChild(portXml);
    }
    projectXml->addChild(audioInListXml);
}

int Project::getUniqueIdNotInList(QList<int> ids)
{
    /* Given the list of ids, find the highest id and add one. */
    int id = -1;
    foreach (int existingId, ids) {
        if (existingId > id) { id = existingId; }
    }
    return id + 1;
}

Xml Project::portScriptToXml(MidiPortPtr port) const
{
    Xml xml(XML_PORT_SCRIPT);

    xml.addTextChild(XML_PORT_SCRIPT_CONTENT, port->script);
    xml.addTextChild(XML_PORT_SCRIPT_ENABLED,
                     QVariant(port->scriptEnabled).toString());
    xml.addTextChild(XML_PORT_SCRIPT_PASS_MIDI_THROUGH,
                     QVariant(port->passMidiThrough).toString());

    return xml;
}

Project::PortScriptData Project::readPortScript(Xml xml)
{
    PortScriptData ret;

    ret.content = xml.childText(XML_PORT_SCRIPT_CONTENT);
    ret.enabled = xml.childBool(XML_PORT_SCRIPT_ENABLED, ret.enabled);
    ret.passMidiThrough = xml.childBool(XML_PORT_SCRIPT_PASS_MIDI_THROUGH,
                                        ret.passMidiThrough);

    return ret;
}

int Project::getUniqueExternalAppId()
{
    return getUniqueIdNotInList( mExternalApps.keys() );
}

void Project::clearExternalApps()
{
    foreach (int id, mExternalApps.keys()) {
        removeExternalApp(id);
    }
}

void Project::addExternalAppsToXml(Xml *xml) const
{
    // Write new-style list
    Xml appListXml(XML_EXT_APP_LIST);
    foreach (const ExternalApp& app, mExternalApps.values()) {
        Xml appXml(XML_EXT_APP);
        appXml.addTextChild(XML_EXT_APP_NAME, app.friendlyName);
        appXml.addTextChild(XML_EXT_APP_CMD, app.command);
        appXml.addTextChild(XML_EXT_APP_RUNATSTARTUP, bool2str(app.runAtStartup));
        appXml.addTextChild(XML_EXT_APP_RESTART, bool2str(app.autoRestart));
        appXml.addTextChild(XML_EXT_APP_WARN_BEFORE_CLOSING,
                            bool2str(app.warnBeforeClosing));
        appXml.addTextChild(XML_EXT_APP_START_DETACHED,
                            bool2str(app.startDetached));
        appListXml.addChild(appXml);
    }
    xml->addChild(appListXml);
}

void Project::readExternalApps(Xml xml)
{
    foreach (Xml appXml, xml.childrenNamed(XML_EXT_APP)) {
        ExternalApp app;
        app.friendlyName = appXml.childText(XML_EXT_APP_NAME);
        app.command = appXml.childText(XML_EXT_APP_CMD);
        appXml.setBoolFromChild(XML_EXT_APP_RUNATSTARTUP,
                                &app.runAtStartup);
        appXml.setBoolFromChild(XML_EXT_APP_RESTART,
                                &app.autoRestart);
        appXml.setBoolFromChild(XML_EXT_APP_WARN_BEFORE_CLOSING,
                                &app.warnBeforeClosing);
        appXml.setBoolFromChild(XML_EXT_APP_START_DETACHED,
                                &app.startDetached);
        addExternalApp(app);
    }
}

void Project::readTriggersFromProjectXml(Xml projectXml)
{
    Xml triggerList = projectXml.child(XML_TRIGGERLIST);
    mProgramChangeSwitchPatches = Qstr2bool(
                triggerList.childText(XML_PROG_CHANGE_SWITCH_PATCHES));
    foreach (Xml triggerXml, triggerList.childrenNamed(XML_TRIGGER)) {
        Trigger trig;
        trig.actionText = triggerXml.childText(XML_TRIGGER_ACTIONTEXT);
        trig.type = triggerXml.childInt(XML_TRIGGER_TYPE,
                                        trig.type);
        trig.channel = triggerXml.childInt(XML_TRIGGER_CHAN, trig.channel);
        trig.data1 = triggerXml.childInt(XML_TRIGGER_DATA1, trig.data1);
        trig.bankMSB = triggerXml.childInt(XML_TRIGGER_BANKMSB,
                                           trig.bankMSB);
        trig.bankLSB = triggerXml.childInt(XML_TRIGGER_BANKLSB,
                                           trig.bankLSB);
        this->addAndReplaceTrigger(trig);
    }
}

void Project::addTriggersToProjectXml(Xml *projectXml) const
{
    Xml triggerListXml(XML_TRIGGERLIST);
    triggerListXml.addTextChild(XML_PROG_CHANGE_SWITCH_PATCHES,
                             bool2str(mProgramChangeSwitchPatches));
    foreach (Trigger trigger, mTriggerHash.values()) {
        Xml triggerXml(XML_TRIGGER);
        triggerXml.addTextChild(XML_TRIGGER_ACTIONTEXT, trigger.actionText);
        triggerXml.addTextChild(XML_TRIGGER_TYPE, n2s(trigger.type));
        triggerXml.addTextChild(XML_TRIGGER_CHAN, n2s(trigger.channel));
        triggerXml.addTextChild(XML_TRIGGER_DATA1, n2s(trigger.data1));
        triggerXml.addTextChild(XML_TRIGGER_BANKMSB, n2s(trigger.bankMSB));
        triggerXml.addTextChild(XML_TRIGGER_BANKLSB, n2s(trigger.bankLSB));
        triggerListXml.addChild(triggerXml);
    }
    projectXml->addChild(triggerListXml);
}

void Project::readJackMidiConList(Xml xml, bool makeNotBreak)
{
    // NB: Do not clear the jackMidiConList, as this function is called
    // separately for make and break connection lists during project load.
    // Make and break lists are stored separately in the project  for backwards
    // compatibility with older versions which only saved make connections.

    foreach (Xml conXml, xml.childrenNamed(XML_OTHERJACKCON)) {

        QString srcPort = conXml.childText(XML_OTHERJACKCON_SRC);
        QString destPort = conXml.childText(XML_OTHERJACKCON_DEST);

        if (makeNotBreak) {
            addJackMidiConMake(srcPort, destPort);
        } else {
            addJackMidiConBreak(srcPort, destPort);
        }

    }
}

void Project::readJackAudioConList(Xml xml, bool makeNotBreak)
{
    // NB: Do not clear the jackAudioConList, as this function is called
    // separately for make and break connection lists during project load.
    // Make and break lists are stored separately in the projedct for backwards
    // compatibility with older versions which only saved make connections.

    foreach (Xml conXml, xml.childrenNamed(XML_OTHERJACKCON)) {

        QString srcPort = conXml.childText(XML_OTHERJACKCON_SRC);
        QString destPort = conXml.childText(XML_OTHERJACKCON_DEST);

        if (makeNotBreak) {
            addJackAudioConMake(srcPort, destPort);
        } else {
            addJackAudioConBreak(srcPort, destPort);
        }

    }
}

/* Adds bus and returns unique bus id. */
int Project::audioBus_add(QString busName)
{
    AudioPortPtr bus(new AudioPort());
    bus->name = busName;

    int busId = audioBus_getUniqueId();
    mAudioBusMap.insert(busId, bus);

    setModified(true);

    return busId;
}

void Project::audioBus_remove(int busId)
{
    KONFYT_ASSERT_RETURN(mAudioBusMap.contains(busId));

    mAudioBusMap.remove(busId);
    setModified(true);
}

int Project::audioBus_count()
{
    return mAudioBusMap.count();
}

bool Project::audioBus_exists(int busId)
{
    return mAudioBusMap.contains(busId);
}

Project::AudioPortPtr Project::audioBus_getBus(int busId) const
{
    KONFYT_ASSERT(mAudioBusMap.contains(busId));

    return mAudioBusMap.value(busId);
}

/* Returns the bus id corresponding to the specified bus pointer, or -1 if the
 * specified bus does not exist. */
int Project::audioBus_idFromPtr(AudioPortPtr p)
{
    return mAudioBusMap.key(p, -1);
}

/* Gets the first bus Id that is not skipId. */
int Project::audioBus_getFirstBusId(int skipId)
{
    int ret = -1;
    QList<int> l = mAudioBusMap.keys();

    KONFYT_ASSERT(l.count());

    for (int i=0; i<l.count(); i++) {
        if (l[i] != skipId) {
            ret = l[i];
            break;
        }
    }

    return ret;
}

QList<int> Project::audioBus_getAllBusIds() const
{
    return mAudioBusMap.keys();
}

QList<Project::AudioPortPtr> Project::audioBus_getAllBuses()
{
    return mAudioBusMap.values();
}

void Project::audioBus_setName(int busId, QString name)
{
    KONFYT_ASSERT_RETURN(audioBus_exists(busId));
    mAudioBusMap[busId]->name = name;
    setModified(true);
    emit audioBusNameChanged(busId);
}

void Project::audioBus_replace(int busId, AudioPortPtr newBus)
{
    audioBus_replace_noModify(busId, newBus);
    setModified(true);
}

/* Do not change the project's modified state. */
void Project::audioBus_replace_noModify(int busId, AudioPortPtr newBus)
{
    KONFYT_ASSERT_RETURN(mAudioBusMap.contains(busId));

    mAudioBusMap.insert(busId, newBus);
}

void Project::audioBus_addClient(int busId, PortLeftRight leftRight, QString client)
{
    KONFYT_ASSERT_RETURN(mAudioBusMap.contains(busId));

    AudioPortPtr b = mAudioBusMap.value(busId);
    if (!b) { return; }

    if (leftRight == Project::LeftPort) {
        if (!b->leftClients.contains(client)) {
            b->leftClients.append(client);
        }
    } else {
        if (!b->rightClients.contains(client)) {
            b->rightClients.append(client);
        }
    }

    setModified(true);
}

void Project::audioBus_removeClient(int busId, PortLeftRight leftRight, QString client)
{
    KONFYT_ASSERT_RETURN(mAudioBusMap.contains(busId));

    AudioPortPtr b = mAudioBusMap.value(busId);
    if (!b) { return; }

    if (leftRight == Project::LeftPort) {
        b->leftClients.removeAll(client);
    } else {
        b->rightClients.removeAll(client);
    }
    mAudioBusMap.insert(busId, b);
    setModified(true);
}

int Project::addExternalApp(ExternalApp app)
{
    int id = getUniqueExternalAppId();
    mExternalApps.insert(id, app);

    setModified(true);

    emit externalAppAdded(id);
    return id;
}

QList<int> Project::audioInPort_getAllPortIds() const
{
    return mAudioInPortMap.keys();
}

QList<Project::AudioPortPtr> Project::audioInPort_getAllPorts()
{
    return mAudioInPortMap.values();
}

int Project::audioInPort_add(QString portName)
{
    AudioPortPtr port(new AudioPort());
    port->name = portName;

    int portId = audioInPort_getUniqueId();
    mAudioInPortMap.insert(portId, port);

    setModified(true);

    return portId;
}

void Project::audioInPort_remove(int portId)
{
    KONFYT_ASSERT_RETURN(mAudioInPortMap.contains(portId));

    mAudioInPortMap.remove(portId);
    setModified(true);
}

int Project::audioInPort_count()
{
    return mAudioInPortMap.count();
}

void Project::audioInPort_setName(int portId, QString name)
{
    AudioPortPtr p = mAudioInPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->name = name;

    setModified(true);
    emit audioInPortNameChanged(portId);
}

void Project::audioInPort_setJackPorts(int portId, KfJackAudioPort *left, KfJackAudioPort *right)
{
    AudioPortPtr p = mAudioInPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->leftJackPort = left;
    p->rightJackPort = right;
    // Do not set modified
}

bool Project::audioInPort_exists(int portId) const
{
    return mAudioInPortMap.contains(portId);
}

/* Returns the audio port corresponding to the specified portId, or a null
 * pointer if a port doesn't exist with the specified portId. */
Project::AudioPortPtr Project::audioInPort_getPort(int portId) const
{
    return mAudioInPortMap.value(portId);
}

/* Returns the port id corresponding to the specified port pointer, or -1 if the
 * specified port does not exist. */
int Project::audioInPort_idFromPtr(AudioPortPtr p)
{
    return mAudioInPortMap.key(p, -1);
}

void Project::audioInPort_addClient(int portId, PortLeftRight leftRight, QString client)
{
    AudioPortPtr p = mAudioInPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    if (leftRight == Project::LeftPort) {
        if (!p->leftClients.contains(client)) {
            p->leftClients.append(client);
        }
    } else {
        if (!p->rightClients.contains(client)) {
            p->rightClients.append(client);
        }
    }

    setModified(true);
}

void Project::audioInPort_removeClient(int portId, PortLeftRight leftRight, QString client)
{
    AudioPortPtr p = mAudioInPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    if (leftRight == Project::LeftPort) {
        p->leftClients.removeAll(client);
    } else {
        p->rightClients.removeAll(client);
    }

    setModified(true);
}

void Project::midiOutPort_removePort(int portId)
{
    KONFYT_ASSERT_RETURN(mMidiOutPortMap.contains(portId));

    mMidiOutPortMap.remove(portId);
    setModified(true);
}

int Project::midiOutPort_count()
{
    return mMidiOutPortMap.count();
}

void Project::midiOutPort_setName(int portId, QString name)
{
    MidiPortPtr p = mMidiOutPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->name = name;

    setModified(true);
    emit midiOutPortNameChanged(portId);
}

void Project::midiOutPort_setJackPort(int portId, KfJackMidiPort *jackport)
{
    MidiPortPtr p = mMidiOutPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->jackPort = jackport;
    // Do not set modified
}

bool Project::midiOutPort_exists(int portId) const
{
    return mMidiOutPortMap.contains(portId);
}

/* Returns a port pointer corresponding to the specified portId, or a null
 * pointer if a port with this portId doesn't exist. */
Project::MidiPortPtr Project::midiOutPort_getPort(int portId) const
{
    return mMidiOutPortMap.value(portId);
}

/* Returns the port id corresponding to the specified port pointer, or -1 if the
 * specified port does not exist. */
int Project::midiOutPort_idFromPtr(MidiPortPtr p)
{
    return mMidiOutPortMap.key(p, -1);
}

void Project::midiOutPort_addClient(int portId, QString client)
{
    MidiPortPtr p = mMidiOutPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->clients.append(client);

    setModified(true);
}

void Project::midiOutPort_removeClient(int portId, QString client)
{
    MidiPortPtr p = mMidiOutPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->clients.removeAll(client);

    setModified(true);
}

QList<int> Project::midiOutPort_getAllPortIds() const
{
    return mMidiOutPortMap.keys();
}

QList<Project::MidiPortPtr> Project::midiOutPort_getAllPorts()
{
    return mMidiOutPortMap.values();
}

QStringList Project::midiOutPort_getClients(int portId)
{
    QStringList ret;

    MidiPortPtr p = mMidiOutPortMap.value(portId);
    KONFYT_ASSERT(!p.isNull());
    if (p) {
        ret = p->clients;
    }

    return ret;
}

void Project::removeExternalApp(int id)
{
    if (mExternalApps.contains(id)) {
        mExternalApps.remove(id);
        setModified(true);
        emit externalAppRemoved(id);
    } else {
        print("ERROR: removeExternalApp: INVALID ID " + n2s(id));
    }
}

Project::ExternalApp Project::getExternalApp(int id)
{
    return mExternalApps.value(id);
}

QList<int> Project::getExternalAppIds()
{
    return mExternalApps.keys();
}

bool Project::hasExternalAppWithId(int id)
{
    return mExternalApps.keys().contains(id);
}

void Project::modifyExternalApp(int id, ExternalApp app)
{
    KONFYT_ASSERT_RETURN(mExternalApps.contains(id));

    mExternalApps.insert(id, app);
    setModified(true);
    emit externalAppModified(id);
}

void Project::addAndReplaceTrigger(Trigger newTrigger)
{
    // Remove any action that has the same trigger
    int trigint = newTrigger.toInt();
    QList<QString> l = mTriggerHash.keys();
    for (int i=0; i<l.count(); i++) {
        if (mTriggerHash.value(l[i]).toInt() == trigint) {
            mTriggerHash.remove(l[i]);
        }
    }

    mTriggerHash.insert(newTrigger.actionText, newTrigger);
    setModified(true);
}

void Project::removeTrigger(QString actionText)
{
    if (mTriggerHash.contains(actionText)) {
        mTriggerHash.remove(actionText);
        setModified(true);
    }
}

QList<Project::Trigger> Project::getTriggerList()
{
    return mTriggerHash.values();
}

bool Project::isProgramChangeSwitchPatches()
{
    return mProgramChangeSwitchPatches;
}

void Project::setProgramChangeSwitchPatches(bool value)
{
    mProgramChangeSwitchPatches = value;
    setModified(true);
}

KonfytJackConPair Project::addJackMidiConMake(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    a.makeNotBreak = true;
    this->mJackMidiConList.append(a);
    setModified(true);
    return a;
}

KonfytJackConPair Project::addJackMidiConBreak(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    a.makeNotBreak = false;
    this->mJackMidiConList.append(a);
    setModified(true);
    return a;
}

QList<KonfytJackConPair> Project::getJackMidiConList()
{
    return this->mJackMidiConList;
}

KonfytJackConPair Project::removeJackMidiCon(int i)
{
    KONFYT_ASSERT_RETURN_VAL( (i >= 0) && (i < mJackMidiConList.count()),
                              KonfytJackConPair() );

    KonfytJackConPair p = mJackMidiConList[i];
    mJackMidiConList.removeAt(i);
    setModified(true);
    return p;
}

KonfytJackConPair Project::addJackAudioConMake(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    a.makeNotBreak = true;
    mJackAudioConList.append(a);
    setModified(true);
    return a;
}

KonfytJackConPair Project::addJackAudioConBreak(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    a.makeNotBreak = false;
    mJackAudioConList.append(a);
    setModified(true);
    return a;
}

QList<KonfytJackConPair> Project::getJackAudioConList()
{
    return mJackAudioConList;
}

KonfytJackConPair Project::removeJackAudioCon(int i)
{
    KONFYT_ASSERT_RETURN_VAL( (i >= 0) && (i < mJackAudioConList.count()),
                              KonfytJackConPair() );

    KonfytJackConPair p = mJackAudioConList[i];
    mJackAudioConList.removeAt(i);
    setModified(true);
    return p;
}

QString Project::filenameFromProjectName()
{
    QString basename = sanitiseFilename(mProjectName.trimmed());
    if (basename.isEmpty()) {
        basename = "project";
    }
    return basename + PROJECT_FILE_EXTENSION_WITHDOT;
}

/* Copy project files of the specified project filename and directory to the
 * backups directory under a new subdirectory, named to end with tag. */
void Project::backupProject(QString dirpath, QString projectFilename,
                                  QString tag)
{
    // Check if project file exists in specified dir
    QFileInfo fi(QString("%1/%2").arg(dirpath, projectFilename));
    if (!fi.exists()) {
        print("backupProject: No existing project file found with name "
              + projectFilename + ", not doing backup.");
        return;
    }

    // Create base backup dir if it doesn't exist
    QString baseBackupPath = QString("%1/%2").arg(dirpath, PROJECT_BACKUP_DIR);

    QDir baseBackupDir(baseBackupPath);
    if (!baseBackupDir.exists()) {
        if (baseBackupDir.mkdir(baseBackupPath)) {
            print("backupProject: Created base backup directory: " + baseBackupPath);
        } else {
            print("ERROR: backupProject: Could not create base backup directory: "
                  + baseBackupPath);
            return;
        }
    }

    // Create unique backup dir
    QString backupDirName = QString("%1_backup_%2_%3")
            .arg(QDir(dirpath).dirName())
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"))
            .arg(tag);
    QString backupDirPath = getUniquePath(baseBackupPath, backupDirName, "");

    if (QDir().mkdir(backupDirPath)) {
        print("backupProject: Created backup directory: " + backupDirPath);
    } else {
        print("ERROR: backupProject: Could not create backup directory: "
              + backupDirPath);
        return;
    }

    // Copy project files to backup location
    QStringList relPathsToCopy;
    // Project file
    relPathsToCopy.append(projectFilename);
    // Patches
    QString srcPatchPath = QString("%1/%2").arg(dirpath, PROJECT_PATCH_DIR);
    foreach (QFileInfo fi, QDir(srcPatchPath).entryInfoList()) {
        if (fi.isFile()) {
            relPathsToCopy.append(QString("%1/%2").arg(PROJECT_PATCH_DIR, fi.fileName()));
        }
    }

    // Copy paths

    // Create patch dir
    if (!QDir(backupDirPath).mkdir(PROJECT_PATCH_DIR)) {
        print("ERROR: backupProject: Could not create patches dir.");
    }
    // Copy each path
    foreach (QString relPath, relPathsToCopy) {
        QString src = QString("%1/%2").arg(dirpath, relPath);
        QString dest = QString("%1/%2").arg(backupDirPath, relPath);
        QFile file(src);
        if (!file.copy(src, dest)) {
            print(QString("ERROR: backupProject: Could not copy file %1 to %2. Error: %3")
                    .arg(src, dest, file.errorString()));
        }
    }

    print("Backed up project to: " + backupDirPath);
}

Xml Project::toXml() const
{
    Xml projectXml(XML_PROJECT);

    projectXml.setAttribute(XML_PROJECT_NAME, this->mProjectName);
    projectXml.setAttribute(XML_KONFYT_VERSION, APP_VERSION);

    // Misc project settings
    projectXml.addTextChild(XML_PATCH_LIST_NUMBERS, bool2str(mShowPatchListNumbers));
    projectXml.addTextChild(XML_PATCH_LIST_NOTES, bool2str(mShowPatchListNotes));
    projectXml.addTextChild(XML_MIDI_PICKUP_RANGE, n2s(mMidiPickupRange));
    projectXml.addTextChild(XML_RESET_OPTION, konfytResetToString(mResetOption));

    // Patches
    addPatchesToProjectXml(&projectXml);

    // MIDI input ports
    addMidiInPortsToProjectXml(&projectXml);

    // MIDI output ports
    addMidiOutPortsToProjectXml(&projectXml);

    // Audio output ports (buses)
    addBusesToProjectXml(&projectXml);

    // Audio input ports
    addAudioInPortsToProjectXml(&projectXml);

    // External applications
    addExternalAppsToXml(&projectXml);

    // Triggers
    addTriggersToProjectXml(&projectXml);

    // Write other JACK MIDI make-connections list
    // Make and break lists are stored separately for backwards compatibility
    // with older versions which only saved make connections.
    Xml midiMakeConListXml(XML_OTHERJACK_MIDI_CON_LIST);
    foreach (const KonfytJackConPair& p, mJackMidiConList) {
        if (!p.makeNotBreak) { continue; }
        Xml conXml(XML_OTHERJACKCON);
        conXml.addTextChild(XML_OTHERJACKCON_SRC, p.srcPort);
        conXml.addTextChild(XML_OTHERJACKCON_DEST, p.destPort);
        midiMakeConListXml.addChild(conXml);
    }
    projectXml.addChild(midiMakeConListXml);

    // Write other JACK MIDI break-connections list
    Xml midiBreakConListXml(XML_OTHERJACK_MIDI_CON_BREAK_LIST);
    foreach (const KonfytJackConPair& p, mJackMidiConList) {
        if (p.makeNotBreak) { continue; }
        Xml conXml(XML_OTHERJACKCON);
        conXml.addTextChild(XML_OTHERJACKCON_SRC, p.srcPort);
        conXml.addTextChild(XML_OTHERJACKCON_DEST, p.destPort);
        midiBreakConListXml.addChild(conXml);
    }
    projectXml.addChild(midiBreakConListXml);

    // Write other JACK Audio make-connections list
    // Make and break lists are stored separately for backwards compatibility
    // with older versions which only saved make connections.
    Xml audioMakeConListXml(XML_OTHERJACK_AUDIO_CON_LIST);
    foreach (const KonfytJackConPair& p, mJackAudioConList) {
        if (!p.makeNotBreak) { continue; }
        Xml conXml(XML_OTHERJACKCON);
        conXml.addTextChild(XML_OTHERJACKCON_SRC, p.srcPort);
        conXml.addTextChild(XML_OTHERJACKCON_DEST, p.destPort);
        audioMakeConListXml.addChild(conXml);
    }
    projectXml.addChild(audioMakeConListXml);

    // Write other JACK Audio break-connections list
    Xml audioBreakConListXml(XML_OTHERJACK_AUDIO_CON_BREAK_LIST);
    foreach (const KonfytJackConPair& p, mJackAudioConList) {
        if (p.makeNotBreak) { continue; }
        Xml conXml(XML_OTHERJACKCON);
        conXml.addTextChild(XML_OTHERJACKCON_SRC, p.srcPort);
        conXml.addTextChild(XML_OTHERJACKCON_DEST, p.destPort);
        audioBreakConListXml.addChild(conXml);
    }
    projectXml.addChild(audioBreakConListXml);

    return projectXml;
}

void Project::readPatchesFromProjectXml(Xml projectXml)
{
    foreach (Xml patchXml, projectXml.childrenNamed(XML_PATCH)) {

        QString patchPathRel = patchXml.childText(XML_PATCH_FILENAME);

        // Add new patch
        PatchPtr patch(new Patch());
        QString errors;
        QString patchPathAbs = mProjectDirpath + "/" + patchPathRel;
        print("loadProject: Loading patch " + patchPathRel);
        Result r = patch->loadPatchFromFile(patchPathAbs);
        if (r.ok) {
            this->addPatch(patch);
        } else {
            print(QString("loadProject: Error loading patch. File: %1. Error: %2")
                  .arg(r.errorString).arg(patchPathAbs));
        }
        if (!errors.isEmpty()) {
            print("Load errors for patch " + patchPathAbs + ":\n" + errors);
        }

    }
}

void Project::addPatchesToProjectXml(Xml *projectXml) const
{
    for (int i = 0; i < mPatches.count(); i++) {

        Xml patchXml(XML_PATCH);

        PatchPtr patch = mPatches.at(i);
        QString patchPathRel = QString("%1/%2_%3.%4")
                                    .arg(PROJECT_PATCH_DIR)
                                    .arg(i)
                                    .arg(sanitiseFilename(patch->name()))
                                    .arg(Patch::PATCH_FILE_EXTENSION_NODOT);
        patchXml.addTextChild(XML_PATCH_FILENAME, patchPathRel);

        // Save the patch file in patches subdirectory
        QString patchPathAbs = mProjectDirpath + "/" + patchPathRel;
        Result r = patch->savePatchToFile(patchPathAbs);
        if (r.ok) {
            print("saveProjectAs: Saved patch: " + patchPathRel);
        } else {
            print(QString("ERROR: saveProjectAs: Failed to save patch. File: %1. Error: %2")
                  .arg(patchPathAbs).arg(r.errorString));
            // TODO: BUBBLE ERRORS UP SO A MSGBOX CAN BE SHOWN TO USER THAT ONE OR MORE ERRORS HAVE OCCURRED.
        }

        projectXml->addChild(patchXml);
    }
}

Project::ExternalApp::ExternalApp(QString name, QString cmd) :
    friendlyName(name), command(cmd)
{

}

QString Project::ExternalApp::displayName()
{
    QString text = friendlyName;
    if (text.trimmed().isEmpty()) {
        text = command;
    }
    if (text.trimmed().isEmpty()) {
        text = "(no name)";
    }
    return text;
}

int Project::Trigger::toInt() const
{
    return hashMidiEventToInt(type, channel, data1, bankMSB, bankLSB);
}

QString Project::Trigger::toString()
{
    return midiEventToString(type, channel, data1, bankMSB, bankLSB);
}
