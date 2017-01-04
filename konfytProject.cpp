#include "konfytProject.h"
#include <iostream>

konfytProject::konfytProject(QObject *parent) :
    QObject(parent)
{
    // Initialise variables
    projectName = "New Project";
    patch_id_counter = 0;
    modified = false;

    // Project has to have a minimum 1 bus
    this->audioBus_add("Master Bus", NULL, NULL); // Ports will be assigned later when loading project
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
        userMessage("sfproject saveProjectAs: Directory does not exist.");
        return false;
    }

    QString patchesPath = dirname + "/" + PROJECT_PATCH_DIR;
    QDir patchesDir(patchesPath);
    if (!patchesDir.exists()) {
        if (patchesDir.mkdir(patchesPath)) {
            userMessage("Created patches directory " + patchesPath);
        } else {
            userMessage("ERROR: Could not create patches directory.");
            return false;
        }
    }

    // Project file:
    QString filename = dirname + "/" + projectName + PROJECT_FILENAME_EXTENSION;
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        userMessage("sfproject saveProjectAs: Could not open file for writing: " + filename);
        return false;
    }

    this->projectDirname = dirname;

    userMessage("sfproject saveProjectAs: Project Directory: " + dirname);
    userMessage("sfproject saveProjectAs: Project filename: " + filename);


    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeComment("This is a Konfyt project.");
    stream.writeComment("Created with " + QString(APP_NAME) + " version " + n2s(APP_VERSION));

    stream.writeStartElement("sfproject");
    stream.writeAttribute("name",this->projectName);

    // Write patches
    for (int i=0; i<patchList.count(); i++) {

        stream.writeStartElement("patch");
        // Patch properties
        konfytPatch* pat = patchList.at(i);
        QString patchFilename = QString(PROJECT_PATCH_DIR) + "/" + n2s(i) + "_" + pat->getName();
        stream.writeTextElement("filename", patchFilename);

        // Save the patch file in the same directory as the project file
        patchFilename = dirname + "/" + patchFilename;
        if ( !pat->savePatchToFile(patchFilename) ) {
            userMessage("ERROR: Failed to save patch " + patchFilename);
        } else {
            userMessage("sfproject saveProjectAs: Saved patch: " + patchFilename);
        }

        stream.writeEndElement();
    }

    // Write midiAutoConnectList
    stream.writeStartElement("midiAutoConnectList");
    for (int i=0; i<midiAutoConnectList.count(); i++) {
        stream.writeTextElement("port", midiAutoConnectList.at(i));
    }
    stream.writeEndElement();

    // Write midiOutPortList
    stream.writeStartElement("midiOutPortList");
    QList<int> midiOutIds = midiOutPort_getAllPortIds();
    for (int i=0; i<midiOutIds.count(); i++) {
        int id = midiOutIds[i];
        prjMidiOutPort p = midiOutPort_getPort(id);
        stream.writeStartElement("port");
        stream.writeTextElement("portId", n2s(id));
        stream.writeTextElement("portName", p.portName);
        QStringList l = p.clients;
        for (int j=0; j<l.count(); j++) {
            stream.writeTextElement("client", l.at(j));
        }
        stream.writeEndElement(); // end of port
    }
    stream.writeEndElement(); // end of midiOutPortList

    // Write audioBusList
    stream.writeStartElement("audioBusList");
    QList<int> busIds = audioBus_getAllBusIds();
    for (int i=0; i<busIds.count(); i++) {
        stream.writeStartElement("bus");
        prjAudioBus b = audioBusMap.value(busIds[i]);
        stream.writeTextElement( "busId", n2s(busIds[i]) );
        stream.writeTextElement( "busName", b.busName );
        stream.writeTextElement( "leftGain", n2s(b.leftGain) );
        stream.writeTextElement( "rightGain", n2s(b.rightGain) );
        for (int j=0; j<b.leftOutClients.count(); j++) {
            stream.writeTextElement( "leftClient", b.leftOutClients.at(j) );
        }
        for (int j=0; j<b.rightOutClients.count(); j++) {
            stream.writeTextElement( "rightClient", b.rightOutClients.at(j) );
        }
        stream.writeEndElement(); // end of bus
    }
    stream.writeEndElement(); // end of audioBusList

    // Write audio input ports
    stream.writeStartElement("audioInputPortList");
    QList<int> audioInIds = audioInPort_getAllPortIds();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        prjAudioInPort p = audioInPort_getPort(id);
        stream.writeStartElement("port");
        stream.writeTextElement( "portId", n2s(id) );
        stream.writeTextElement( "portName", p.portName );
        stream.writeTextElement( "leftGain", n2s(p.leftGain) );
        stream.writeTextElement( "rightGain", n2s(p.rightGain) );
        stream.writeTextElement( "destinationBus", n2s(p.destinationBus) );
        for (int j=0; j<p.leftInClients.count(); j++) {
            stream.writeTextElement( "leftClient", p.leftInClients.at(j) );
        }
        for (int j=0; j<p.rightInClients.count(); j++) {
            stream.writeTextElement( "rightClient", p.rightInClients.at(j) );
        }
        stream.writeEndElement(); // end of port
    }
    stream.writeEndElement(); // end of audioInputPortList

    // Write process list (external applications)
    stream.writeStartElement("processList");
    for (int i=0; i<processList.count(); i++) {
        stream.writeStartElement("process");
        konfytProcess* gp = processList.at(i);
        stream.writeTextElement("appname", gp->appname);
        stream.writeTextElement("dir", gp->dir);
        for (int j=0; j<gp->args.count(); j++) {
            stream.writeTextElement("argument", gp->args.at(j));
        }
        stream.writeEndElement(); // end of process
    }
    stream.writeEndElement(); // end of processList

    // Write trigger list
    stream.writeStartElement("triggerList");
    QList<konfytTrigger> trigs = triggerHash.values();
    for (int i=0; i<trigs.count(); i++) {
        stream.writeStartElement("trigger");
        stream.writeTextElement("actionText", trigs[i].actionText);
        stream.writeTextElement("type", n2s(trigs[i].type) );
        stream.writeTextElement("channel", n2s(trigs[i].channel) );
        stream.writeTextElement("data1", n2s(trigs[i].data1) );
        stream.writeTextElement("bankMSB", n2s(trigs[i].bankMSB) );
        stream.writeTextElement("bankLSB", n2s(trigs[i].bankLSB) );
        stream.writeEndElement(); // end of trigger
    }
    stream.writeEndElement(); // end of trigger list

    stream.writeEndElement(); // sfproject

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
        userMessage("sfproject loadProject: Could not open file for reading.");
        return false;
    }



    QFileInfo fi(file);
    QDir dir = fi.dir(); // Get file parent directory
    this->projectDirname = dir.path();
    userMessage("sfproject loadProject: Loading project file " + filename);
    userMessage("sfproject loadProject: in dir " + dir.path());

    QXmlStreamReader r(&file);
    r.setNamespaceProcessing(false);

    QString patchFilename;
    patchList.clear();
    midiAutoConnectList.clear();
    midiOutPortMap.clear();
    processList.clear();
    audioBusMap.clear();
    audioInPortMap.clear();

    while (r.readNextStartElement()) { // sfproject

        // Get the project name attribute
        QXmlStreamAttributes ats =  r.attributes();
        if (ats.count()) {
            this->projectName = ats.at(0).value().toString();
        }

        while (r.readNextStartElement()) { // patch

            if (r.name() == "patch") {

                while (r.readNextStartElement()) { // patch properties

                    if (r.name() == "filename") {
                        patchFilename = r.readElementText();
                    } else {
                        userMessage("sfproject::loadProject: "
                                    "Unrecognized patch element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }

                }

                // Add new patch
                konfytPatch* pt = new konfytPatch();
                patchFilename = dir.path() + "/" + patchFilename;
                userMessage("sfproject loadProject: Loading patch " + patchFilename);
                if (pt->loadPatchFromFile(patchFilename)) {
                    this->addPatch(pt);
                } else {
                    // Error message on loading patch.
                    userMessage("sfproject loadProject: Error loading patch: " + patchFilename);
                }

            } else if (r.name() == "midiAutoConnectList") {

                while (r.readNextStartElement()) { // port
                    if (r.name() == "port") {
                        this->midiAutoConnectList.append(r.readElementText());
                    } else {
                        userMessage("sfproject::loadProject: "
                                    "Unrecognized midiAutoConnectList port element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

            } else if (r.name() == "midiOutPortList") {

                while (r.readNextStartElement()) { // port
                    prjMidiOutPort p;
                    int id = midiOutPort_getUniqueId();
                    while (r.readNextStartElement()) {
                        if (r.name() == "portId") {
                            id = r.readElementText().toInt();
                        } else if (r.name() == "portName") {
                            p.portName = r.readElementText();
                        } else if (r.name() == "client") {
                            p.clients.append( r.readElementText() );
                        } else {
                            userMessage("sfproject::loadProject: "
                                        "Unrecognized midiOutPortList port element: " + r.name().toString() );
                            r.skipCurrentElement();
                        }
                    }
                    if (midiOutPortMap.contains(id)) {
                        userMessage("sfproject::loadProject: "
                                    "Duplicate midi out port id detected: " + n2s(id));
                    }
                    this->midiOutPortMap.insert(id, p);
                }

            } else if (r.name() == "audioBusList") {

                while (r.readNextStartElement()) { // bus
                    prjAudioBus b;
                    int id = audioBus_getUniqueId();
                    while (r.readNextStartElement()) {
                        if (r.name() == "busId") {
                            id = r.readElementText().toInt();
                        } else if (r.name() == "busName") {
                            b.busName = r.readElementText();
                        } else if (r.name() == "leftGain") {
                            b.leftGain = r.readElementText().toFloat();
                        } else if (r.name() == "rightGain") {
                            b.rightGain = r.readElementText().toFloat();
                        } else if (r.name() == "leftClient") {
                            b.leftOutClients.append( r.readElementText() );
                        } else if (r.name() == "rightClient") {
                            b.rightOutClients.append( r.readElementText() );
                        } else {
                            userMessage("sfproject::loadProject: "
                                        "Unrecognized bus element: " + r.name().toString() );
                            r.skipCurrentElement();
                        }
                    }
                    if (audioBusMap.contains(id)) {
                        userMessage("sfproject::loadProject: "
                                    "Duplicate bus id detected: " + n2s(id));
                    }
                    this->audioBusMap.insert(id, b);
                }

            } else if (r.name() == "audioInputPortList") {

                while (r.readNextStartElement()) { // port
                    prjAudioInPort p;
                    int id = audioInPort_getUniqueId();
                    while (r.readNextStartElement()) {
                        if (r.name() == "portId") {
                            id = r.readElementText().toInt();
                        } else if (r.name() == "portName") {
                            p.portName = r.readElementText();
                        } else if (r.name() == "leftGain") {
                            p.leftGain = r.readElementText().toFloat();
                        } else if (r.name() == "rightGain") {
                            p.rightGain = r.readElementText().toFloat();
                        } else if (r.name() == "destinationBus") {
                            p.destinationBus = r.readElementText().toInt();
                        } else if (r.name() == "leftClient") {
                            p.leftInClients.append( r.readElementText() );
                        } else if (r.name() == "rightClient") {
                            p.rightInClients.append( r.readElementText() );
                        } else {
                            userMessage("sfproject::loadProject: "
                                        "Unrecognized audio input port element: " + r.name().toString() );
                            r.skipCurrentElement();
                        }
                    }
                    if (audioInPortMap.contains(id)) {
                        userMessage("sfproject::loadProject: "
                                    "Duplicate audio in port id detected: " + n2s(id));
                    }
                    this->audioInPortMap.insert(id, p);
                }

            } else if (r.name() == "processList") {

                while (r.readNextStartElement()) { // process
                    konfytProcess* gp = new konfytProcess();
                    while (r.readNextStartElement()) {
                        if (r.name() == "appname") {
                            gp->appname = r.readElementText();
                        } else if (r.name() == "dir") {
                            gp->dir = r.readElementText();
                        } else if (r.name() == "argument") {
                            gp->args.append(r.readElementText());
                        } else {
                            userMessage("sfproject::loadProject: "
                                        "Unrecognized process element: " + r.name().toString() );
                            r.skipCurrentElement();
                        }
                    }
                    gp->projectDir = this->getDirname();
                    this->addProcess(gp);
                }


            } else if (r.name() == "triggerList") {

                while (r.readNextStartElement()) {
                    if (r.name() == "trigger") {

                        konfytTrigger trig;
                        while (r.readNextStartElement()) {
                            if (r.name() == "actionText") {
                                trig.actionText = r.readElementText();
                            } else if (r.name() == "type") {
                                trig.type = r.readElementText().toInt();
                            } else if (r.name() == "channel") {
                                trig.channel = r.readElementText().toInt();
                            } else if (r.name() == "data1") {
                                trig.data1 = r.readElementText().toInt();
                            } else if (r.name() == "bankMSB") {
                                trig.bankMSB = r.readElementText().toInt();
                            } else if (r.name() == "bankLSB") {
                                trig.bankLSB = r.readElementText().toInt();
                            } else {
                                userMessage("sfproject::loadProject: "
                                            "Unrecognized trigger element: " + r.name().toString() );
                                r.skipCurrentElement();
                            }
                        }
                        this->addAndReplaceTrigger(trig);

                    } else {
                        userMessage("sfproject::loadProject: "
                                    "Unrecognized triggerList element: " + r.name().toString() );
                        r.skipCurrentElement();
                    }
                }

            }
            else {
                userMessage("sfproject::loadProject: "
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

void konfytProject::midiInPort_addClient(QString port)
{
    this->midiAutoConnectList.append(port);
    setModified(true);
}

void konfytProject::midiInPort_removeClient(QString port)
{
    this->midiAutoConnectList.removeOne(port);
    setModified(true);
}

void konfytProject::midiInPort_clearClients()
{
    this->midiAutoConnectList.clear();
    setModified(true);
}

QStringList konfytProject::midiInPort_getClients()
{
    return this->midiAutoConnectList;
}

int konfytProject::midiOutPort_addPort(QString portName)
{
    prjMidiOutPort p;
    p.portName = portName;
    p.jackPort = NULL;

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
    prjAudioBus bus;
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

prjAudioBus konfytProject::audioBus_getBus(int busId)
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

void konfytProject::audioBus_replace(int busId, prjAudioBus newBus)
{
    audioBus_replace_noModify(busId, newBus);
    setModified(true);
}

/* Do not change the project's modified state. */
void konfytProject::audioBus_replace_noModify(int busId, prjAudioBus newBus)
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
        prjAudioBus b = audioBusMap.value(busId);
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
        prjAudioBus b = audioBusMap.value(busId);
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
    prjAudioInPort port;
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

prjAudioInPort konfytProject::audioInPort_getPort(int portId)
{
    if (audioInPortMap.contains(portId)) {
        return audioInPortMap.value(portId);
    } else {
        error_abort("audioInPorts_getPort: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::audioInPort_replace(int portId, prjAudioInPort newPort)
{
    audioInPort_replace_noModify(portId, newPort);
    setModified(true);
}

/* Do not change the project's modify state. */
void konfytProject::audioInPort_replace_noModify(int portId, prjAudioInPort newPort)
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
        prjAudioInPort p = audioInPortMap.value(portId);
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
        prjAudioInPort p = audioInPortMap.value(portId);
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
        error_abort("midiOutPortList_removePort: port with id " + n2s(portId) + " does not exist.");
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

prjMidiOutPort konfytProject::midiOutPort_getPort(int portId)
{
    if (midiOutPortMap.contains(portId)) {
        return midiOutPortMap.value(portId);
    } else {
        error_abort("midiOutPortList_getPort: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::midiOutPort_addClient(int portId, QString client)
{
    if (midiOutPortMap.contains(portId)) {
        prjMidiOutPort p = midiOutPortMap.value(portId);
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
        prjMidiOutPort p = midiOutPortMap.value(portId);
        p.clients.removeAll(client);
        midiOutPortMap.insert(portId, p);
        setModified(true);
    } else {
        error_abort("midiOutPortList_removeClient: port with id " + n2s(portId) + " does not exist.");
    }
}

void konfytProject::midiOutPort_replace(int portId, prjMidiOutPort port)
{
    midiOutPort_replace_noModify(portId, port);
    setModified(true);
}

/* Do not change the project's modify state. */
void konfytProject::midiOutPort_replace_noModify(int portId, prjMidiOutPort port)
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
        error_abort("midiOutPortList_getClients: port with id " + n2s(portId) + " does not exist.");
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

void konfytProject::runProcess(int index)
{
    if ( (index>=0) && (index < processList.count()) ) {
        // Start process
        processList.at(index)->start();
        userMessage("Starting process " + processList.at(index)->appname);
        userMessage("   args:");
        QStringList args = processList.at(index)->args;
        for (int i=0; i<args.count(); i++) {
            userMessage("       " + args[i]);
        }
    }
}

void konfytProject::stopProcess(int index)
{
    if ( (index>=0) && (index < processList.count()) ) {
        // Stop process
        processList.at(index)->stop();
        userMessage("Stopping process " + processList.at(index)->appname);
    }
}

void konfytProject::removeProcess(int index)
{
    if ( (index>=0) && (index < processList.count()) ) {
        processList.removeAt(index);
        setModified(true);
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

void konfytProject::addAndReplaceTrigger(konfytTrigger newTrigger)
{
    removeTrigger(newTrigger.actionText);
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

QList<konfytTrigger> konfytProject::getTriggerList()
{
    return triggerHash.values();
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
    std::cout << "\n" << "Konfyt ERROR, ABORTING: sfproject:" << msg.toLocal8Bit().constData();
    abort();
}

