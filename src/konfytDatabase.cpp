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

#include "konfytDatabase.h"


// ============================================================================
// konfytDatabaseWorker
// ============================================================================

konfytDatabaseWorker::konfytDatabaseWorker()
{
    synth = NULL;
}

// Scan specified directories for soundfonts, SFZs and patches.
// To get info on soundfonts, they have to be loaded into fluidsynth.
// To save time, soundfonts in the specified ignoreList will not be loaded into fluidsynth.
void konfytDatabaseWorker::scanDirs(QString sfontDir, QString sfzDir, QString patchesDir, QList<KonfytSoundfont *> sfontIgnoreList)
{
    QStringList sfontPaths, sfzPaths, patchPaths;
    QStringList sfontSuffix, sfzSuffix, patchSuffix;
    sfontSuffix << "sf2";
    sfzSuffix << "sfz" << "gig";
    patchSuffix << KONFYT_PATCH_SUFFIX;
    QStringList sfontsToLoad;
    QList<KonfytSoundfont*> sfontsToReturn;

    // -----------------------------------------------------------------
    // Soundfonts
    emit scanDirsStatus("Scanning for soundfonts in " + sfontDir);
    scanDirForFiles(sfontDir, sfontSuffix, sfontPaths);
    emit scanDirsStatus("Loading new soundfonts...");
    // For each soundfont path, if it is not a soundfont in the ignore list, add it to the load list.
    for (int i=0; i<sfontPaths.count(); i++) {
        bool ignore = false;
        for (int j=0; j<sfontIgnoreList.count(); j++) {
            KonfytSoundfont* sf = sfontIgnoreList[j];
            if (sf->filename == sfontPaths[i]) {
                ignore = true;
                break;
            }
        }
        if (!ignore) { sfontsToLoad.append(sfontPaths[i]); }
    }
    // For each soundfont path in the load list, load it in fluidsynth to extract its data.
    for (int i=0; i<sfontsToLoad.count(); i++) {
        emit scanDirsStatus("Loading soundfont " + sfontsToLoad[i]);
        KonfytSoundfont* newSfont = _sfontFromFile(sfontsToLoad[i]);
        if (newSfont == NULL) {
            emit userMessage("Failed to create sfont from filename " + sfontsToLoad[i]);
        } else {
            sfontsToReturn.append(newSfont);
        }
    }

    // -----------------------------------------------------------------
    // SFZs
    emit scanDirsStatus("Scanning for SFZs in " + sfzDir);
    scanDirForFiles(sfzDir, sfzSuffix, sfzPaths);

    // -----------------------------------------------------------------
    // Patches
    emit scanDirsStatus("Scanning for patches in " + patchesDir);
    scanDirForFiles(patchesDir, patchSuffix, patchPaths);

    // -----------------------------------------------------------------
    // Finally, return lists.
    emit scanDirsFihished(sfontsToReturn, sfzPaths, patchPaths);
}

/* Scans directory and subdirectories recursively and add all files with specified suffixes
 * to the list.*/
void konfytDatabaseWorker::scanDirForFiles(QString dirname, QStringList suffixes, QStringList &list)
{
    if (dirname.length() == 0) { return; }
    QDir dir(dirname);
    if (!dir.exists()) {
        emit userMessage("scanDirForFiles: Dir does not exist.");
        return;
    }

    // Get list of all subfiles and directories. Then for each:
    // If a file, add it if it is a soundfont.
    // If a directory, run this function on it.
    QFileInfoList fil = dir.entryInfoList();
    for (int i=0; i<fil.count(); i++) {
        QFileInfo fi = fil.at(i);
        if (fi.fileName() == ".") { continue; }
        if (fi.fileName() == "..") { continue; }

        if (fi.isFile()) {
            for (int j=0; j<suffixes.count(); j++) {
                QString suffix = suffixes[j];
                if (fi.suffix().toLower() == suffix.toLower()) {
                    // Add to list
                    list.append(fi.filePath());
                }
            }
        } else if (fi.isDir()) {
            // Scan the dir
            scanDirForFiles(fi.filePath(), suffixes, list);
        }
    }

}

/* Slot to create a konfytSoundfont object from a filename by loading it into fluidsynth
 * in order to extract soundfont info, returning it with a signal so the rest of the
 * application can continue during this potentially long operation. */
void konfytDatabaseWorker::sfontFromFile(QString filename, int source)
{
    KonfytSoundfont* newSfont = _sfontFromFile(filename);
    emit sfontFromFileFinished(newSfont, source);
}

// Create an sfont object from a file. Returns NULL on error.
// The soundfont is loaded in a fluidsynth engine in order to extract
// the program information.
KonfytSoundfont* konfytDatabaseWorker::_sfontFromFile(QString filename)
{
    if (synth == NULL) {
        emit userMessage("sfontFromFile: Fluidsynth is not initialised.");
        return NULL;
    }

    // Load soundfont file
    int sfID = fluid_synth_sfload(synth, filename.toLocal8Bit().data(), 1);
    if (sfID == -1) {
        emit userMessage("sfontFromFile: Fluidsynth failed to load soundfont " + filename);
        return NULL;
    }

    KonfytSoundfont* newSfont = new KonfytSoundfont();
    newSfont->filename = filename;

    // Get fluidsynth soundfont object
    fluid_sfont_t* sf = fluid_synth_get_sfont_by_id(synth,sfID);

    // Get soundfont name
    QDir dir(filename);
    newSfont->name = dir.dirName();

    // Iterate through all the presets within the soundfont
    int more = 1;
    fluid_preset_t* preset = new fluid_preset_t();
    // Reset the iteration
    sf->iteration_start(sf);
    while (more) {
        more = sf->iteration_next(sf, preset);
        if (more) {
            // Get preset name
            QString presetName = QString(QByteArray( preset->get_name(preset) ));
            int banknum = preset->get_banknum(preset);
            int num = preset->get_num(preset);
            KonfytSoundfontProgram sfp;
            sfp.name = presetName;
            sfp.bank = banknum;
            sfp.program = num;
            sfp.parent_soundfont = newSfont->filename;
            newSfont->programlist.append(sfp);
        }
    }

    // Unload soundfont to save memory
    fluid_synth_sfunload(synth,sfID,1);
    delete preset;

    return newSfont;
}



// ============================================================================
// konfytDatabase
// ============================================================================

konfytDatabase::konfytDatabase()
{
    settings = NULL;
    synth = NULL;

    sfontTree = new konfytDbTree();
    sfontTree_results = new konfytDbTree();
    sfzTree = new konfytDbTree();
    sfzTree_results = new konfytDbTree();

    qRegisterMetaType<QList<KonfytSoundfont*> >("QList<konfytSoundfont*>");

    connect(&worker, &konfytDatabaseWorker::userMessage,
            this, &konfytDatabase::userMessageFromWorker);

    connect(&worker, &konfytDatabaseWorker::scanDirsStatus,
            this, &konfytDatabase::scanDirsStatusFromWorker);

    connect(&worker, &konfytDatabaseWorker::scanDirsFihished,
            this, &konfytDatabase::scanDirsFinished);

    connect(this, &konfytDatabase::start_scanDirs,
            &worker, &konfytDatabaseWorker::scanDirs);

    connect(this, &konfytDatabase::start_sfontFromFile,
            &worker, &konfytDatabaseWorker::sfontFromFile);

    connect(&worker, &konfytDatabaseWorker::sfontFromFileFinished,
            this, &konfytDatabase::sfontFromFileFinished);

    worker.moveToThread(&workerThread);
    workerThread.start();

    // Initialise fluidsynth
    if (initFluidsynth()) {
        emit userMessage("konfytDatabase: Failed to initialise fluidsynth.");
    }

}

konfytDatabase::~konfytDatabase()
{
    workerThread.quit();
    workerThread.wait();
}

void konfytDatabase::userMessageFromWorker(QString msg)
{
    userMessage(msg);
}

void konfytDatabase::scanDirsStatusFromWorker(QString msg)
{
    emit scanDirs_status(msg);
}

// Starts scanning directories and returns. Finished signal will be emitted later when done.
void konfytDatabase::scanDirs(QString sfontsDir, QString sfzDir, QString patchesDir)
{
    _sfontsDir = sfontsDir;
    _sfzDir = sfzDir;
    _patchesDir = patchesDir;

    emit scanDirs_status("Starting scan...");

    if (synth == NULL) {
        emit userMessage("sfontFromFile: Fluidsynth not initialized.");
        emit scanDirs_finished();
        return;
    }

    // Signal the worker to start scanning.
    // We pass the current sfontlist as ignoreList parameter to not reload soundfonts we already have info on.
    emit start_scanDirs(_sfontsDir, _sfzDir, _patchesDir, sfontlist);
    // We now wait for the scanDirsFinished signal from the worker. See the scanDirsFinished() slot.
}

void konfytDatabase::scanDirsFinished(QList<KonfytSoundfont*> sfonts, QStringList sfzs, QStringList patches)
{
    // We have received lists from the worker thread.

    // Add sfonts
    for (int i=0; i<sfonts.count(); i++) {
        addSfont(sfonts[i]);
    }

    // Add SFZs
    for (int i=0; i<sfzs.count(); i++) {
        addSfz(sfzs[i]);
    }

    // Add patches
    for (int i=0; i<patches.count(); i++) {
        addPatch(patches[i]);
    }

    // Directory scanning has finished. Emit signal to user.
    emit scanDirs_finished();
}



void konfytDatabase::sfontFromFileFinished(KonfytSoundfont *sfont, int source)
{
    if (source == konfytDatabaseSource_returnSfont) {
        // The original command was sent from returnSfont()
        emit returnSfont_finished(sfont);
    }
}


// Adds a patch object to the database from a given filename.
// Returns true if patch could be successfully loaded from filename,
// false otherwise.
void konfytDatabase::addPatch(QString filename)
{
    KonfytPatch p;
    if ( p.loadPatchFromFile(filename) ) {
        patchList.append(p);
        patchFilenameList.append(filename);
    } else {
        userMessage("konfytDatabase addPatch: Failed to load patch " + filename);
    }

}

void konfytDatabase::addSfz(QString filename)
{
    sfzList.append(filename);
}


// Adds an sfont object to the database.
void konfytDatabase::addSfont(KonfytSoundfont *sf)
{
    sfontlist.append(sf);
}


// Clears the database soundfont list and results.
void konfytDatabase::clearDatabase()
{
    // Clear soundfonts
    sfontlist.clear();
    sfontResults.clear();

    // Clear patches
    patchList.clear();
    patchFilenameList.clear();
    patchResults.clear();

    // Clear sfz
    sfzList.clear();
    sfzResults.clear();
}

void konfytDatabase::clearDatabase_exceptSoundfonts()
{
    // Clear patches
    patchList.clear();
    patchFilenameList.clear();
    patchResults.clear();

    // Clear sfz
    sfzList.clear();
    sfzResults.clear();
}

// General use function to return a sfont object based on the filename.
// The database is first search for the sfont. If it is not in the database,
// the sfont is loaded from file and returned.
// This function does not block and the soundfont is actually returned by the
// returnSfont_finished() signal.
void konfytDatabase::returnSfont(QString filename)
{
    // First check in database.
    for (int i=0; i<sfontlist.count(); i++) {
        if (filename == sfontlist.at(i)->filename) {
            emit returnSfont_finished(sfontlist.at(i));
        }
    }
    // Not found in database. Load from file.
    emit start_sfontFromFile(filename, konfytDatabaseSource_returnSfont);
    // The worker thread will now load the soundfont and return a signal
    // when finished to the returnSfontFinished() slot.
}

// General use function to return a sfont object parent of given program.
// Similar to returnSfont(QString filename).
// This function does not block and the soundfont is actually returned by the
// returnSfont_finished() signal.
void konfytDatabase::returnSfont(KonfytSoundfontProgram p)
{
    return returnSfont(p.parent_soundfont);
}




// Initialise fluidsynth and return 0. Returns 1 on error.
int konfytDatabase::initFluidsynth()
{
    if (settings != NULL) {
        delete_fluid_settings(settings);
    }
    if (synth != NULL) {
        delete_fluid_synth(synth);
    }
    settings = NULL;
    synth = NULL;

    // Create settings object
    settings = new_fluid_settings();
    if (settings == NULL) {
        emit userMessage("initFluidSynth: Failed to create fluidsynth settings.");
        return 1;
    }

    // Create the synthesizer
    synth = new_fluid_synth(settings);
    if (synth == NULL) {
        emit userMessage("initFluidSynth: Failed to create fluidsnth synthesizer.");
        return 1;
    }

    worker.synth = synth;

    return 0;

}


// Save the database to an xml file. Returns 0 if success, 1 if not.
bool konfytDatabase::saveDatabaseToFile(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit userMessage("Failed to open file for saving database.");
        return false;
    }

    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeComment("This is a Konfyt database.");

    stream.writeStartElement(XML_DATABASE);

    // All the soundfonts
    for (int i=0; i<sfontlist.count(); i++) {

        stream.writeStartElement("soundfont");

        KonfytSoundfont* sf = sfontlist.at(i);

        stream.writeAttribute("filename", sf->filename);
        stream.writeAttribute("name", sf->name);

        // All the programs ("presets")
        for (int j=0; j<sf->programlist.count(); j++) {

            stream.writeStartElement("preset");

            stream.writeTextElement("bank", n2s(sf->programlist.at(j).bank));
            stream.writeTextElement("program", n2s(sf->programlist.at(j).program));
            stream.writeTextElement("name", sf->programlist.at(j).name);

            stream.writeEndElement();
        }

        stream.writeEndElement(); // soundfont
    }

    // All the patches
    for (int i=0; i<patchList.count(); i++) {

        stream.writeStartElement("patch");

        KonfytPatch pt = patchList.at(i);

        stream.writeAttribute("filename", patchFilenameList.at(i));
        stream.writeAttribute("name", pt.getName());


        stream.writeEndElement(); // patch
    }

    // All the SFZs
    for (int i=0; i<sfzList.count(); i++) {

        stream.writeStartElement("sfz");
        stream.writeAttribute("filename", sfzList.at(i));
        stream.writeEndElement();

    }

    stream.writeEndElement(); // sfdatabase

    stream.writeEndDocument();

    file.close();
    return true;
}


QList<KonfytSoundfont*> konfytDatabase::getSfontList()
{
    return sfontlist;
}

int konfytDatabase::getNumSfonts()
{
    return sfontlist.count();
}

QStringList konfytDatabase::getSfzList()
{
    return sfzList;
}

int konfytDatabase::getNumSfz()
{
    return sfzList.count();
}

/* Build a tree of all sfz items, based on their directory structure. */
void konfytDatabase::buildSfzTree()
{
    sfzTree->clearTree();

    for (int sfz=0; sfz<sfzList.count(); sfz++) {

        QStringList l = sfzList.at(sfz).split("/");

        if (l[0] == "") { l.removeAt(0); }
        QString path;

        konfytDbTreeItem* item = this->sfzTree->root;
        for (int i=0; i<l.count(); i++) {
            path += "/" + l.at(i);
            bool contains = false;
            QList<konfytDbTreeItem*> children = item->children;
            for (int j=0; j<children.count(); j++) {
                if (children.at(j)->name == l.at(i)) {
                    contains = true;
                    item = children.at(j);
                }
            }
            if (contains==false) {
                konfytDbTreeItem* newItem = new konfytDbTreeItem(item, l.at(i));
                newItem->path = path;
                item->children.append(newItem);
                item = newItem;
            }
        }
        // Last item is the leaf
        item->path = sfzList.at(sfz);
    }


    // Compact tree
    compactTree(sfzTree->root);
}

void konfytDatabase::buildSfzTree_results()
{
    sfzTree_results->clearTree();

    for (int sfz=0; sfz<sfzResults.count(); sfz++) {

        QStringList l = sfzResults.at(sfz).split("/");

        if (l[0] == "") { l.removeAt(0); }
        QString path;

        konfytDbTreeItem* item = this->sfzTree_results->root;
        for (int i=0; i<l.count(); i++) {
            path += "/" + l.at(i);
            bool contains = false;
            QList<konfytDbTreeItem*> children = item->children;
            for (int j=0; j<children.count(); j++) {
                if (children.at(j)->name == l.at(i)) {
                    contains = true;
                    item = children.at(j);
                }
            }
            if (contains==false) {
                konfytDbTreeItem* newItem = new konfytDbTreeItem(item, l.at(i));
                newItem->path = path;
                item->children.append(newItem);
                item = newItem;
            }
        }
        // Last item is the leaf
        item->path = sfzResults.at(sfz);
    }


    // Compact tree
    compactTree(sfzTree_results->root);
}

void konfytDatabase::buildSfontTree()
{
    sfontTree->clearTree();

    for (int sf=0; sf<sfontlist.count(); sf++) {
        QStringList l = sfontlist.at(sf)->filename.split("/");

        if (l[0] == "") { l.removeAt(0); }
        QString path;

        konfytDbTreeItem* item = this->sfontTree->root;
        for (int i=0; i<l.count(); i++) {
            path += "/" + l.at(i);
            bool contains = false;
            QList<konfytDbTreeItem*> children = item->children;
            for (int j=0; j<children.count(); j++) {
                if (children.at(j)->name == l.at(i)) {
                    contains = true;
                    item = children.at(j);
                }
            }
            if (contains == false) {
                konfytDbTreeItem* newItem = new konfytDbTreeItem(item, l.at(i));
                newItem->path = path;
                item->children.append(newItem);
                item = newItem;
            }
        }
        // Last item is the leaf
        item->path = sfontlist.at(sf)->filename;
        item->data = sfontlist.at(sf);
    }

    // Compact tree
    compactTree(sfontTree->root);
}

void konfytDatabase::buildSfontTree_results()
{
    sfontTree_results->clearTree();

    QList<KonfytSoundfont*> sfResultsList = getResults_sfonts();
    for (int sf=0; sf<sfResultsList.count(); sf++) {
        QStringList l = sfResultsList.at(sf)->filename.split("/");

        if (l[0] == "") { l.removeAt(0); }
        QString path;

        konfytDbTreeItem* item = this->sfontTree_results->root;
        for (int i=0; i<l.count(); i++) {
            path += "/" + l.at(i);
            bool contains = false;
            QList<konfytDbTreeItem*> children = item->children;
            for (int j=0; j<children.count(); j++) {
                if (children.at(j)->name == l.at(i)) {
                    contains = true;
                    item = children.at(j);
                }
            }
            if (contains == false) {
                konfytDbTreeItem* newItem = new konfytDbTreeItem(item, l.at(i));
                newItem->path = path;
                item->children.append(newItem);
                item = newItem;
            }
        }
        // Last item is the leaf
        item->path = sfResultsList.at(sf)->filename;
        item->data = sfResultsList.at(sf);
    }

    // Compact tree
    compactTree(sfontTree_results->root);
}

/* Compact a tree, by combining branches with their children
 * if they only have single children. E.g. a->b->c.sfz becomes a/b->c.sfz */
void konfytDatabase::compactTree(konfytDbTreeItem* item)
{
    for (int i=0; i<item->children.count(); i++) {
        compactTree(item->children.at(i));
    }

    if (item->parent != NULL) { // Do not do this for the root item
        if (item->children.count() == 1) {
            konfytDbTreeItem* child = item->children.at(0);
            item->name = item->name + "/" + child->name;
            // NB remember to copy all of the child's data over
            item->path = child->path;
            item->data = child->data;
            item->children = child->children;
        }
    }
}

// Clears the database and loads it from a single saved database xml file.
bool konfytDatabase::loadDatabaseFromFile(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit userMessage("Failed to open database file for reading.");
        return false;
    }

    QXmlStreamReader r(&file);
    r.setNamespaceProcessing(false);




    this->clearDatabase();

    while (r.readNextStartElement()) { // sfdatabase

        while (r.readNextStartElement()) { // soundfont or patch

            if (r.name() == "soundfont") {

                KonfytSoundfont* sf = new KonfytSoundfont();
                sf->filename = r.attributes().value("filename").toString();
                sf->name = r.attributes().value("name").toString();

                while (r.readNextStartElement()) { // preset

                    if (r.name() == "preset") {

                        KonfytSoundfontProgram p;

                        while (r.readNextStartElement()) {
                            if (r.name() == "bank") {
                                p.bank = r.readElementText().toInt();
                            } else if (r.name() == "program") {
                                p.program = r.readElementText().toInt();
                            } else if (r.name() == "name") {
                                p.name = r.readElementText();
                            }
                        }

                        p.parent_soundfont = sf->filename;
                        sf->programlist.append(p);

                    } else {
                        r.skipCurrentElement();
                    }
                }

                this->addSfont(sf);

            } else if (r.name() == "patch") {

                QString patchFilename = r.attributes().value("filename").toString();
                KonfytPatch pt;
                if (pt.loadPatchFromFile(patchFilename)) {
                    patchList.append(pt);
                    patchFilenameList.append(patchFilename);
                }
                r.skipCurrentElement();

            } else if (r.name() == "sfz") {

                QString sfzFilename = r.attributes().value("filename").toString();
                sfzList.append(sfzFilename);
                r.skipCurrentElement();

            } else {
                r.skipCurrentElement();
            }
        }
    }

    file.close();
    return true;

}




// Searches all the programs in all the soundfonts.
// If programs in a soundfont match the given string, they are
// added to the results list in their soundfont.
// All the resultant soundfonts (containing only the programs matching
// the search string) are added to the sfontResults map.
// The results can be accessed with the getResults functions.
void konfytDatabase::searchProgram(QString str)
{
    // Search all the soundfonts.
    // If a soundfont's filename matches the search string, the entire
    // soundfont (with all its programs) is included in the results.
    // Otherwise, the soundfont is only included if one or more of its
    // programs match the search string (and only those programs are included).
    sfontResults.clear();
    for (int i=0; i<sfontlist.count(); i++) {
        KonfytSoundfont* sf = sfontlist.at(i);
        sf->searchResults.clear();
        if (sf->name.toLower().contains(str.toLower())) {
            // Soundfont name match. Include all programs
            sf->searchResults = sf->programlist;
        } else {
            // Search soundfont programs
            for (int j=0; j<sf->programlist.count(); j++) {
                KonfytSoundfontProgram p = sf->programlist.at(j);
                if (p.name.toLower().contains(str.toLower())) {
                    sf->searchResults.append(p);
                }
            }
        }
        if (sf->searchResults.count()) {
            sfontResults.insert(sf->filename,sf);
        }
    }

    // Search all the patches (patch names as well as programs)
    patchResults.clear();
    for (int i=0; i<patchList.count(); i++) {
        KonfytPatch pt = patchList.at(i);
        // First check patch name
        if (pt.getName().toLower().contains(str.toLower())) {
            patchResults.append(pt);
        } else {
            // Else check patch layers
            QList<KonfytPatchLayer> layers = pt.getLayerItems();
            for (int j=0; j<layers.count(); j++) {
                if (layers[j].getName().toLower().contains(str.toLower())) {
                    patchResults.append(pt);
                    break;
                }
            }
        }
    }

    // Search all the SFZs
    sfzResults.clear();
    for (int i=0; i<sfzList.count(); i++) {
        if (sfzList.at(i).toLower().contains(str.toLower())) {
            sfzResults.append(sfzList.at(i));
        }
    }
}

// Returns a list of patches from the search results.
QList<KonfytPatch> konfytDatabase::getResults_patches()
{
    return patchResults;
}

QStringList konfytDatabase::getResults_sfz()
{
    return sfzResults;
}

// Returns a list of soundfonts from the search results.
// The soundfonts only contain the programs matching the search.
QList<KonfytSoundfont*> konfytDatabase::getResults_sfonts()
{
    QList<KonfytSoundfont*> l;
    QList<QString> keys = sfontResults.keys();
    for (int i=0; i<keys.count(); i++) {
        l.append(sfontResults.value(keys.at(i)));
    }
    return l;
}

int konfytDatabase::getNumSfontsResults()
{
    return sfontResults.count();
}

int konfytDatabase::getNumSfontProgramResults()
{
    int programs = 0;
    QList<KonfytSoundfont*> sfonts = sfontResults.values();
    for (int i=0; i<sfonts.count(); i++) {
        programs += sfonts[i]->searchResults.count();
    }
    return programs;
}

// Returns a list of all programs within a specific sounfont matching the search.
QList<KonfytSoundfontProgram> konfytDatabase::getResults_sfontPrograms(KonfytSoundfont* sf)
{
    return sfontResults.value(sf->filename)->searchResults;
}

// Returns a list of all the programs in the search results.
QList<KonfytSoundfontProgram> konfytDatabase::getResults_allPrograms()
{
    QList<KonfytSoundfontProgram> l;
    QList<KonfytSoundfont*> all = sfontResults.values();
    for (int i=0; i<all.count(); i++) {
        l.append(all.at(i)->searchResults);
    }
    return l;
}

int konfytDatabase::getNumPatchesResults()
{
    return patchResults.count();
}

int konfytDatabase::getNumSfzResults()
{
    return sfzResults.count();
}

QList<KonfytPatch> konfytDatabase::getPatchList()
{
    return patchList;
}

int konfytDatabase::getNumPatches()
{
    return patchList.count();
}

