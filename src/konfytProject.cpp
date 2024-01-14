/******************************************************************************
 *
 * Copyright 2023 Gideon van der Kolf
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

#include <iostream>

// ============================================================================

const QString KonfytProject::NEW_PROJECT_DEFAULT_NAME = "New Project";
const QString KonfytProject::PROJECT_FILENAME_EXTENSION = ".konfytproject";
const QString KonfytProject::PROJECT_PATCH_DIR = "patches";
const QString KonfytProject::PROJECT_BACKUP_DIR = "backup";

// ============================================================================

KonfytProject::KonfytProject(QObject *parent) :
    QObject(parent)
{
    // Project has to have a minimum 1 bus
    // Ports will be assigned later when loading project
    this->audioBus_add("Master Bus");
    // Add at least 1 MIDI input port also
    this->midiInPort_addPort("MIDI In");
}

bool KonfytProject::saveProject()
{
    return saveProjectAs(mProjectDirpath);
}

/* Save project XML file, as well as the accompanying files, to the specified
 * directory.
 * If the project already has a filename (i.e. set when loaded) that filename
 * will be used for the project XML file. If the filename is empty, a new
 * filename will be derived from the project name. */
bool KonfytProject::saveProjectAs(QString dirpath)
{
    // Abort if directory does not exist
    QDir dir(dirpath);
    if (!dir.exists()) {
        print("saveProjectAs: Directory does not exist.");
        return false;
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
            return false;
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
                          "\"%1\" to \"%2\".")
                  .arg(mProjectFilename, newFilename));
        }
    }

    // Open project file for writing
    QString filepath = dirpath + "/" + mProjectFilename;
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        print("saveProjectAs: Could not open file for writing: " + filepath);
        return false;
    }

    print("saveProjectAs: Project directory: " + mProjectDirpath);
    print("saveProjectAs: Project filename: " + mProjectFilename);

    QXmlStreamWriter stream(&file);
    writeToXmlStream(&stream);
    file.close();

    setModified(false);

    // Create another backup of the saved files. This will ensure that if this
    // project is now opened with an earlier version of Konfyt which does not
    // create backups and does not retain data related to newer features, that
    // this version will still be backed up.
    backupProject(dirpath, mProjectFilename, "b_postsave");

    return true;
}

/* Load project xml file (containing list of all the patches) and load all the
 * patch files from the same directory. */
bool KonfytProject::loadProject(QString filepath)
{
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        print("loadProject: Could not open file for reading.");
        return false;
    }

    QFileInfo fi(file);
    mProjectFilename = fi.fileName();
    mProjectDirpath = fi.dir().path();

    print("loadProject: Loading project file: " + mProjectFilename);
    print("loadProject: In directory: " +mProjectDirpath);

    QXmlStreamReader r(&file);
    readFromXmlStream(&r);

    file.close();

    postExternalAppsRead(); // Commit loaded list. Used for backwards compatibility.

    // Create an audio output bus if there are none, so there is at least one.
    if (audioBus_count() == 0) {
        audioBus_add("Master Bus");
    }

    // Create a MIDI input port if there are none, so there is at least one.
    if (midiInPort_count() == 0) {
        midiInPort_addPort("MIDI In");
    }

    setModified(false);

    return true;
}

bool KonfytProject::getShowPatchListNumbers()
{
    return patchListNumbers;
}

void KonfytProject::setShowPatchListNumbers(bool show)
{
    patchListNumbers = show;
    setModified(true);
}

bool KonfytProject::getShowPatchListNotes()
{
    return patchListNotes;
}

void KonfytProject::setShowPatchListNotes(bool show)
{
    patchListNotes = show;
    setModified(true);
}

void KonfytProject::setMidiPickupRange(int range)
{
    if (midiPickupRange != range) {
        midiPickupRange = range;
        setModified(true);
        emit midiPickupRangeChanged(range);
    }
}

int KonfytProject::getMidiPickupRange()
{
    return midiPickupRange;
}

void KonfytProject::addPatch(KonfytPatch *newPatch)
{
    patchList.append(newPatch);
    setModified(true);
    patchURIsNeedUpdating();
}

void KonfytProject::insertPatch(KonfytPatch *newPatch, int index)
{
    patchList.insert(index, newPatch);
    setModified(true);
    patchURIsNeedUpdating();
}

/* Removes the patch from project and returns the pointer.
 * Note that the pointer has not been freed.
 * Returns nullptr if index out of bounds. */
KonfytPatch *KonfytProject::removePatch(int i)
{
    if ( (i>=0) && (i<patchList.count())) {
        KonfytPatch* patch = patchList[i];
        patchList.removeAt(i);
        setModified(true);
        patchURIsNeedUpdating();
        return patch;
    } else {
        return nullptr;
    }
}

void KonfytProject::movePatch(int indexFrom, int indexTo)
{
    KONFYT_ASSERT_RETURN( (indexFrom >= 0) && (indexFrom < patchList.count()) );
    KONFYT_ASSERT_RETURN( (indexTo >= 0) && (indexTo < patchList.count()) );

    patchList.move(indexFrom, indexTo);
    setModified(true);
    patchURIsNeedUpdating();
}

KonfytPatch *KonfytProject::getPatch(int i)
{
    if ( (i>=0) && (i<patchList.count())) {
        return patchList.at(i);
    } else {
        return nullptr;
    }
}

/* Returns the index of the specified patch and -1 if invalid. */
int KonfytProject::getPatchIndex(KonfytPatch *patch)
{
    return patchList.indexOf(patch);
}

QList<KonfytPatch *> KonfytProject::getPatchList()
{
    return patchList;
}

int KonfytProject::getNumPatches()
{
    return patchList.count();
}

QString KonfytProject::getDirPath()
{
    return mProjectDirpath;
}

void KonfytProject::setDirPath(QString path)
{
    mProjectDirpath = path;
    setModified(true);
}

QString KonfytProject::getProjectName()
{
    return mProjectName;
}

void KonfytProject::setProjectName(QString newName)
{
    mProjectName = newName;
    emit projectNameChanged();
    setModified(true);
}

QString KonfytProject::getProjectFilePath()
{
    QString ret = mProjectDirpath;
    if (!ret.isEmpty()) { ret += "/"; }
    ret += mProjectFilename;
    return ret;
}

QString KonfytProject::getProjectFileName()
{
    return mProjectFilename;
}

void KonfytProject::setProjectFileName(QString newName)
{
    mProjectFilename = newName;
}

QList<int> KonfytProject::midiInPort_getAllPortIds()
{
    return midiInPortMap.keys();
}

QList<PrjMidiPortPtr> KonfytProject::midiInPort_getAllPorts()
{
    return midiInPortMap.values();
}

int KonfytProject::midiInPort_addPort(QString portName)
{
    PrjMidiPortPtr p(new PrjMidiPort());

    p->portName = portName;

    int portId = midiInPort_getUniqueId();
    midiInPortMap.insert(portId, p);

    setModified(true);

    return portId;
}

void KonfytProject::midiInPort_removePort(int portId)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    midiInPortMap.remove(portId);
    setModified(true);
}

bool KonfytProject::midiInPort_exists(int portId)
{
    return midiInPortMap.contains(portId);
}

PrjMidiPortPtr KonfytProject::midiInPort_getPort(int portId)
{
    KONFYT_ASSERT_RETURN_VAL(midiInPort_exists(portId), PrjMidiPortPtr());

    return midiInPortMap.value(portId);
}

/* Returns the port id corresponding to the specified port pointer, or -1 if the
 * specified port does not exist. */
int KonfytProject::midiInPort_idFromPtr(PrjMidiPortPtr p)
{
    return midiInPortMap.key(p, -1);
}

int KonfytProject::midiInPort_getPortIdWithJackId(KfJackMidiPort *jackPort)
{
    int ret = -1;

    QList<int> ids = midiInPortMap.keys();
    foreach (int id, ids) {
        PrjMidiPortPtr p = midiInPortMap[id];
        if (p->jackPort == jackPort) {
            ret = id;
            break;
        }
    }

    return ret;
}

/* Gets the first MIDI Input Port Id that is not skipId. */
int KonfytProject::midiInPort_getFirstPortId(int skipId)
{
    int ret = -1;
    QList<int> l = midiInPortMap.keys();
    KONFYT_ASSERT_RETURN_VAL(l.count(), ret);

    for (int i=0; i < l.count(); i++) {
        if (l[i] != skipId) {
            ret = l[i];
            break;
        }
    }

    return ret;
}

int KonfytProject::midiInPort_count()
{
    return midiInPortMap.count();
}

void KonfytProject::midiInPort_setName(int portId, QString name)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    midiInPortMap[portId]->portName = name;
    setModified(true);
    emit midiInPortNameChanged(portId);
}

void KonfytProject::midiInPort_setJackPort(int portId, KfJackMidiPort *jackport)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    midiInPortMap[portId]->jackPort = jackport;
    // Do not set the project modified
}

QStringList KonfytProject::midiInPort_getClients(int portId)
{
    QStringList ret;
    KONFYT_ASSERT_RETURN_VAL(midiInPort_exists(portId), ret);

    ret = midiInPortMap.value(portId)->clients;

    return ret;
}

void KonfytProject::midiInPort_addClient(int portId, QString client)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    PrjMidiPortPtr p = midiInPortMap.value(portId);
    if (p) {
        p->clients.append(client);
        setModified(true);
    }
}

void KonfytProject::midiInPort_removeClient(int portId, QString client)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    PrjMidiPortPtr p = midiInPortMap.value(portId);
    if (p) {
        p->clients.removeAll(client);
        setModified(true);
    }
}

void KonfytProject::midiInPort_setPortFilter(int portId, KonfytMidiFilter filter)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    PrjMidiPortPtr p = midiInPortMap.value(portId);
    if (p) {
        p->filter = filter;
        setModified(true);
    }
}

int KonfytProject::midiOutPort_addPort(QString portName)
{
    PrjMidiPortPtr p(new PrjMidiPort());
    p->portName = portName;

    int portId = midiOutPort_getUniqueId();
    midiOutPortMap.insert(portId, p);

    setModified(true);

    return portId;
}

int KonfytProject::audioBus_getUniqueId()
{
    QList<int> l = audioBusMap.keys();
    return getUniqueIdHelper( l );
}

int KonfytProject::midiInPort_getUniqueId()
{
    QList<int> l = midiInPortMap.keys();
    return getUniqueIdHelper( l );
}

int KonfytProject::midiOutPort_getUniqueId()
{
    QList<int> l = midiOutPortMap.keys();
    return getUniqueIdHelper( l );
}

int KonfytProject::audioInPort_getUniqueId()
{
    QList<int> l = audioInPortMap.keys();
    return getUniqueIdHelper( l );
}

int KonfytProject::getUniqueIdHelper(QList<int> ids)
{
    /* Given the list of ids, find the highest id and add one. */
    int id=-1;
    for (int i=0; i<ids.count(); i++) {
        if (ids[i]>id) { id = ids[i]; }
    }
    return id+1;
}

void KonfytProject::writePortScript(QXmlStreamWriter* w, PrjMidiPortPtr prjPort) const
{
    w->writeStartElement(XML_PRJ_PORT_SCRIPT);

    w->writeTextElement(XML_PRJ_PORT_SCRIPT_CONTENT, prjPort->script);
    w->writeTextElement(XML_PRJ_PORT_SCRIPT_ENABLED,
                             QVariant(prjPort->scriptEnabled).toString());
    w->writeTextElement(XML_PRJ_PORT_SCRIPT_PASS_MIDI_THROUGH,
                             QVariant(prjPort->passMidiThrough).toString());

    w->writeEndElement(); // script
}

KonfytProject::PortScriptData KonfytProject::readPortScript(QXmlStreamReader* r)
{
    PortScriptData ret;

    while (r->readNextStartElement()) {
        if (r->name() == XML_PRJ_PORT_SCRIPT_CONTENT) {
            ret.content = r->readElementText();
        } else if (r->name() == XML_PRJ_PORT_SCRIPT_ENABLED) {
            ret.enabled = QVariant(r->readElementText()).toBool();
        } else if (r->name() == XML_PRJ_PORT_SCRIPT_PASS_MIDI_THROUGH) {
            ret.passMidiThrough = QVariant(r->readElementText()).toBool();
        } else {
            r->skipCurrentElement();
        }
    }

    return ret;
}

int KonfytProject::getUniqueExternalAppId()
{
    return getUniqueIdHelper( externalApps.keys() );
}

void KonfytProject::clearExternalApps()
{
    foreach (int id, externalApps.keys()) {
        removeExternalApp(id);
    }
}

void KonfytProject::writeExternalApps(QXmlStreamWriter *w)
{
    // Write old list for backwards compatibility
    // TODO DEPRECATED
    w->writeStartElement(XML_PRJ_PROCESSLIST);
    foreach (const ExternalApp& app, externalApps.values()) {
        w->writeStartElement(XML_PRJ_PROCESS);
        w->writeTextElement(XML_PRJ_PROCESS_APPNAME, app.command);
        w->writeEndElement(); // end of process
    }
    w->writeEndElement(); // end of processList

    // Write new-style list
    w->writeStartElement(XML_PRJ_EXT_APP_LIST);
    foreach (const ExternalApp& app, externalApps.values()) {
        w->writeStartElement(XML_PRJ_EXT_APP);
        w->writeTextElement(XML_PRJ_EXT_APP_NAME, app.friendlyName);
        w->writeTextElement(XML_PRJ_EXT_APP_CMD, app.command);
        w->writeTextElement(XML_PRJ_EXT_APP_RUNATSTARTUP, bool2str(app.runAtStartup));
        w->writeTextElement(XML_PRJ_EXT_APP_RESTART, bool2str(app.autoRestart));
        w->writeEndElement();
    }
    w->writeEndElement(); // end of external apps
}

void KonfytProject::preExternalAppsRead()
{
    // TODO DEPRECATED
    tempExternalAppList.clear();
}

void KonfytProject::readExternalApps(QXmlStreamReader *r)
{
    if (r->name() == XML_PRJ_PROCESSLIST) {
        // Old deprecated list for backwards compatability
        // Only load if other list not loaded yet
        // TODO DEPRECATED
        if (!tempExternalAppList.isEmpty()) {
            print("loadProject: Skipping deprecated external apps as new-style list already loaded.");
            r->skipCurrentElement();
            return;
        }

        while (r->readNextStartElement()) { // process
            ExternalApp app;
            while (r->readNextStartElement()) {
                if (r->name() == XML_PRJ_PROCESS_APPNAME) {
                    app.command = r->readElementText();
                } else {
                    print("loadProject: Unrecognized process element: " +r->name().toString());
                    r->skipCurrentElement();
                }
            }
            tempExternalAppList.append(app);
        }

    } else if (r->name() == XML_PRJ_EXT_APP_LIST) {

        if (!tempExternalAppList.isEmpty()) {
            // Clear (overwrite) temp list which could contain old deprecated list.
            print("loadProject: Ignoring deprecated old external apps list in favor new-style list found in project.");
            tempExternalAppList.clear();
        }

        while (r->readNextStartElement()) { // External App
            if (r->name() == XML_PRJ_EXT_APP) {
                ExternalApp app;
                while (r->readNextStartElement()) {
                    if (r->name() == XML_PRJ_EXT_APP_NAME) {
                        app.friendlyName = r->readElementText();
                    } else if (r->name() == XML_PRJ_EXT_APP_CMD) {
                        app.command = r->readElementText();
                    } else if (r->name() == XML_PRJ_EXT_APP_RUNATSTARTUP) {
                        app.runAtStartup = Qstr2bool(r->readElementText());
                    } else if (r->name() == XML_PRJ_EXT_APP_RESTART) {
                        app.autoRestart = Qstr2bool(r->readElementText());
                    } else {
                        print("loadProject: Unrecognized externalApp element: " + r->name().toString());
                        r->skipCurrentElement();
                    }
                }
                tempExternalAppList.append(app);
            } else {
                print("loadProject: Unrecognized externalAppList element: " + r->name().toString());
                r->skipCurrentElement();
            }
        }
    }
}

void KonfytProject::postExternalAppsRead()
{
    // TODO DEPRECATED
    foreach (const ExternalApp& app, tempExternalAppList) {
        addExternalApp(app);
    }
}

void KonfytProject::readJackMidiConList(QXmlStreamReader* r, bool makeNotBreak)
{
    // NB: Do not clear the jackMidiConList, as this function is called
    // separately for make and break connection lists during project load.
    // Make and break lists are stored separately for backwards compatibility
    // with older versions which only saved make connections.

    while (r->readNextStartElement()) {
        if (r->name() == XML_PRJ_OTHERJACKCON) {

            QString srcPort, destPort;
            while (r->readNextStartElement()) {
                if (r->name() == XML_PRJ_OTHERJACKCON_SRC) {
                    srcPort = r->readElementText();
                } else if (r->name() == XML_PRJ_OTHERJACKCON_DEST) {
                    destPort = r->readElementText();
                } else {
                    print("readJackMidiConList: Unrecognized JACK con element: "
                          + r->name().toString() );
                    r->skipCurrentElement();
                }
            }

            if (makeNotBreak) {
                addJackMidiConMake(srcPort, destPort);
            } else {
                addJackMidiConBreak(srcPort, destPort);
            }

        } else {
            print("readJackMidiConList: "
                  "Unrecognized otherJackMidiConList element: "
                  + r->name().toString() );
            r->skipCurrentElement();
        }
    }
}

void KonfytProject::readJackAudioConList(QXmlStreamReader* r, bool makeNotBreak)
{
    // NB: Do not clear the jackAudioConList, as this function is called
    // separately for make and break connection lists during project load.
    // Make and break lists are stored separately for backwards compatibility
    // with older versions which only saved make connections.

    while (r->readNextStartElement()) {
        if (r->name() == XML_PRJ_OTHERJACKCON) {

            QString srcPort, destPort;
            while (r->readNextStartElement()) {
                if (r->name() == XML_PRJ_OTHERJACKCON_SRC) {
                    srcPort = r->readElementText();
                } else if (r->name() == XML_PRJ_OTHERJACKCON_DEST) {
                    destPort = r->readElementText();
                } else {
                    print("readJackAudioConList: "
                          "Unrecognized JACK con element: "
                          + r->name().toString() );
                    r->skipCurrentElement();
                }
            }

            if (makeNotBreak) {
                addJackAudioConMake(srcPort, destPort);
            } else {
                addJackAudioConBreak(srcPort, destPort);
            }

        } else {
            print("readJackAudioConList: "
                  "Unrecognized otherJackAudioConList element: "
                  + r->name().toString() );
            r->skipCurrentElement();
        }
    }
}

/* Adds bus and returns unique bus id. */
int KonfytProject::audioBus_add(QString busName)
{
    PrjAudioBusPtr bus(new PrjAudioBus());
    bus->busName = busName;

    int busId = audioBus_getUniqueId();
    audioBusMap.insert(busId, bus);

    setModified(true);

    return busId;
}

void KonfytProject::audioBus_remove(int busId)
{
    KONFYT_ASSERT_RETURN(audioBusMap.contains(busId));

    audioBusMap.remove(busId);
    setModified(true);
}

int KonfytProject::audioBus_count()
{
    return audioBusMap.count();
}

bool KonfytProject::audioBus_exists(int busId)
{
    return audioBusMap.contains(busId);
}

PrjAudioBusPtr KonfytProject::audioBus_getBus(int busId)
{
    KONFYT_ASSERT(audioBusMap.contains(busId));

    return audioBusMap.value(busId);
}

/* Returns the bus id corresponding to the specified bus pointer, or -1 if the
 * specified bus does not exist. */
int KonfytProject::audioBus_idFromPtr(PrjAudioBusPtr p)
{
    return audioBusMap.key(p, -1);
}

/* Gets the first bus Id that is not skipId. */
int KonfytProject::audioBus_getFirstBusId(int skipId)
{
    int ret = -1;
    QList<int> l = audioBusMap.keys();

    KONFYT_ASSERT(l.count());

    for (int i=0; i<l.count(); i++) {
        if (l[i] != skipId) {
            ret = l[i];
            break;
        }
    }

    return ret;
}

QList<int> KonfytProject::audioBus_getAllBusIds()
{
    return audioBusMap.keys();
}

QList<PrjAudioBusPtr> KonfytProject::audioBus_getAllBuses()
{
    return audioBusMap.values();
}

void KonfytProject::audioBus_setName(int busId, QString name)
{
    KONFYT_ASSERT_RETURN(audioBus_exists(busId));
    audioBusMap[busId]->busName = name;
    setModified(true);
    emit audioBusNameChanged(busId);
}

void KonfytProject::audioBus_replace(int busId, PrjAudioBusPtr newBus)
{
    audioBus_replace_noModify(busId, newBus);
    setModified(true);
}

/* Do not change the project's modified state. */
void KonfytProject::audioBus_replace_noModify(int busId, PrjAudioBusPtr newBus)
{
    KONFYT_ASSERT_RETURN(audioBusMap.contains(busId));

    audioBusMap.insert(busId, newBus);
}

void KonfytProject::audioBus_addClient(int busId, PortLeftRight leftRight, QString client)
{
    KONFYT_ASSERT_RETURN(audioBusMap.contains(busId));

    PrjAudioBusPtr b = audioBusMap.value(busId);
    if (!b) { return; }

    if (leftRight == KonfytProject::LeftPort) {
        if (!b->leftOutClients.contains(client)) {
            b->leftOutClients.append(client);
        }
    } else {
        if (!b->rightOutClients.contains(client)) {
            b->rightOutClients.append(client);
        }
    }

    setModified(true);
}

void KonfytProject::audioBus_removeClient(int busId, PortLeftRight leftRight, QString client)
{
    KONFYT_ASSERT_RETURN(audioBusMap.contains(busId));

    PrjAudioBusPtr b = audioBusMap.value(busId);
    if (!b) { return; }

    if (leftRight == KonfytProject::LeftPort) {
        b->leftOutClients.removeAll(client);
    } else {
        b->rightOutClients.removeAll(client);
    }
    audioBusMap.insert(busId, b);
    setModified(true);
}

int KonfytProject::addExternalApp(ExternalApp app)
{
    int id = getUniqueExternalAppId();
    externalApps.insert(id, app);

    setModified(true);

    emit externalAppAdded(id);
    return id;
}

QList<int> KonfytProject::audioInPort_getAllPortIds()
{
    return audioInPortMap.keys();
}

QList<PrjAudioInPortPtr> KonfytProject::audioInPort_getAllPorts()
{
    return audioInPortMap.values();
}

int KonfytProject::audioInPort_add(QString portName)
{
    PrjAudioInPortPtr port(new PrjAudioInPort());
    port->portName = portName;

    int portId = audioInPort_getUniqueId();
    audioInPortMap.insert(portId, port);

    setModified(true);

    return portId;
}

void KonfytProject::audioInPort_remove(int portId)
{
    KONFYT_ASSERT_RETURN(audioInPortMap.contains(portId));

    audioInPortMap.remove(portId);
    setModified(true);
}

int KonfytProject::audioInPort_count()
{
    return audioInPortMap.count();
}

void KonfytProject::audioInPort_setName(int portId, QString name)
{
    PrjAudioInPortPtr p = audioInPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->portName = name;

    setModified(true);
    emit audioInPortNameChanged(portId);
}

void KonfytProject::audioInPort_setJackPorts(int portId, KfJackAudioPort *left, KfJackAudioPort *right)
{
    PrjAudioInPortPtr p = audioInPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->leftJackPort = left;
    p->rightJackPort = right;
    // Do not set modified
}

bool KonfytProject::audioInPort_exists(int portId) const
{
    return audioInPortMap.contains(portId);
}

PrjAudioInPortPtr KonfytProject::audioInPort_getPort(int portId) const
{
    KONFYT_ASSERT(audioInPortMap.contains(portId));

    return audioInPortMap.value(portId);
}

/* Returns the port id corresponding to the specified port pointer, or -1 if the
 * specified port does not exist. */
int KonfytProject::audioInPort_idFromPtr(PrjAudioInPortPtr p)
{
    return audioInPortMap.key(p, -1);
}

void KonfytProject::audioInPort_addClient(int portId, PortLeftRight leftRight, QString client)
{
    PrjAudioInPortPtr p = audioInPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    if (leftRight == KonfytProject::LeftPort) {
        if (!p->leftInClients.contains(client)) {
            p->leftInClients.append(client);
        }
    } else {
        if (!p->rightInClients.contains(client)) {
            p->rightInClients.append(client);
        }
    }

    setModified(true);
}

void KonfytProject::audioInPort_removeClient(int portId, PortLeftRight leftRight, QString client)
{
    PrjAudioInPortPtr p = audioInPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    if (leftRight == KonfytProject::LeftPort) {
        p->leftInClients.removeAll(client);
    } else {
        p->rightInClients.removeAll(client);
    }

    setModified(true);
}

void KonfytProject::midiOutPort_removePort(int portId)
{
    KONFYT_ASSERT_RETURN(midiOutPortMap.contains(portId));

    midiOutPortMap.remove(portId);
    setModified(true);
}

int KonfytProject::midiOutPort_count()
{
    return midiOutPortMap.count();
}

void KonfytProject::midiOutPort_setName(int portId, QString name)
{
    PrjMidiPortPtr p = midiOutPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->portName = name;

    setModified(true);
    emit midiOutPortNameChanged(portId);
}

void KonfytProject::midiOutPort_setJackPort(int portId, KfJackMidiPort *jackport)
{
    PrjMidiPortPtr p = midiOutPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->jackPort = jackport;
    // Do not set modified
}

bool KonfytProject::midiOutPort_exists(int portId) const
{
    return midiOutPortMap.contains(portId);
}

PrjMidiPortPtr KonfytProject::midiOutPort_getPort(int portId) const
{
    KONFYT_ASSERT(midiOutPortMap.contains(portId));

    return midiOutPortMap.value(portId);
}

/* Returns the port id corresponding to the specified port pointer, or -1 if the
 * specified port does not exist. */
int KonfytProject::midiOutPort_idFromPtr(PrjMidiPortPtr p)
{
    return midiOutPortMap.key(p, -1);
}

void KonfytProject::midiOutPort_addClient(int portId, QString client)
{
    PrjMidiPortPtr p = midiOutPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->clients.append(client);

    setModified(true);
}

void KonfytProject::midiOutPort_removeClient(int portId, QString client)
{
    PrjMidiPortPtr p = midiOutPortMap.value(portId);
    KONFYT_ASSERT_RETURN(!p.isNull());

    p->clients.removeAll(client);

    setModified(true);
}

QList<int> KonfytProject::midiOutPort_getAllPortIds()
{
    return midiOutPortMap.keys();
}

QList<PrjMidiPortPtr> KonfytProject::midiOutPort_getAllPorts()
{
    return midiOutPortMap.values();
}

QStringList KonfytProject::midiOutPort_getClients(int portId)
{
    QStringList ret;

    PrjMidiPortPtr p = midiOutPortMap.value(portId);
    KONFYT_ASSERT(!p.isNull());
    if (p) {
        ret = p->clients;
    }

    return ret;
}

void KonfytProject::removeExternalApp(int id)
{
    if (externalApps.contains(id)) {
        externalApps.remove(id);
        setModified(true);
        emit externalAppRemoved(id);
    } else {
        print("ERROR: removeExternalApp: INVALID ID " + n2s(id));
    }
}

ExternalApp KonfytProject::getExternalApp(int id)
{
    return externalApps.value(id);
}

QList<int> KonfytProject::getExternalAppIds()
{
    return externalApps.keys();
}

bool KonfytProject::hasExternalAppWithId(int id)
{
    return externalApps.keys().contains(id);
}

void KonfytProject::modifyExternalApp(int id, ExternalApp app)
{
    KONFYT_ASSERT_RETURN(externalApps.contains(id));

    externalApps.insert(id, app);
    setModified(true);
    emit externalAppModified(id);
}

void KonfytProject::addAndReplaceTrigger(KonfytTrigger newTrigger)
{
    // Remove any action that has the same trigger
    int trigint = newTrigger.toInt();
    QList<QString> l = triggerHash.keys();
    for (int i=0; i<l.count(); i++) {
        if (triggerHash.value(l[i]).toInt() == trigint) {
            triggerHash.remove(l[i]);
        }
    }

    triggerHash.insert(newTrigger.actionText, newTrigger);
    setModified(true);
}

void KonfytProject::removeTrigger(QString actionText)
{
    if (triggerHash.contains(actionText)) {
        triggerHash.remove(actionText);
        setModified(true);
    }
}

QList<KonfytTrigger> KonfytProject::getTriggerList()
{
    return triggerHash.values();
}

bool KonfytProject::isProgramChangeSwitchPatches()
{
    return programChangeSwitchPatches;
}

void KonfytProject::setProgramChangeSwitchPatches(bool value)
{
    programChangeSwitchPatches = value;
    setModified(true);
}

KonfytJackConPair KonfytProject::addJackMidiConMake(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    a.makeNotBreak = true;
    this->jackMidiConList.append(a);
    setModified(true);
    return a;
}

KonfytJackConPair KonfytProject::addJackMidiConBreak(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    a.makeNotBreak = false;
    this->jackMidiConList.append(a);
    setModified(true);
    return a;
}

QList<KonfytJackConPair> KonfytProject::getJackMidiConList()
{
    return this->jackMidiConList;
}

KonfytJackConPair KonfytProject::removeJackMidiCon(int i)
{
    KONFYT_ASSERT_RETURN_VAL( (i >= 0) && (i < jackMidiConList.count()),
                              KonfytJackConPair() );

    KonfytJackConPair p = jackMidiConList[i];
    jackMidiConList.removeAt(i);
    setModified(true);
    return p;
}

KonfytJackConPair KonfytProject::addJackAudioConMake(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    a.makeNotBreak = true;
    jackAudioConList.append(a);
    setModified(true);
    return a;
}

KonfytJackConPair KonfytProject::addJackAudioConBreak(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    a.makeNotBreak = false;
    jackAudioConList.append(a);
    setModified(true);
    return a;
}

QList<KonfytJackConPair> KonfytProject::getJackAudioConList()
{
    return jackAudioConList;
}

KonfytJackConPair KonfytProject::removeJackAudioCon(int i)
{
    KONFYT_ASSERT_RETURN_VAL( (i >= 0) && (i < jackAudioConList.count()),
                              KonfytJackConPair() );

    KonfytJackConPair p = jackAudioConList[i];
    jackAudioConList.removeAt(i);
    setModified(true);
    return p;
}

void KonfytProject::setModified(bool mod)
{
    this->modified = mod;
    emit projectModifiedStateChanged(mod);
}

QString KonfytProject::filenameFromProjectName()
{
    QString basename = sanitiseFilename(mProjectName.trimmed());
    if (basename.isEmpty()) {
        basename = "project";
    }
    return basename + PROJECT_FILENAME_EXTENSION;
}

/* Copy project files of the specified project filename and directory to the
 * backups directory under a new subdirectory, named to end with tag. */
void KonfytProject::backupProject(QString dirpath, QString projectFilename,
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
        if (!QFile(src).copy(dest)) {
            print(QString("ERROR: backupProject: Could not copy file %1 to %2")
                    .arg(src, dest));
        }
    }

    print("Backed up project to: " + backupDirPath);
}

void KonfytProject::writeToXmlStream(QXmlStreamWriter* stream)
{
    stream->setAutoFormatting(true);
    stream->writeStartDocument();

    stream->writeComment("This is a Konfyt project.");
    stream->writeComment(QString("Created with %1 version %2")
                        .arg(APP_NAME).arg(APP_VERSION));

    stream->writeStartElement(XML_PRJ);
    stream->writeAttribute(XML_PRJ_NAME, this->mProjectName);
    stream->writeAttribute(XML_PRJ_KONFYT_VERSION, APP_VERSION);

    // Write misc settings
    stream->writeTextElement(XML_PRJ_PATCH_LIST_NUMBERS, bool2str(patchListNumbers));
    stream->writeTextElement(XML_PRJ_PATCH_LIST_NOTES, bool2str(patchListNotes));
    stream->writeTextElement(XML_PRJ_MIDI_PICKUP_RANGE, n2s(midiPickupRange));

    // Write patches
    for (int i=0; i<patchList.count(); i++) {

        stream->writeStartElement(XML_PRJ_PATCH);
        // Patch properties
        KonfytPatch* pat = patchList.at(i);
        QString patchPathRel = QString("%1/%2_%3.%4")
                                    .arg(PROJECT_PATCH_DIR)
                                    .arg(i)
                                    .arg(sanitiseFilename(pat->name()))
                                    .arg(KONFYT_PATCH_SUFFIX);
        stream->writeTextElement(XML_PRJ_PATCH_FILENAME, patchPathRel);

        // Save the patch file in patches subdirectory
        QString patchPathAbs = mProjectDirpath + "/" + patchPathRel;
        if ( !pat->savePatchToFile(patchPathAbs) ) {
            print("ERROR: saveProjectAs: Failed to save patch " + patchPathAbs);
            // TODO: BUBBLE ERRORS UP SO A MSGBOX CAN BE SHOWN TO USER THAT ONE OR MORE ERRORS HAVE OCCURRED.
        } else {
            print("saveProjectAs: Saved patch: " + patchPathRel);
        }

        stream->writeEndElement();
    }

    // Write midiInPortList
    stream->writeStartElement(XML_PRJ_MIDI_IN_PORTLIST);
    QList<int> midiInIds = midiInPort_getAllPortIds();
    for (int i=0; i < midiInIds.count(); i++) {
        int id = midiInIds[i];
        PrjMidiPortPtr p = midiInPort_getPort(id);
        stream->writeStartElement(XML_PRJ_MIDI_IN_PORT);
        stream->writeTextElement(XML_PRJ_MIDI_IN_PORT_ID, n2s(id));
        stream->writeTextElement(XML_PRJ_MIDI_IN_PORT_NAME, p->portName);
        p->filter.writeToXMLStream(stream);
        foreach (QString client, p->clients) {
            stream->writeTextElement(XML_PRJ_MIDI_IN_PORT_CLIENT, client);
        }
        writePortScript(stream, p);
        stream->writeEndElement(); // end of port
    }
    stream->writeEndElement(); // end of midiInPortList

    // Write midiOutPortList
    stream->writeStartElement(XML_PRJ_MIDI_OUT_PORTLIST);
    QList<int> midiOutIds = midiOutPort_getAllPortIds();
    for (int i=0; i<midiOutIds.count(); i++) {
        int id = midiOutIds[i];
        PrjMidiPortPtr p = midiOutPort_getPort(id);
        stream->writeStartElement(XML_PRJ_MIDI_OUT_PORT);
        stream->writeTextElement(XML_PRJ_MIDI_OUT_PORT_ID, n2s(id));
        stream->writeTextElement(XML_PRJ_MIDI_OUT_PORT_NAME, p->portName);
        foreach (QString client, p->clients) {
            stream->writeTextElement(XML_PRJ_MIDI_OUT_PORT_CLIENT, client);
        }
        stream->writeEndElement(); // end of port
    }
    stream->writeEndElement(); // end of midiOutPortList

    // Write audioBusList
    stream->writeStartElement(XML_PRJ_BUSLIST);
    QList<int> busIds = audioBus_getAllBusIds();
    for (int i=0; i<busIds.count(); i++) {
        stream->writeStartElement(XML_PRJ_BUS);
        PrjAudioBusPtr b = audioBusMap.value(busIds[i]);
        stream->writeTextElement( XML_PRJ_BUS_ID, n2s(busIds[i]) );
        stream->writeTextElement( XML_PRJ_BUS_NAME, b->busName );
        stream->writeTextElement( XML_PRJ_BUS_LGAIN, n2s(b->leftGain) );
        stream->writeTextElement( XML_PRJ_BUS_RGAIN, n2s(b->rightGain) );
        stream->writeTextElement( XML_PRJ_BUS_IGNORE_GLOBAL_VOLUME,
                                 bool2str(b->ignoreMasterGain) );
        foreach (QString client, b->leftOutClients) {
            stream->writeTextElement(XML_PRJ_BUS_LCLIENT, client);
        }
        foreach (QString client, b->rightOutClients) {
            stream->writeTextElement(XML_PRJ_BUS_RCLIENT, client);
        }
        stream->writeEndElement(); // end of bus
    }
    stream->writeEndElement(); // end of audioBusList

    // Write audio input ports
    stream->writeStartElement(XML_PRJ_AUDIOINLIST);
    QList<int> audioInIds = audioInPort_getAllPortIds();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        PrjAudioInPortPtr p = audioInPort_getPort(id);
        stream->writeStartElement(XML_PRJ_AUDIOIN_PORT);
        stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_ID, n2s(id) );
        stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_NAME, p->portName );
        stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_LGAIN, n2s(p->leftGain) );
        stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_RGAIN, n2s(p->rightGain) );
        foreach (QString client, p->leftInClients) {
            stream->writeTextElement(XML_PRJ_AUDIOIN_PORT_LCLIENT, client);
        }
        foreach (QString client, p->rightInClients) {
            stream->writeTextElement(XML_PRJ_AUDIOIN_PORT_RCLIENT, client);
        }
        stream->writeEndElement(); // end of port
    }
    stream->writeEndElement(); // end of audioInputPortList

    // External applications
    writeExternalApps(stream);

    // Write trigger list
    stream->writeStartElement(XML_PRJ_TRIGGERLIST);
    stream->writeTextElement(XML_PRJ_PROG_CHANGE_SWITCH_PATCHES, bool2str(programChangeSwitchPatches));
    QList<KonfytTrigger> trigs = triggerHash.values();
    for (int i=0; i<trigs.count(); i++) {
        stream->writeStartElement(XML_PRJ_TRIGGER);
        stream->writeTextElement(XML_PRJ_TRIGGER_ACTIONTEXT, trigs[i].actionText);
        stream->writeTextElement(XML_PRJ_TRIGGER_TYPE, n2s(trigs[i].type) );
        stream->writeTextElement(XML_PRJ_TRIGGER_CHAN, n2s(trigs[i].channel) );
        stream->writeTextElement(XML_PRJ_TRIGGER_DATA1, n2s(trigs[i].data1) );
        stream->writeTextElement(XML_PRJ_TRIGGER_BANKMSB, n2s(trigs[i].bankMSB) );
        stream->writeTextElement(XML_PRJ_TRIGGER_BANKLSB, n2s(trigs[i].bankLSB) );
        stream->writeEndElement(); // end of trigger
    }
    stream->writeEndElement(); // end of trigger list

    // Write other JACK MIDI make-connections list
    // Make and break lists are stored separately for backwards compatibility
    // with older versions which only saved make connections.
    stream->writeStartElement(XML_PRJ_OTHERJACK_MIDI_CON_LIST);
    foreach (const KonfytJackConPair& p, jackMidiConList) {
        if (!p.makeNotBreak) { continue; }
        stream->writeStartElement(XML_PRJ_OTHERJACKCON);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_SRC, p.srcPort);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_DEST, p.destPort);
        stream->writeEndElement(); // end JACK connection pair
    }
    stream->writeEndElement(); // Other JACK MIDI connections list

    // Write other JACK MIDI break-connections list
    stream->writeStartElement(XML_PRJ_OTHERJACK_MIDI_CON_BREAK_LIST);
    foreach (const KonfytJackConPair& p, jackMidiConList) {
        if (p.makeNotBreak) { continue; }
        stream->writeStartElement(XML_PRJ_OTHERJACKCON);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_SRC, p.srcPort);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_DEST, p.destPort);
        stream->writeEndElement();
    }
    stream->writeEndElement();

    // Write other JACK Audio make-connections list
    // Make and break lists are stored separately for backwards compatibility
    // with older versions which only saved make connections.
    stream->writeStartElement(XML_PRJ_OTHERJACK_AUDIO_CON_LIST);
    foreach (const KonfytJackConPair& p, jackAudioConList) {
        if (!p.makeNotBreak) { continue; }
        stream->writeStartElement(XML_PRJ_OTHERJACKCON);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_SRC, p.srcPort);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_DEST, p.destPort);
        stream->writeEndElement();
    }
    stream->writeEndElement();

    // Write other JACK Audio break-connections list
    stream->writeStartElement(XML_PRJ_OTHERJACK_AUDIO_CON_BREAK_LIST);
    foreach (const KonfytJackConPair& p, jackAudioConList) {
        if (p.makeNotBreak) { continue; }
        stream->writeStartElement(XML_PRJ_OTHERJACKCON);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_SRC, p.srcPort);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_DEST, p.destPort);
        stream->writeEndElement();
    }
    stream->writeEndElement();

    stream->writeEndElement(); // project

    stream->writeEndDocument();
}

void KonfytProject::readFromXmlStream(QXmlStreamReader* r)
{
    r->setNamespaceProcessing(false);

    QString patchPathRel;
    patchList.clear();
    midiInPortMap.clear();
    midiOutPortMap.clear();
    clearExternalApps();
    preExternalAppsRead(); // Used for backwards compatibility.
    audioBusMap.clear();
    audioInPortMap.clear();
    jackMidiConList.clear();
    jackAudioConList.clear();

    while (r->readNextStartElement()) { // project

        // Get the project name attribute
        QXmlStreamAttributes ats =  r->attributes();
        if (ats.count()) {
            this->mProjectName = ats.at(0).value().toString(); // Project name
        }

        while (r->readNextStartElement()) {

            if (r->name() == XML_PRJ_PATCH) { // patch

                while (r->readNextStartElement()) { // patch properties

                    if (r->name() == XML_PRJ_PATCH_FILENAME) {
                        patchPathRel = r->readElementText();
                    } else {
                        print("loadProject: Unrecognized patch element: "
                              + r->name().toString() );
                        r->skipCurrentElement();
                    }

                }

                // Add new patch
                KonfytPatch* pt = new KonfytPatch();
                QString errors;
                QString patchPathAbs = mProjectDirpath + "/" + patchPathRel;
                print("loadProject: Loading patch " + patchPathRel);
                if (pt->loadPatchFromFile(patchPathAbs, &errors)) {
                    this->addPatch(pt);
                } else {
                    // Error message on loading patch.
                    print("loadProject: Error loading patch: " + patchPathAbs);
                }
                if (!errors.isEmpty()) {
                    print("Load errors for patch " + patchPathAbs + ":\n" + errors);
                }

            } else if (r->name() == XML_PRJ_PATCH_LIST_NUMBERS) {

                patchListNumbers = Qstr2bool(r->readElementText());

            } else if (r->name() == XML_PRJ_PATCH_LIST_NOTES) {

                patchListNotes = Qstr2bool(r->readElementText());

            } else if (r->name() == XML_PRJ_MIDI_PICKUP_RANGE) {

                setMidiPickupRange(r->readElementText().toInt());

            } else if (r->name() == XML_PRJ_MIDI_IN_PORTLIST) {

                while (r->readNextStartElement()) { // port
                    PrjMidiPortPtr p(new PrjMidiPort());
                    int id = midiInPort_getUniqueId();
                    while (r->readNextStartElement()) {
                        if (r->name() == XML_PRJ_MIDI_IN_PORT_ID) {
                            id = r->readElementText().toInt();
                        } else if (r->name() == XML_PRJ_MIDI_IN_PORT_NAME) {
                            p->portName = r->readElementText();
                        } else if (r->name() == XML_PRJ_MIDI_IN_PORT_CLIENT) {
                            p->clients.append( r->readElementText() );
                        } else if (r->name() == XML_MIDIFILTER) {
                            p->filter.readFromXMLStream(r);
                        } else if (r->name() == XML_PRJ_PORT_SCRIPT) {
                            PortScriptData s = readPortScript(r);
                            p->script = s.content;
                            p->scriptEnabled = s.enabled;
                            p->passMidiThrough = s.passMidiThrough;
                        } else {
                            print("loadProject: "
                                  "Unrecognized midiInPortList port element: "
                                  + r->name().toString() );
                        }
                    }
                    if (midiInPortMap.contains(id)) {
                        print("loadProject: Duplicate midi in port id detected: "
                              + n2s(id));
                    }
                    this->midiInPortMap.insert(id, p);
                }

            } else if (r->name() == XML_PRJ_MIDI_OUT_PORTLIST) {

                while (r->readNextStartElement()) { // port
                    PrjMidiPortPtr p(new PrjMidiPort());
                    int id = midiOutPort_getUniqueId();
                    while (r->readNextStartElement()) {
                        if (r->name() == XML_PRJ_MIDI_OUT_PORT_ID) {
                            id = r->readElementText().toInt();
                        } else if (r->name() == XML_PRJ_MIDI_OUT_PORT_NAME) {
                            p->portName = r->readElementText();
                        } else if (r->name() == XML_PRJ_MIDI_OUT_PORT_CLIENT) {
                            p->clients.append( r->readElementText() );
                        } else {
                            print("loadProject: "
                                  "Unrecognized midiOutPortList port element: "
                                  + r->name().toString() );
                            r->skipCurrentElement();
                        }
                    }
                    if (midiOutPortMap.contains(id)) {
                        print("loadProject: Duplicate midi out port id detected: "
                              + n2s(id));
                    }
                    this->midiOutPortMap.insert(id, p);
                }

            } else if (r->name() == XML_PRJ_BUSLIST) {

                while (r->readNextStartElement()) { // bus
                    PrjAudioBusPtr b(new PrjAudioBus());
                    int id = audioBus_getUniqueId();
                    while (r->readNextStartElement()) {
                        if (r->name() == XML_PRJ_BUS_ID) {
                            id = r->readElementText().toInt();
                        } else if (r->name() == XML_PRJ_BUS_NAME) {
                            b->busName = r->readElementText();
                        } else if (r->name() == XML_PRJ_BUS_LGAIN) {
                            b->leftGain = r->readElementText().toFloat();
                        } else if (r->name() == XML_PRJ_BUS_RGAIN) {
                            b->rightGain = r->readElementText().toFloat();
                        } else if (r->name() == XML_PRJ_BUS_LCLIENT) {
                            b->leftOutClients.append( r->readElementText() );
                        } else if (r->name() == XML_PRJ_BUS_RCLIENT) {
                            b->rightOutClients.append( r->readElementText() );
                        } else if (r->name() == XML_PRJ_BUS_IGNORE_GLOBAL_VOLUME) {
                            b->ignoreMasterGain = Qstr2bool(r->readElementText());
                        } else {
                            print("loadProject: Unrecognized bus element: "
                                  + r->name().toString() );
                            r->skipCurrentElement();
                        }
                    }
                    if (audioBusMap.contains(id)) {
                        print("loadProject: Duplicate bus id detected: " + n2s(id));
                    }
                    this->audioBusMap.insert(id, b);
                }

            } else if (r->name() == XML_PRJ_AUDIOINLIST) {

                while (r->readNextStartElement()) { // port
                    PrjAudioInPortPtr p(new PrjAudioInPort());
                    int id = audioInPort_getUniqueId();
                    while (r->readNextStartElement()) {
                        if (r->name() == XML_PRJ_AUDIOIN_PORT_ID) {
                            id = r->readElementText().toInt();
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_NAME) {
                            p->portName = r->readElementText();
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_LGAIN) {
                            p->leftGain = r->readElementText().toFloat();
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_RGAIN) {
                            p->rightGain = r->readElementText().toFloat();
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_LCLIENT) {
                            p->leftInClients.append( r->readElementText() );
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_RCLIENT) {
                            p->rightInClients.append( r->readElementText() );
                        } else {
                            print("loadProject: "
                                  "Unrecognized audio input port element: "
                                  + r->name().toString() );
                            r->skipCurrentElement();
                        }
                    }
                    if (audioInPortMap.contains(id)) {
                        print("loadProject: Duplicate audio in port id detected: "
                              + n2s(id));
                    }
                    this->audioInPortMap.insert(id, p);
                }

            } else if (r->name() == XML_PRJ_PROCESSLIST) {

                // TODO DEPRECATED
                readExternalApps(r);

            } else if (r->name() == XML_PRJ_EXT_APP_LIST) {

                readExternalApps(r);


            } else if (r->name() == XML_PRJ_TRIGGERLIST) {

                while (r->readNextStartElement()) {

                    if (r->name() == XML_PRJ_PROG_CHANGE_SWITCH_PATCHES) {
                        programChangeSwitchPatches = Qstr2bool(r->readElementText());
                    } else if (r->name() == XML_PRJ_TRIGGER) {

                        KonfytTrigger trig;
                        while (r->readNextStartElement()) {
                            if (r->name() == XML_PRJ_TRIGGER_ACTIONTEXT) {
                                trig.actionText = r->readElementText();
                            } else if (r->name() == XML_PRJ_TRIGGER_TYPE) {
                                trig.type = r->readElementText().toInt();
                            } else if (r->name() == XML_PRJ_TRIGGER_CHAN) {
                                trig.channel = r->readElementText().toInt();
                            } else if (r->name() == XML_PRJ_TRIGGER_DATA1) {
                                trig.data1 = r->readElementText().toInt();
                            } else if (r->name() == XML_PRJ_TRIGGER_BANKMSB) {
                                trig.bankMSB = r->readElementText().toInt();
                            } else if (r->name() == XML_PRJ_TRIGGER_BANKLSB) {
                                trig.bankLSB = r->readElementText().toInt();
                            } else {
                                print("loadProject: Unrecognized trigger element: "
                                      + r->name().toString() );
                                r->skipCurrentElement();
                            }
                        }
                        this->addAndReplaceTrigger(trig);

                    } else {
                        print("loadProject: Unrecognized triggerList element: "
                              + r->name().toString() );
                        r->skipCurrentElement();
                    }
                }

            } else if (r->name() == XML_PRJ_OTHERJACK_MIDI_CON_LIST) {

                readJackMidiConList(r, true);

            } else if (r->name() == XML_PRJ_OTHERJACK_MIDI_CON_BREAK_LIST) {

                readJackMidiConList(r, false);

            } else if (r->name() == XML_PRJ_OTHERJACK_AUDIO_CON_LIST) {

                readJackAudioConList(r, true);

            } else if (r->name() == XML_PRJ_OTHERJACK_AUDIO_CON_BREAK_LIST) {

                readJackAudioConList(r, false);

            } else {
                print("loadProject: Unrecognized project element: "
                      + r->name().toString() );
                r->skipCurrentElement();
            }
        }
    }
}

bool KonfytProject::isModified()
{
    return this->modified;
}
