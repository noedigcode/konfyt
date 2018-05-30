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

#include "konfytProject.h"
#include <iostream>

konfytProject::konfytProject(QObject *parent) :
    QObject(parent)
{
    // Initialise variables
    projectName = "New Project";
    patch_id_counter = 0;
    patchListNumbers = true;
    patchListNotes = false;
    programChangeSwitchPatches = true;
    modified = false;

    // Project has to have a minimum 1 bus
    this->audioBus_add("Master Bus", NULL, NULL); // Ports will be assigned later when loading project
    // Add at least 1 MIDI input port also
    this->midiInPort_addPort("MIDI In");
}



bool konfytProject::saveProject()
{
    if (projectDirname == "") {
        return false;
    } else {
        return saveProjectAs(projectDirname);
    }
}


// Save project xml file containing a list of all the patches,
// as well as all the related patch files, in the specified directory.
bool konfytProject::saveProjectAs(QString dirname)
{
    // Directory in which the project will be saved (from parameter):
    QDir dir(dirname);
    if (!dir.exists()) {
        userMessage("saveProjectAs: Directory does not exist.");
        return false;
    }

    QString patchesPath = dirname + "/" + PROJECT_PATCH_DIR;
    QDir patchesDir(patchesPath);
    if (!patchesDir.exists()) {
        if (patchesDir.mkdir(patchesPath)) {
            userMessage("saveProjectAs: Created patches directory " + patchesPath);
        } else {
            userMessage("ERROR: saveProjectAs: Could not create patches directory.");
            return false;
        }
    }

    // Project file:
    QString filename = dirname + "/" + sanitiseFilename(projectName) + PROJECT_FILENAME_EXTENSION;
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        userMessage("saveProjectAs: Could not open file for writing: " + filename);
        return false;
    }

    this->projectDirname = dirname;

    userMessage("saveProjectAs: Project Directory: " + dirname);
    userMessage("saveProjectAs: Project filename: " + filename);


    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeComment("This is a Konfyt project.");
    stream.writeComment("Created with " + QString(APP_NAME) + " version " + APP_VERSION);

    stream.writeStartElement(XML_PRJ);
    stream.writeAttribute(XML_PRJ_NAME,this->projectName);

    // Write misc settings
    stream.writeTextElement(XML_PRJ_PATCH_LIST_NUMBERS, bool2str(patchListNumbers));
    stream.writeTextElement(XML_PRJ_PATCH_LIST_NOTES, bool2str(patchListNotes));

    // Write patches
    for (int i=0; i<patchList.count(); i++) {

        stream.writeStartElement(XML_PRJ_PATCH);
        // Patch properties
        konfytPatch* pat = patchList.at(i);
        QString patchFilename = QString(PROJECT_PATCH_DIR) + "/" + n2s(i) + "_" + sanitiseFilename(pat->getName()) + "." + KONFYT_PATCH_SUFFIX;
        stream.writeTextElement(XML_PRJ_PATCH_FILENAME, patchFilename);

        // Save the patch file in the same directory as the project file
        patchFilename = dirname + "/" + patchFilename;
        if ( !pat->savePatchToFile(patchFilename) ) {
            userMessage("ERROR: saveProjectAs: Failed to save patch " + patchFilename);
        } else {
            userMessage("saveProjectAs: Saved patch: " + patchFilename);
        }

        stream.writeEndElement();
    }

    // Write midiInPortList
    stream.writeStartElement(XML_PRJ_MIDI_IN_PORTLIST);
    QList<int> midiInIds = midiInPort_getAllPortIds();
    for (int i=0; i < midiInIds.count(); i++) {
        int id = midiInIds[i];
        PrjMidiPort p = midiInPort_getPort(id);
        stream.writeStartElement(XML_PRJ_MIDI_IN_PORT);
        stream.writeTextElement(XML_PRJ_MIDI_IN_PORT_ID, n2s(id));
        stream.writeTextElement(XML_PRJ_MIDI_IN_PORT_NAME, p.portName);
        QStringList l = p.clients;
        for (int j=0; j < l.count(); j++) {
            stream.writeTextElement(XML_PRJ_MIDI_IN_PORT_CLIENT, l.at(j));
        }
        stream.writeEndElement(); // end of port
    }
    stream.writeEndElement(); // end of midiInPortList

    // Write midiOutPortList
    stream.writeStartElement(XML_PRJ_MIDI_OUT_PORTLIST);
    QList<int> midiOutIds = midiOutPort_getAllPortIds();
    for (int i=0; i<midiOutIds.count(); i++) {
        int id = midiOutIds[i];
        PrjMidiPort p = midiOutPort_getPort(id);
        stream.writeStartElement(XML_PRJ_MIDI_OUT_PORT);
        stream.writeTextElement(XML_PRJ_MIDI_OUT_PORT_ID, n2s(id));
        stream.writeTextElement(XML_PRJ_MIDI_OUT_PORT_NAME, p.portName);
        QStringList l = p.clients;
        for (int j=0; j<l.count(); j++) {
            stream.writeTextElement(XML_PRJ_MIDI_OUT_PORT_CLIENT, l.at(j));
        }
        stream.writeEndElement(); // end of port
    }
    stream.writeEndElement(); // end of midiOutPortList

    // Write audioBusList
    stream.writeStartElement(XML_PRJ_BUSLIST);
    QList<int> busIds = audioBus_getAllBusIds();
    for (int i=0; i<busIds.count(); i++) {
        stream.writeStartElement(XML_PRJ_BUS);
        PrjAudioBus b = audioBusMap.value(busIds[i]);
        stream.writeTextElement( XML_PRJ_BUS_ID, n2s(busIds[i]) );
        stream.writeTextElement( XML_PRJ_BUS_NAME, b.busName );
        stream.writeTextElement( XML_PRJ_BUS_LGAIN, n2s(b.leftGain) );
        stream.writeTextElement( XML_PRJ_BUS_RGAIN, n2s(b.rightGain) );
        for (int j=0; j<b.leftOutClients.count(); j++) {
            stream.writeTextElement( XML_PRJ_BUS_LCLIENT, b.leftOutClients.at(j) );
        }
        for (int j=0; j<b.rightOutClients.count(); j++) {
            stream.writeTextElement( XML_PRJ_BUS_RCLIENT, b.rightOutClients.at(j) );
        }
        stream.writeEndElement(); // end of bus
    }
    stream.writeEndElement(); // end of audioBusList

    // Write audio input ports
    stream.writeStartElement(XML_PRJ_AUDIOINLIST);
    QList<int> audioInIds = audioInPort_getAllPortIds();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        PrjAudioInPort p = audioInPort_getPort(id);
        stream.writeStartElement(XML_PRJ_AUDIOIN_PORT);
        stream.writeTextElement( XML_PRJ_AUDIOIN_PORT_ID, n2s(id) );
        stream.writeTextElement( XML_PRJ_AUDIOIN_PORT_NAME, p.portName );
        stream.writeTextElement( XML_PRJ_AUDIOIN_PORT_LGAIN, n2s(p.leftGain) );
        stream.writeTextElement( XML_PRJ_AUDIOIN_PORT_RGAIN, n2s(p.rightGain) );
        stream.writeTextElement( XML_PRJ_AUDIOIN_PORT_BUS, n2s(p.destinationBus) );
        for (int j=0; j<p.leftInClients.count(); j++) {
            stream.writeTextElement( XML_PRJ_AUDIOIN_PORT_LCLIENT, p.leftInClients.at(j) );
        }
        for (int j=0; j<p.rightInClients.count(); j++) {
            stream.writeTextElement( XML_PRJ_AUDIOIN_PORT_RCLIENT, p.rightInClients.at(j) );
        }
        stream.writeEndElement(); // end of port
    }
    stream.writeEndElement(); // end of audioInputPortList

    // Write process list (external applications)
    stream.writeStartElement(XML_PRJ_PROCESSLIST);
    for (int i=0; i<processList.count(); i++) {
        stream.writeStartElement(XML_PRJ_PROCESS);
        konfytProcess* gp = processList.at(i);
        stream.writeTextElement(XML_PRJ_PROCESS_APPNAME, gp->appname);
        stream.writeTextElement(XML_PRJ_PROCESS_DIR, gp->dir); // TODO: NOT USED
        for (int j=0; j<gp->args.count(); j++) {
            stream.writeTextElement(XML_PRJ_PROCESS_ARG, gp->args.at(j)); // TODO: NOT USED
        }
        stream.writeEndElement(); // end of process
    }
    stream.writeEndElement(); // end of processList

    // Write trigger list
    stream.writeStartElement(XML_PRJ_TRIGGERLIST);
    stream.writeTextElement(XML_PRJ_PROG_CHANGE_SWITCH_PATCHES, bool2str(programChangeSwitchPatches));
    QList<KonfytTrigger> trigs = triggerHash.values();
    for (int i=0; i<trigs.count(); i++) {
        stream.writeStartElement(XML_PRJ_TRIGGER);
        stream.writeTextElement(XML_PRJ_TRIGGER_ACTIONTEXT, trigs[i].actionText);
        stream.writeTextElement(XML_PRJ_TRIGGER_TYPE, n2s(trigs[i].type) );
        stream.writeTextElement(XML_PRJ_TRIGGER_CHAN, n2s(trigs[i].channel) );
        stream.writeTextElement(XML_PRJ_TRIGGER_DATA1, n2s(trigs[i].data1) );
        stream.writeTextElement(XML_PRJ_TRIGGER_BANKMSB, n2s(trigs[i].bankMSB) );
        stream.writeTextElement(XML_PRJ_TRIGGER_BANKLSB, n2s(trigs[i].bankLSB) );
        stream.writeEndElement(); // end of trigger
    }
    stream.writeEndElement(); // end of trigger list

    // Write other JACK connections list
    stream.writeStartElement(XML_PRJ_OTHERJACKCON_LIST);
    for (int i=0; i<jackConList.count(); i++) {
        stream.writeStartElement(XML_PRJ_OTHERJACKCON);
        stream.writeTextElement(XML_PRJ_OTHERJACKCON_SRC, jackConList[i].srcPort);
        stream.writeTextElement(XML_PRJ_OTHERJACKCON_DEST, jackConList[i].destPort);
        stream.writeEndElement(); // JACK connection pair
    }
    stream.writeEndElement(); // Other JACK connections list

    stream.writeEndElement(); // project

    stream.writeEndDocument();

    file.close();

    setModified(false);

    return true;
}

// Load project xml file (containing list of all the patches) and
// load all the patch files from the same directory.
bool konfytProject::loadProject(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        userMessage("loadProject: Could not open file for reading.");
        return false;
    }



    QFileInfo fi(file);
    QDir dir = fi.dir(); // Get file parent directory
    this->projectDirname = dir.path();
    userMessage("loadProject: Loading project file " + filename);
    userMessage("loadProject: in dir " + dir.path());

    QXmlStreamReader r(&file);
    r.setNamespaceProcessing(false);

    QString patchFilename;
    patchList.clear();
    midiAutoConnectList.clear();
    midiOutPortMap.clear();
    processList.clear();
    audioBusMap.clear();
    audioInPortMap.clear();

    while (r.readNextStartElement()) { // project

        // Get the project name attribute
        QXmlStreamAttributes ats =  r.attributes();
        if (ats.count()) {
            this->projectName = ats.at(0).value().toString(); // Project name
        }

        while (r.readNextStartElement()) {

            if (r.name() == XML_PRJ_PATCH) { // patch

                while (r.readNextStartElement()) { // patch properties

                    if (r.name() == XML_PRJ_PATCH_FILENAME) {
                        patchFilename = r.readElementText();
                    } else {
                        userMessage("loadProject: "
                                    "Unrecognized patch element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }

                }

                // Add new patch
                konfytPatch* pt = new konfytPatch();
                patchFilename = dir.path() + "/" + patchFilename;
                userMessage("loadProject: Loading patch " + patchFilename);
                if (pt->loadPatchFromFile(patchFilename)) {
                    this->addPatch(pt);
                } else {
                    // Error message on loading patch.
                    userMessage("loadProject: Error loading patch: " + patchFilename);
                }

            } else if (r.name() == XML_PRJ_PATCH_LIST_NUMBERS) {

                patchListNumbers = Qstr2bool(r.readElementText());

            } else if (r.name() == XML_PRJ_PATCH_LIST_NOTES) {

                patchListNotes = Qstr2bool(r.readElementText());

            } else if (r.name() == XML_PRJ_MIDI_IN_PORTLIST) {

                while (r.readNextStartElement()) { // port
                    PrjMidiPort p;
                    int id = midiInPort_getUniqueId();
                    while (r.readNextStartElement()) {
                        if (r.name() == XML_PRJ_MIDI_IN_PORT_ID) {
                            id = r.readElementText().toInt();
                        } else if (r.name == XML_PRJ_MIDI_IN_PORT_NAME) {
                            p.portName = r.readElementText();
                        } else if (r.name == XML_PRJ_MIDI_IN_PORT_CLIENT) {
                            p.clients.append( r.readElementText() );
                        } else {
                            userMessage("loadProject: "
                                        "Unrecognized midiInPortList port element: " + r.name().toString() );
                        }
                        if (midiInPortMap.contains(id)) {
                            userMessage("loadProject: "
                                        "Duplicate midi in port id detected: " + n2s(id));
                        }
                        this->midiInPortMap.insert(id, p);
                    }
                }

            } else if (r.name() == XML_PRJ_MIDI_OUT_PORTLIST) {

                while (r.readNextStartElement()) { // port
                    PrjMidiPort p;
                    int id = midiOutPort_getUniqueId();
                    while (r.readNextStartElement()) {
                        if (r.name() == XML_PRJ_MIDI_OUT_PORT_ID) {
                            id = r.readElementText().toInt();
                        } else if (r.name() == XML_PRJ_MIDI_OUT_PORT_NAME) {
                            p.portName = r.readElementText();
                        } else if (r.name() == XML_PRJ_MIDI_OUT_PORT_CLIENT) {
                            p.clients.append( r.readElementText() );
                        } else {
                            userMessage("loadProject: "
                                        "Unrecognized midiOutPortList port element: " + r.name().toString() );
                            r.skipCurrentElement();
                        }
                    }
                    if (midiOutPortMap.contains(id)) {
                        userMessage("loadProject: "
                                    "Duplicate midi out port id detected: " + n2s(id));
                    }
                    this->midiOutPortMap.insert(id, p);
                }

            } else if (r.name() == XML_PRJ_BUSLIST) {

                while (r.readNextStartElement()) { // bus
                    PrjAudioBus b;
                    int id = audioBus_getUniqueId();
                    while (r.readNextStartElement()) {
                        if (r.name() == XML_PRJ_BUS_ID) {
                            id = r.readElementText().toInt();
                        } else if (r.name() == XML_PRJ_BUS_NAME) {
                            b.busName = r.readElementText();
                        } else if (r.name() == XML_PRJ_BUS_LGAIN) {
                            b.leftGain = r.readElementText().toFloat();
                        } else if (r.name() == XML_PRJ_BUS_RGAIN) {
                            b.rightGain = r.readElementText().toFloat();
                        } else if (r.name() == XML_PRJ_BUS_LCLIENT) {
                            b.leftOutClients.append( r.readElementText() );
                        } else if (r.name() == XML_PRJ_BUS_RCLIENT) {
                            b.rightOutClients.append( r.readElementText() );
                        } else {
                            userMessage("loadProject: "
                                        "Unrecognized bus element: " + r.name().toString() );
                            r.skipCurrentElement();
                        }
                    }
                    if (audioBusMap.contains(id)) {
                        userMessage("loadProject: "
                                    "Duplicate bus id detected: " + n2s(id));
                    }
                    this->audioBusMap.insert(id, b);
                }

            } else if (r.name() == XML_PRJ_AUDIOINLIST) {

                while (r.readNextStartElement()) { // port
                    PrjAudioInPort p;
                    int id = audioInPort_getUniqueId();
                    while (r.readNextStartElement()) {
                        if (r.name() == XML_PRJ_AUDIOIN_PORT_ID) {
                            id = r.readElementText().toInt();
                        } else if (r.name() == XML_PRJ_AUDIOIN_PORT_NAME) {
                            p.portName = r.readElementText();
                        } else if (r.name() == XML_PRJ_AUDIOIN_PORT_LGAIN) {
                            p.leftGain = r.readElementText().toFloat();
                        } else if (r.name() == XML_PRJ_AUDIOIN_PORT_RGAIN) {
                            p.rightGain = r.readElementText().toFloat();
                        } else if (r.name() == XML_PRJ_AUDIOIN_PORT_BUS) {
                            p.destinationBus = r.readElementText().toInt();
                        } else if (r.name() == XML_PRJ_AUDIOIN_PORT_LCLIENT) {
                            p.leftInClients.append( r.readElementText() );
                        } else if (r.name() == XML_PRJ_AUDIOIN_PORT_RCLIENT) {
                            p.rightInClients.append( r.readElementText() );
                        } else {
                            userMessage("loadProject: "
                                        "Unrecognized audio input port element: " + r.name().toString() );
                            r.skipCurrentElement();
                        }
                    }
                    if (audioInPortMap.contains(id)) {
                        userMessage("loadProject: "
                                    "Duplicate audio in port id detected: " + n2s(id));
                    }
                    this->audioInPortMap.insert(id, p);
                }

            } else if (r.name() == XML_PRJ_PROCESSLIST) {

                while (r.readNextStartElement()) { // process
                    konfytProcess* gp = new konfytProcess();
                    while (r.readNextStartElement()) {
                        if (r.name() == XML_PRJ_PROCESS_APPNAME) {
                            gp->appname = r.readElementText();
                        } else if (r.name() == XML_PRJ_PROCESS_DIR) { // todo unused
                            gp->dir = r.readElementText();
                        } else if (r.name() == XML_PRJ_PROCESS_ARG) { // todo unused
                            gp->args.append(r.readElementText());
                        } else {
                            userMessage("loadProject: "
                                        "Unrecognized process element: " + r.name().toString() );
                            r.skipCurrentElement();
                        }
                    }
                    gp->projectDir = this->getDirname();
                    this->addProcess(gp);
                }


            } else if (r.name() == XML_PRJ_TRIGGERLIST) {

                while (r.readNextStartElement()) {

                    if (r.name() == XML_PRJ_PROG_CHANGE_SWITCH_PATCHES) {
                        programChangeSwitchPatches = Qstr2bool(r.readElementText());
                    } else if (r.name() == XML_PRJ_TRIGGER) {

                        KonfytTrigger trig;
                        while (r.readNextStartElement()) {
                            if (r.name() == XML_PRJ_TRIGGER_ACTIONTEXT) {
                                trig.actionText = r.readElementText();
                            } else if (r.name() == XML_PRJ_TRIGGER_TYPE) {
                                trig.type = r.readElementText().toInt();
                            } else if (r.name() == XML_PRJ_TRIGGER_CHAN) {
                                trig.channel = r.readElementText().toInt();
                            } else if (r.name() == XML_PRJ_TRIGGER_DATA1) {
                                trig.data1 = r.readElementText().toInt();
                            } else if (r.name() == XML_PRJ_TRIGGER_BANKMSB) {
                                trig.bankMSB = r.readElementText().toInt();
                            } else if (r.name() == XML_PRJ_TRIGGER_BANKLSB) {
                                trig.bankLSB = r.readElementText().toInt();
                            } else {
                                userMessage("loadProject: "
                                            "Unrecognized trigger element: " + r.name().toString() );
                                r.skipCurrentElement();
                            }
                        }
                        this->addAndReplaceTrigger(trig);

                    } else {
                        userMessage("loadProject: "
                                    "Unrecognized triggerList element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

            }

            else if (r.name() == XML_PRJ_OTHERJACKCON_LIST) {

                while (r.readNextStartElement()) {
                    if (r.name() == XML_PRJ_OTHERJACKCON) {

                        QString srcPort, destPort;
                        while (r.readNextStartElement()) {
                            if (r.name() == XML_PRJ_OTHERJACKCON_SRC) {
                                srcPort = r.readElementText();
                            } else if (r.name() == XML_PRJ_OTHERJACKCON_DEST) {
                                destPort = r.readElementText();
                            } else {
                                userMessage("loadProject: "
                                            "Unrecognized JACK con element: " + r.name().toString() );
                                r.skipCurrentElement();
                            }
                        }
                        this->addJackCon(srcPort, destPort);

                    } else {
                        userMessage("loadProject: "
                                    "Unrecognized otherJackConList element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

            }
            else {
                userMessage("loadProject: "
                            "Unrecognized project element: " + r.name().toString() );
                r.skipCurrentElement();
            }
        }
    }


    file.close();

    // Check if we have at least one audio output bus. If not, create a default one.
    if (audioBus_count() == 0) {
        // Project has to have a minimum 1 bus
        this->audioBus_add("Master Bus", NULL, NULL); // Ports will be assigned later when loading project
    }

    setModified(false);

    return true;
}

void konfytProject::setProjectName(QString newName)
{
    projectName = newName;
    setModified(true);
}

bool konfytProject::getShowPatchListNumbers()
{
    return patchListNumbers;
}

void konfytProject::setShowPatchListNumbers(bool show)
{
    patchListNumbers = show;
    setModified(true);
}

bool konfytProject::getShowPatchListNotes()
{
    return patchListNotes;
}

void konfytProject::setShowPatchListNotes(bool show)
{
    patchListNotes = show;
    setModified(true);
}

QString konfytProject::getProjectName()
{
    return projectName;
}

void konfytProject::addPatch(konfytPatch *newPatch)
{
    // Set unique ID, so we can later identify this patch.
    newPatch->id_in_project = this->patch_id_counter++;

    patchList.append(newPatch);
    setModified(true);
}


// Removes the patch from project and returns the pointer.
// Note that the pointer has not been freed.
// Returns NULL if index out of bounds.
konfytPatch *konfytProject::removePatch(int i)
{
    if ( (i>=0) && (i<patchList.count())) {
        konfytPatch* patch = patchList[i];
        patchList.removeAt(i);
        setModified(true);
        return patch;
    } else {
        return NULL;
    }
}



void konfytProject::movePatchUp(int i)
{
    if ( (i>=1) && (i<patchList.count())) {
        patchList.move(i,i-1);
        setModified(true);
    }
}

void konfytProject::movePatchDown(int i)
{
    if ( (i>=0) && (i<patchList.count()-1) ) {
        patchList.move(i, i+1);
        setModified(true);
    }
}

konfytPatch *konfytProject::getPatch(int i)
{
    if ( (i>=0) && (i<patchList.count())) {
        return patchList.at(i);
    } else {
        return NULL;
    }
}

QList<konfytPatch *> konfytProject::getPatchList()
{
    return patchList;
}

int konfytProject::getNumPatches()
{
    return patchList.count();
}

QString konfytProject::getDirname()
{
    return projectDirname;
}

void konfytProject::setDirname(QString newDirname)
{
    projectDirname = newDirname;
    setModified(true);
}

QList<int> konfytProject::midiInPort_getAllPortIds()
{
    return midiInPortMap.keys();
}

int konfytProject::midiInPort_addPort(QString portName)
{
    PrjMidiPort p;
    p.portName = portName;

    int portId = midiInPort_getUniqueId();
    midiInPortMap.insert(portId, p);

    setModified(true);

    return portId;
}

void konfytProject::midiInPort_removePort(int portId)
{
    if (midiInPort_exists(portId)) {
        midiInPortMap.remove(portId);
        setModified(true);
    } else {
        error_abort("midiInPort_removePort: port with id " + n2s(portId) + " does not exist.");
    }
}

bool konfytProject::midiInPort_exists(int portId)
{
    return midiInPortMap.contains(portId);
}

PrjMidiPort konfytProject::midiInPort_getPort(int portId)
{
    if (midiInPort_exists(portId)) {
        return midiInPortMap.value(portId);
    } else {
        error_abort("midiInPort_getPort: port with id " + n2s(portId) + " does not exist.");
    }
}

int konfytProject::midiInPort_count()
{
    return midiInPortMap.count();
}

void konfytProject::midiInPort_replace(int portId, PrjMidiPort port)
{
    midiInPort_replace_noModify(portId, port);
    setModified(true);
}

void konfytProject::midiInPort_replace_noModify(int portId, PrjMidiPort port)
{
    if (midiInPort_exists(portId)) {
        midiInPortMap.insert(portId, port);
    } else {
        error_abort("midiInPort_replace_noModify: Port with id " + n2s(portId) + " does not exist.");
    }
}

QStringList konfytProject::midiInPort_getClients(int portId)
{
    if (midiInPort_exists(portId)) {
        return midiInPortMap.value(portId).clients;
    } else {
        error_abort("midiInPort_getClients: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::midiInPort_addClient(int portId, QString client)
{
    if (midiInPort_exists(portId)) {
        PrjMidiPort p = midiInPortMap.value(portId);
        p.clients.append(client);
        midiInPortMap.insert(portId, p);
        setModified(true);
    } else {
        error_abort("midiInPort_addClient: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::midiInPort_removeClient(int portId, QString client)
{
    if (midiInPort_exists(portId)) {
        PrjMidiPort p = midiInPortMap.value(portId);
        p.clients.removeAll(client);
        midiInPortMap.insert(portId, p);
        setModified(true);
    } else {
        error_abort("midiInPort_removeClient: port with id " + n2s(portId) + " does not exist.");
    }
}

int konfytProject::midiOutPort_addPort(QString portName)
{
    PrjMidiPort p;
    p.portName = portName;

    int portId = midiOutPort_getUniqueId();
    midiOutPortMap.insert(portId, p);

    setModified(true);

    return portId;
}

int konfytProject::audioBus_getUniqueId()
{
    QList<int> l = audioBusMap.keys();
    return getUniqueIdHelper( l );
}

int konfytProject::midiInPort_getUniqueId()
{
    QList<int> l = midiInPortMap.keys();
    return getUniqueIdHelper( l );
}

int konfytProject::midiOutPort_getUniqueId()
{
    QList<int> l = midiOutPortMap.keys();
    return getUniqueIdHelper( l );
}

int konfytProject::audioInPort_getUniqueId()
{
    QList<int> l = audioInPortMap.keys();
    return getUniqueIdHelper( l );
}

int konfytProject::getUniqueIdHelper(QList<int> ids)
{
    /* Given the list of ids, find the highest id and add one. */
    int id=-1;
    for (int i=0; i<ids.count(); i++) {
        if (ids[i]>id) { id = ids[i]; }
    }
    return id+1;
}

/* Adds bus and returns unique bus id. */
int konfytProject::audioBus_add(QString busName, konfytJackPort* leftJackPort, konfytJackPort* rightJackPort)
{
    PrjAudioBus bus;
    bus.busName = busName;
    bus.leftGain = 1;
    bus.rightGain = 1;
    bus.leftJackPort = leftJackPort;
    bus.rightJackPort = rightJackPort;

    int busId = audioBus_getUniqueId();
    audioBusMap.insert(busId, bus);

    setModified(true);

    return busId;
}

void konfytProject::audioBus_remove(int busId)
{
    if (audioBusMap.contains(busId)) {
        audioBusMap.remove(busId);
        setModified(true);
    } else {
        error_abort("audioBus_remove: bus with id " + n2s(busId) + " does not exist.");
    }
}

int konfytProject::audioBus_count()
{
    return audioBusMap.count();
}

bool konfytProject::audioBus_exists(int busId)
{
    return audioBusMap.contains(busId);
}

PrjAudioBus konfytProject::audioBus_getBus(int busId)
{
    if (audioBusMap.contains(busId)) {
        return audioBusMap.value(busId);
    } else {
        error_abort("audioBus_getBus: bus with id " + n2s(busId) + " does not exist.");
    }
}

/* Gets the first bus Id that is not skipId. */
int konfytProject::audioBus_getFirstBusId(int skipId)
{
    int ret = -1;
    QList<int> l = audioBusMap.keys();
    if (l.count()) {
        for (int i=0; i<l.count(); i++) {
            if (l[i] != skipId) {
                ret = l[i];
                break;
            }
        }
        return ret;
    } else {
        error_abort("audioBus_getFirstBusId: no busses in project!");
        return -1;
    }
}

QList<int> konfytProject::audioBus_getAllBusIds()
{
    return audioBusMap.keys();
}

void konfytProject::audioBus_replace(int busId, PrjAudioBus newBus)
{
    audioBus_replace_noModify(busId, newBus);
    setModified(true);
}

/* Do not change the project's modified state. */
void konfytProject::audioBus_replace_noModify(int busId, PrjAudioBus newBus)
{
    if (audioBusMap.contains(busId)) {
        audioBusMap.insert(busId, newBus);
    } else {
        error_abort("audioBus_replace: bus with id " + n2s(busId) + " does not exist.");
    }
}


void konfytProject::audioBus_addClient(int busId, portLeftRight leftRight, QString client)
{
    if (audioBusMap.contains(busId)) {
        PrjAudioBus b = audioBusMap.value(busId);
        if (leftRight == leftPort) {
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
    } else {
        error_abort("audioBus_addClient: bus with id " + n2s(busId) + " does not exist.");
    }

}

void konfytProject::audioBus_removeClient(int busId, portLeftRight leftRight, QString client)
{
    if (audioBusMap.contains(busId)) {
        PrjAudioBus b = audioBusMap.value(busId);
        if (leftRight == leftPort) {
            b.leftOutClients.removeAll(client);
        } else {
            b.rightOutClients.removeAll(client);
        }
        audioBusMap.insert(busId, b);
        setModified(true);
    } else {
        error_abort("audioBus_removeClient: bus with id " + n2s(busId) + " does not exist.");
    }

}

QList<int> konfytProject::audioInPort_getAllPortIds()
{
    return audioInPortMap.keys();
}

int konfytProject::audioInPort_add(QString portName)
{
    PrjAudioInPort port;
    port.portName = portName;
    port.destinationBus = 0;
    port.leftGain = 1;
    port.leftJackPort = NULL;
    port.rightJackPort = NULL;
    port.rightGain = 1;

    int portId = audioInPort_getUniqueId();
    audioInPortMap.insert(portId, port);

    setModified(true);

    return portId;
}

void konfytProject::audioInPort_remove(int portId)
{
    if (audioInPortMap.contains(portId)) {
        audioInPortMap.remove(portId);
        setModified(true);
    } else {
        error_abort("audioInPorts_remove: port with id " + n2s(portId) + " does not exist.");
    }
}

int konfytProject::audioInPort_count()
{
    return audioInPortMap.count();
}

bool konfytProject::audioInPort_exists(int portId)
{
    return audioInPortMap.contains(portId);
}

PrjAudioInPort konfytProject::audioInPort_getPort(int portId)
{
    if (audioInPortMap.contains(portId)) {
        return audioInPortMap.value(portId);
    } else {
        error_abort("audioInPorts_getPort: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::audioInPort_replace(int portId, PrjAudioInPort newPort)
{
    audioInPort_replace_noModify(portId, newPort);
    setModified(true);
}

/* Do not change the project's modify state. */
void konfytProject::audioInPort_replace_noModify(int portId, PrjAudioInPort newPort)
{
    if (audioInPortMap.contains(portId)) {
        audioInPortMap.insert(portId, newPort);
    } else {
        error_abort("audioInPort_replace_noModify: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::audioInPort_addClient(int portId, portLeftRight leftRight, QString client)
{
    if (audioInPortMap.contains(portId)) {
        PrjAudioInPort p = audioInPortMap.value(portId);
        if (leftRight == leftPort) {
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
    } else {
        error_abort("audioInPorts_addClient: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::audioInPort_removeClient(int portId, portLeftRight leftRight, QString client)
{
    if (audioInPortMap.contains(portId)) {
        PrjAudioInPort p = audioInPortMap.value(portId);
        if (leftRight == leftPort) {
            p.leftInClients.removeAll(client);
        } else {
            p.rightInClients.removeAll(client);
        }
        audioInPortMap.insert(portId, p);
        setModified(true);
    } else {
        error_abort("audioInPorts_removeClient: port with id " + n2s(portId) + " does not exist.");
    }

}

void konfytProject::midiOutPort_removePort(int portId)
{
    if (midiOutPortMap.contains(portId)) {
        midiOutPortMap.remove(portId);
        setModified(true);
    } else {
        error_abort("midiOutPort_removePort: port with id " + n2s(portId) + " does not exist.");
    }
}

int konfytProject::midiOutPort_count()
{
    return midiOutPortMap.count();
}

bool konfytProject::midiOutPort_exists(int portId)
{
    return midiOutPortMap.contains(portId);
}

PrjMidiPort konfytProject::midiOutPort_getPort(int portId)
{
    if (midiOutPortMap.contains(portId)) {
        return midiOutPortMap.value(portId);
    } else {
        error_abort("midiOutPort_getPort: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::midiOutPort_addClient(int portId, QString client)
{
    if (midiOutPortMap.contains(portId)) {
        PrjMidiPort p = midiOutPortMap.value(portId);
        p.clients.append(client);
        midiOutPortMap.insert(portId, p);
        setModified(true);
    } else {
        error_abort("midiOutPort_addClient: port with id " + n2s(portId) + " does not exist.");
    }
}


void konfytProject::midiOutPort_removeClient(int portId, QString client)
{
    if (midiOutPortMap.contains(portId)) {
        PrjMidiPort p = midiOutPortMap.value(portId);
        p.clients.removeAll(client);
        midiOutPortMap.insert(portId, p);
        setModified(true);
    } else {
        error_abort("midiOutPort_removeClient: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::midiOutPort_replace(int portId, PrjMidiPort port)
{
    midiOutPort_replace_noModify(portId, port);
    setModified(true);
}

/* Do not change the project's modify state. */
void konfytProject::midiOutPort_replace_noModify(int portId, PrjMidiPort port)
{
    if (midiOutPortMap.contains(portId)) {
        midiOutPortMap.insert(portId, port);
    } else {
        error_abort("midiOutPort_replace_noModify: Port with id " + n2s(portId) + " does not exist.");
    }
}

QList<int> konfytProject::midiOutPort_getAllPortIds()
{
    return midiOutPortMap.keys();
}

QStringList konfytProject::midiOutPort_getClients(int portId)
{
    if (midiOutPortMap.contains(portId)) {
        return midiOutPortMap.value(portId).clients;
    } else {
        error_abort("midiOutPort_getClients: port with id " + n2s(portId) + " does not exist.");
    }
}

// Add process (External program) to GUI and current project.
void konfytProject::addProcess(konfytProcess* process)
{
    // Add to internal list
    process->projectDir = this->getDirname();
    processList.append(process);
    // Connect signals
    connect(process, SIGNAL(started(konfytProcess*)), this, SLOT(processStartedSlot(konfytProcess*)));
    connect(process, SIGNAL(finished(konfytProcess*)), this, SLOT(processFinishedSlot(konfytProcess*)));
    setModified(true);
}

bool konfytProject::isProcessRunning(int index)
{
    if ( (index>=0) && (index < processList.count()) ) {
        return processList.at(index)->isRunning();
    } else {
        userMessage("ERROR: isProcessRunning: INVALID PROCESS INDEX " + n2s(index));
    }
}

void konfytProject::runProcess(int index)
{
    if ( (index>=0) && (index < processList.count()) ) {
        // Start process
        processList.at(index)->start();
        userMessage("Starting process " + processList.at(index)->appname);
    } else {
        userMessage("ERROR: runProcess: INVALID PROCESS INDEX " + n2s(index));
    }
}

void konfytProject::stopProcess(int index)
{
    if ( (index>=0) && (index < processList.count()) ) {
        // Stop process
        processList.at(index)->stop();
        userMessage("Stopping process " + processList.at(index)->appname);
    } else {
        userMessage("ERROR: stopProcess: INVALID PROCESS INDEX " + n2s(index));
    }
}

void konfytProject::removeProcess(int index)
{
    if ( (index>=0) && (index < processList.count()) ) {
        processList.removeAt(index);
        setModified(true);
    } else {
        userMessage("ERROR: removeProcess: INVALID PROCESS INDEX " + n2s(index));
    }
}

// Slot for signal from Process object, when the process was started.
void konfytProject::processStartedSlot(konfytProcess *process)
{
    int index = processList.indexOf(process);
    processStartedSignal(index, process);
}

void konfytProject::processFinishedSlot(konfytProcess *process)
{
    int index = processList.indexOf(process);
    processFinishedSignal(index, process);
}

QList<konfytProcess*> konfytProject::getProcessList()
{
    return processList;
}

// Used to determine whether patches are loaded. Uses unique patch id.
// This addes the patch id to the loaded_patch_ids list.
void konfytProject::markPatchLoaded(int patch_id)
{
    this->loaded_patch_ids.append(patch_id);
}

// Used to determine whether patches are loaded. Uses unique patch id.
bool konfytProject::isPatchLoaded(int patch_id)
{
    return this->loaded_patch_ids.contains(patch_id);
}

void konfytProject::addAndReplaceTrigger(KonfytTrigger newTrigger)
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

void konfytProject::removeTrigger(QString actionText)
{
    if (triggerHash.contains(actionText)) {
        triggerHash.remove(actionText);
        setModified(true);
    }
}

QList<KonfytTrigger> konfytProject::getTriggerList()
{
    return triggerHash.values();
}

bool konfytProject::isProgramChangeSwitchPatches()
{
    return programChangeSwitchPatches;
}

void konfytProject::setProgramChangeSwitchPatches(bool value)
{
    programChangeSwitchPatches = value;
    setModified(true);
}

konfytJackConPair konfytProject::addJackCon(QString srcPort, QString destPort)
{
    konfytJackConPair a;
    a.srcPort = srcPort;
    a.destPort = destPort;
    this->jackConList.append(a);
    setModified(true);
    return a;
}

QList<konfytJackConPair> konfytProject::getJackConList()
{
    return this->jackConList;
}

konfytJackConPair konfytProject::removeJackCon(int i)
{
    if ( (i<0) || (i>= jackConList.count()) ) {
        error_abort("Invalid other JACK connection index: " + n2s(i));
        return;
    }
    konfytJackConPair p = jackConList[i];
    jackConList.removeAt(i);
    setModified(true);
    return p;
}

void konfytProject::setModified(bool mod)
{
    this->modified = mod;
    emit projectModifiedChanged(mod);
}

bool konfytProject::isModified()
{
    return this->modified;
}


// Print error message to stdout, and abort app.
void konfytProject::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: konfytProject:" << msg.toLocal8Bit().constData();
    abort();
}



