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
    if (mProjectDirpath == "") {
        return false;
    } else {
        return saveProjectAs(mProjectDirpath);
    }
}

/* Save project xml file containing a list of all the patches, as well as all
 * the related patch files, in the specified directory. */
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

void KonfytProject::setProjectName(QString newName)
{
    mProjectName = newName;
    setModified(true);
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

QString KonfytProject::getProjectName()
{
    return mProjectName;
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

QString KonfytProject::getDirname()
{
    return mProjectDirpath;
}

void KonfytProject::setDirname(QString newDirname)
{
    mProjectDirpath = newDirname;
    setModified(true);
}

QList<int> KonfytProject::midiInPort_getAllPortIds()
{
    return midiInPortMap.keys();
}

int KonfytProject::midiInPort_addPort(QString portName)
{
    PrjMidiPort p;
    p.portName = portName;

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

PrjMidiPort KonfytProject::midiInPort_getPort(int portId)
{
    KONFYT_ASSERT_RETURN_VAL(midiInPort_exists(portId), PrjMidiPort());

    return midiInPortMap.value(portId);
}

int KonfytProject::midiInPort_getPortIdWithJackId(KfJackMidiPort *jackPort)
{
    int ret = -1;

    QList<int> ids = midiInPortMap.keys();
    foreach (int id, ids) {
        PrjMidiPort p = midiInPortMap[id];
        if (p.jackPort == jackPort) {
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

    for (int i=0; i<l.count(); i++) {
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

    midiInPortMap[portId].portName = name;
    setModified(true);
    emit midiInPortNameChanged(portId);
}

void KonfytProject::midiInPort_setJackPort(int portId, KfJackMidiPort *jackport)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    midiInPortMap[portId].jackPort = jackport;
    // Do not set the project modified
}

QStringList KonfytProject::midiInPort_getClients(int portId)
{
    QStringList ret;
    KONFYT_ASSERT_RETURN_VAL(midiInPort_exists(portId), ret);

    ret = midiInPortMap.value(portId).clients;

    return ret;
}

void KonfytProject::midiInPort_addClient(int portId, QString client)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    PrjMidiPort p = midiInPortMap.value(portId);
    p.clients.append(client);
    midiInPortMap.insert(portId, p);
    setModified(true);
}

void KonfytProject::midiInPort_removeClient(int portId, QString client)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    PrjMidiPort p = midiInPortMap.value(portId);
    p.clients.removeAll(client);
    midiInPortMap.insert(portId, p);
    setModified(true);
}

void KonfytProject::midiInPort_setPortFilter(int portId, KonfytMidiFilter filter)
{
    KONFYT_ASSERT_RETURN(midiInPort_exists(portId));

    PrjMidiPort p = midiInPortMap.value(portId);
    p.filter = filter;
    midiInPortMap.insert(portId, p);
    setModified(true);
}

int KonfytProject::midiOutPort_addPort(QString portName)
{
    PrjMidiPort p;
    p.portName = portName;

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

/* Adds bus and returns unique bus id. */
int KonfytProject::audioBus_add(QString busName)
{
    PrjAudioBus bus;
    bus.busName = busName;

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

PrjAudioBus KonfytProject::audioBus_getBus(int busId)
{
    KONFYT_ASSERT(audioBusMap.contains(busId));

    return audioBusMap.value(busId);
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

void KonfytProject::audioBus_replace(int busId, PrjAudioBus newBus)
{
    audioBus_replace_noModify(busId, newBus);
    setModified(true);
}

/* Do not change the project's modified state. */
void KonfytProject::audioBus_replace_noModify(int busId, PrjAudioBus newBus)
{
    KONFYT_ASSERT_RETURN(audioBusMap.contains(busId));

    audioBusMap.insert(busId, newBus);
}

void KonfytProject::audioBus_addClient(int busId, PortLeftRight leftRight, QString client)
{
    KONFYT_ASSERT_RETURN(audioBusMap.contains(busId));

    PrjAudioBus b = audioBusMap.value(busId);
    if (leftRight == KonfytProject::LeftPort) {
        if (!b.leftOutClients.contains(client)) {
            b.leftOutClients.append(client);
        }
    } else {
        if (!b.rightOutClients.contains(client)) {
            b.rightOutClients.append(client);
        }
    }
    audioBusMap.insert(busId, b);
    setModified(true);
}

void KonfytProject::audioBus_removeClient(int busId, PortLeftRight leftRight, QString client)
{
    KONFYT_ASSERT_RETURN(audioBusMap.contains(busId));

    PrjAudioBus b = audioBusMap.value(busId);
    if (leftRight == KonfytProject::LeftPort) {
        b.leftOutClients.removeAll(client);
    } else {
        b.rightOutClients.removeAll(client);
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

int KonfytProject::audioInPort_add(QString portName)
{
    PrjAudioInPort port;
    port.portName = portName;

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
    KONFYT_ASSERT_RETURN(audioInPort_exists(portId));
    audioInPortMap[portId].portName = name;
    setModified(true);
    emit audioInPortNameChanged(portId);
}

void KonfytProject::audioInPort_setJackPorts(int portId, KfJackAudioPort *left, KfJackAudioPort *right)
{
    KONFYT_ASSERT_RETURN(audioInPort_exists(portId));
    audioInPortMap[portId].leftJackPort = left;
    audioInPortMap[portId].rightJackPort = right;
    // Do not set modified
}

bool KonfytProject::audioInPort_exists(int portId) const
{
    return audioInPortMap.contains(portId);
}

PrjAudioInPort KonfytProject::audioInPort_getPort(int portId) const
{
    KONFYT_ASSERT(audioInPortMap.contains(portId));

    return audioInPortMap.value(portId);
}

void KonfytProject::audioInPort_addClient(int portId, PortLeftRight leftRight, QString client)
{
    KONFYT_ASSERT_RETURN(audioInPortMap.contains(portId));

    PrjAudioInPort p = audioInPortMap.value(portId);
    if (leftRight == KonfytProject::LeftPort) {
        if (!p.leftInClients.contains(client)) {
            p.leftInClients.append(client);
        }
    } else {
        if (!p.rightInClients.contains(client)) {
            p.rightInClients.append(client);
        }
    }
    audioInPortMap.insert(portId, p);
    setModified(true);
}

void KonfytProject::audioInPort_removeClient(int portId, PortLeftRight leftRight, QString client)
{
    KONFYT_ASSERT_RETURN(audioInPortMap.contains(portId));

    PrjAudioInPort p = audioInPortMap.value(portId);
    if (leftRight == KonfytProject::LeftPort) {
        p.leftInClients.removeAll(client);
    } else {
        p.rightInClients.removeAll(client);
    }
    audioInPortMap.insert(portId, p);
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
    KONFYT_ASSERT_RETURN(midiOutPort_exists(portId));
    midiOutPortMap[portId].portName = name;
    setModified(true);
    emit midiOutPortNameChanged(portId);
}

void KonfytProject::midiOutPort_setJackPort(int portId, KfJackMidiPort *jackport)
{
    KONFYT_ASSERT_RETURN(midiOutPort_exists(portId));
    midiOutPortMap[portId].jackPort = jackport;
    // Do not set modified
}

bool KonfytProject::midiOutPort_exists(int portId) const
{
    return midiOutPortMap.contains(portId);
}

PrjMidiPort KonfytProject::midiOutPort_getPort(int portId) const
{
    KONFYT_ASSERT(midiOutPortMap.contains(portId));

    return midiOutPortMap.value(portId);
}

void KonfytProject::midiOutPort_addClient(int portId, QString client)
{
    KONFYT_ASSERT_RETURN(midiOutPortMap.contains(portId));

    PrjMidiPort p = midiOutPortMap.value(portId);
    p.clients.append(client);
    midiOutPortMap.insert(portId, p);
    setModified(true);
}

void KonfytProject::midiOutPort_removeClient(int portId, QString client)
{
    KONFYT_ASSERT_RETURN(midiOutPortMap.contains(portId));

    PrjMidiPort p = midiOutPortMap.value(portId);
    p.clients.removeAll(client);
    midiOutPortMap.insert(portId, p);
    setModified(true);
}

QList<int> KonfytProject::midiOutPort_getAllPortIds()
{
    return midiOutPortMap.keys();
}

QStringList KonfytProject::midiOutPort_getClients(int portId)
{
    KONFYT_ASSERT(midiOutPortMap.contains(portId));

    return midiOutPortMap.value(portId).clients;
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

KonfytJackConPair KonfytProject::addJackMidiCon(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
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

KonfytJackConPair KonfytProject::addJackAudioCon(QString srcPort, QString destPort)
{
    KonfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
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
        PrjMidiPort p = midiInPort_getPort(id);
        stream->writeStartElement(XML_PRJ_MIDI_IN_PORT);
        stream->writeTextElement(XML_PRJ_MIDI_IN_PORT_ID, n2s(id));
        stream->writeTextElement(XML_PRJ_MIDI_IN_PORT_NAME, p.portName);
        p.filter.writeToXMLStream(stream);
        QStringList l = p.clients;
        for (int j=0; j < l.count(); j++) {
            stream->writeTextElement(XML_PRJ_MIDI_IN_PORT_CLIENT, l.at(j));
        }
        stream->writeEndElement(); // end of port
    }
    stream->writeEndElement(); // end of midiInPortList

    // Write midiOutPortList
    stream->writeStartElement(XML_PRJ_MIDI_OUT_PORTLIST);
    QList<int> midiOutIds = midiOutPort_getAllPortIds();
    for (int i=0; i<midiOutIds.count(); i++) {
        int id = midiOutIds[i];
        PrjMidiPort p = midiOutPort_getPort(id);
        stream->writeStartElement(XML_PRJ_MIDI_OUT_PORT);
        stream->writeTextElement(XML_PRJ_MIDI_OUT_PORT_ID, n2s(id));
        stream->writeTextElement(XML_PRJ_MIDI_OUT_PORT_NAME, p.portName);
        QStringList l = p.clients;
        for (int j=0; j<l.count(); j++) {
            stream->writeTextElement(XML_PRJ_MIDI_OUT_PORT_CLIENT, l.at(j));
        }
        stream->writeEndElement(); // end of port
    }
    stream->writeEndElement(); // end of midiOutPortList

    // Write audioBusList
    stream->writeStartElement(XML_PRJ_BUSLIST);
    QList<int> busIds = audioBus_getAllBusIds();
    for (int i=0; i<busIds.count(); i++) {
        stream->writeStartElement(XML_PRJ_BUS);
        PrjAudioBus b = audioBusMap.value(busIds[i]);
        stream->writeTextElement( XML_PRJ_BUS_ID, n2s(busIds[i]) );
        stream->writeTextElement( XML_PRJ_BUS_NAME, b.busName );
        stream->writeTextElement( XML_PRJ_BUS_LGAIN, n2s(b.leftGain) );
        stream->writeTextElement( XML_PRJ_BUS_RGAIN, n2s(b.rightGain) );
        stream->writeTextElement( XML_PRJ_BUS_IGNORE_GLOBAL_VOLUME,
                                 bool2str(b.ignoreMasterGain) );
        for (int j=0; j<b.leftOutClients.count(); j++) {
            stream->writeTextElement( XML_PRJ_BUS_LCLIENT, b.leftOutClients.at(j) );
        }
        for (int j=0; j<b.rightOutClients.count(); j++) {
            stream->writeTextElement( XML_PRJ_BUS_RCLIENT, b.rightOutClients.at(j) );
        }
        stream->writeEndElement(); // end of bus
    }
    stream->writeEndElement(); // end of audioBusList

    // Write audio input ports
    stream->writeStartElement(XML_PRJ_AUDIOINLIST);
    QList<int> audioInIds = audioInPort_getAllPortIds();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        PrjAudioInPort p = audioInPort_getPort(id);
        stream->writeStartElement(XML_PRJ_AUDIOIN_PORT);
        stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_ID, n2s(id) );
        stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_NAME, p.portName );
        stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_LGAIN, n2s(p.leftGain) );
        stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_RGAIN, n2s(p.rightGain) );
        for (int j=0; j<p.leftInClients.count(); j++) {
            stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_LCLIENT, p.leftInClients.at(j) );
        }
        for (int j=0; j<p.rightInClients.count(); j++) {
            stream->writeTextElement( XML_PRJ_AUDIOIN_PORT_RCLIENT, p.rightInClients.at(j) );
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

    // Write other JACK MIDI connections list
    stream->writeStartElement(XML_PRJ_OTHERJACK_MIDI_CON_LIST);
    for (int i=0; i<jackMidiConList.count(); i++) {
        stream->writeStartElement(XML_PRJ_OTHERJACKCON);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_SRC, jackMidiConList[i].srcPort);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_DEST, jackMidiConList[i].destPort);
        stream->writeEndElement(); // end JACK connection pair
    }
    stream->writeEndElement(); // Other JACK MIDI connections list

    // Write other JACK Audio connections list
    stream->writeStartElement(XML_PRJ_OTHERJACK_AUDIO_CON_LIST);
    for (int i=0; i < jackAudioConList.count(); i++) {
        stream->writeStartElement(XML_PRJ_OTHERJACKCON); // start JACK connection pair
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_SRC, jackAudioConList[i].srcPort);
        stream->writeTextElement(XML_PRJ_OTHERJACKCON_DEST, jackAudioConList[i].destPort);
        stream->writeEndElement(); // end JACK connection pair
    }
    stream->writeEndElement(); // end JACK Audio connections list

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
                    PrjMidiPort p;
                    int id = midiInPort_getUniqueId();
                    while (r->readNextStartElement()) {
                        if (r->name() == XML_PRJ_MIDI_IN_PORT_ID) {
                            id = r->readElementText().toInt();
                        } else if (r->name() == XML_PRJ_MIDI_IN_PORT_NAME) {
                            p.portName = r->readElementText();
                        } else if (r->name() == XML_PRJ_MIDI_IN_PORT_CLIENT) {
                            p.clients.append( r->readElementText() );
                        } else if (r->name() == XML_MIDIFILTER) {
                            p.filter.readFromXMLStream(r);
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
                    PrjMidiPort p;
                    int id = midiOutPort_getUniqueId();
                    while (r->readNextStartElement()) {
                        if (r->name() == XML_PRJ_MIDI_OUT_PORT_ID) {
                            id = r->readElementText().toInt();
                        } else if (r->name() == XML_PRJ_MIDI_OUT_PORT_NAME) {
                            p.portName = r->readElementText();
                        } else if (r->name() == XML_PRJ_MIDI_OUT_PORT_CLIENT) {
                            p.clients.append( r->readElementText() );
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
                    PrjAudioBus b;
                    int id = audioBus_getUniqueId();
                    while (r->readNextStartElement()) {
                        if (r->name() == XML_PRJ_BUS_ID) {
                            id = r->readElementText().toInt();
                        } else if (r->name() == XML_PRJ_BUS_NAME) {
                            b.busName = r->readElementText();
                        } else if (r->name() == XML_PRJ_BUS_LGAIN) {
                            b.leftGain = r->readElementText().toFloat();
                        } else if (r->name() == XML_PRJ_BUS_RGAIN) {
                            b.rightGain = r->readElementText().toFloat();
                        } else if (r->name() == XML_PRJ_BUS_LCLIENT) {
                            b.leftOutClients.append( r->readElementText() );
                        } else if (r->name() == XML_PRJ_BUS_RCLIENT) {
                            b.rightOutClients.append( r->readElementText() );
                        } else if (r->name() == XML_PRJ_BUS_IGNORE_GLOBAL_VOLUME) {
                            b.ignoreMasterGain = Qstr2bool(r->readElementText());
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
                    PrjAudioInPort p;
                    int id = audioInPort_getUniqueId();
                    while (r->readNextStartElement()) {
                        if (r->name() == XML_PRJ_AUDIOIN_PORT_ID) {
                            id = r->readElementText().toInt();
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_NAME) {
                            p.portName = r->readElementText();
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_LGAIN) {
                            p.leftGain = r->readElementText().toFloat();
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_RGAIN) {
                            p.rightGain = r->readElementText().toFloat();
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_LCLIENT) {
                            p.leftInClients.append( r->readElementText() );
                        } else if (r->name() == XML_PRJ_AUDIOIN_PORT_RCLIENT) {
                            p.rightInClients.append( r->readElementText() );
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

                // Other JACK MIDI connections list

                while (r->readNextStartElement()) {
                    if (r->name() == XML_PRJ_OTHERJACKCON) {

                        QString srcPort, destPort;
                        while (r->readNextStartElement()) {
                            if (r->name() == XML_PRJ_OTHERJACKCON_SRC) {
                                srcPort = r->readElementText();
                            } else if (r->name() == XML_PRJ_OTHERJACKCON_DEST) {
                                destPort = r->readElementText();
                            } else {
                                print("loadProject: Unrecognized JACK con element: "
                                      + r->name().toString() );
                                r->skipCurrentElement();
                            }
                        }
                        this->addJackMidiCon(srcPort, destPort);

                    } else {
                        print("loadProject: "
                              "Unrecognized otherJackMidiConList element: "
                              + r->name().toString() );
                        r->skipCurrentElement();
                    }
                }

            } else if (r->name() == XML_PRJ_OTHERJACK_AUDIO_CON_LIST) {

                // Other JACK Audio connections list

                while (r->readNextStartElement()) {
                    if (r->name() == XML_PRJ_OTHERJACKCON) {

                        QString srcPort, destPort;
                        while (r->readNextStartElement()) {
                            if (r->name() == XML_PRJ_OTHERJACKCON_SRC) {
                                srcPort = r->readElementText();
                            } else if (r->name() == XML_PRJ_OTHERJACKCON_DEST) {
                                destPort = r->readElementText();
                            } else {
                                print("loadProject: "
                                      "Unrecognized JACK con element: "
                                      + r->name().toString() );
                                r->skipCurrentElement();
                            }
                        }
                        addJackAudioCon(srcPort, destPort);

                    } else {
                        print("loadProject: "
                              "Unrecognized otherJackAudioConList element: "
                              + r->name().toString() );
                        r->skipCurrentElement();
                    }
                }

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
