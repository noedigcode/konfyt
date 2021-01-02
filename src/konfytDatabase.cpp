/******************************************************************************
 *
 * Copyright 2021 Gideon van der Kolf
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

KonfytDatabaseWorker::KonfytDatabaseWorker()
{
}

KfSoundPtr KonfytDatabaseWorker::patchFromFile(QString filename)
{
    KfSoundPtr ret;

    KonfytPatch p;
    if (p.loadPatchFromFile(filename)) {
        ret.reset(new KonfytSound());
        ret->filename = filename;
        ret->name = p.name();
        foreach (KfPatchLayerSharedPtr layer, p.layers()) {
            KonfytSoundPreset preset;
            preset.name = layer->name();
            ret->presets.append(preset);
        }
    }

    return ret;
}

/* Scan specified directories for soundfonts, SFZs and patches.
 * To get info on soundfonts, they have to be loaded into Fluidsynth.
 * To save time, soundfonts in the specified ignoreList will not be loaded into
 * Fluidsynth. */
void KonfytDatabaseWorker::scan()
{
    scanSfonts();
    scanSfzs();
    scanPatches();

    emit scanFinished();
}

/* Scans directory and subdirectories recursively and add all files with specified suffixes
 * to the list. */
void KonfytDatabaseWorker::scanDirForFiles(QString dirname, QStringList suffixes, QStringList &list)
{
    if (dirname.length() == 0) { return; }
    QDir dir(dirname);
    if (!dir.exists()) {
        emit print("scanDirForFiles: Dir does not exist.");
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

void KonfytDatabaseWorker::scanSfonts()
{
    QStringList sfontSuffix = {"sf2", "sf3"};
    sfontResults.clear();

    emit scanStatus("Scanning for soundfonts in " + sfontDir);
    QStringList sfontPaths;
    scanDirForFiles(sfontDir, sfontSuffix, sfontPaths);

    emit scanStatus("Loading new soundfonts...");
    QStringList sfontsToLoad;
    // Load each soundfont that is not in the ignore list.
    foreach (QString path, sfontPaths) {
        bool ignore = false;
        foreach (KfSoundPtr sf, sfontIgnoreList) {
            if (sf->filename == path) {
                ignore = true;
                break;
            }
        }
        if (!ignore) { sfontsToLoad.append(path); }
    }

    // For each soundfont path in the load list, load it in fluidsynth to extract its data.
    foreach (QString path, sfontsToLoad) {
        emit scanStatus("Loading soundfont " + path);
        KfSoundPtr newSfont = sfontFromFile(path);
        if (newSfont) {
            sfontResults.append(newSfont);
        } else {
            emit print("Failed to load soundfont " + path);
        }
    }
}

void KonfytDatabaseWorker::scanSfzs()
{
    QStringList sfzSuffix = {"sfz", "gig"};
    sfzResults.clear();

    emit scanStatus("Scanning for SFZs in " + sfzDir);
    QStringList sfzPaths;
    scanDirForFiles(sfzDir, sfzSuffix, sfzPaths);

    foreach (QString path, sfzPaths) {
        KfSoundPtr sfz = KfSoundPtr(new KonfytSound());
        sfz->filename = path;
        sfz->name = QFileInfo(sfz->filename).fileName();
        sfzResults.append(sfz);
    }
}

void KonfytDatabaseWorker::scanPatches()
{
    QStringList patchSuffix = {KONFYT_PATCH_SUFFIX};

    emit scanStatus("Scanning for patches in " + patchDir);
    QStringList patchPaths;
    scanDirForFiles(patchDir, patchSuffix, patchPaths);

    // For each patch, load it to extract its data
    foreach (QString path, patchPaths) {
        emit scanStatus("Loading patch " + path);
        KfSoundPtr patch = patchFromFile(path);
        if (patch) {
            patchResults.append(patch);
        } else {
            emit print("Failed to load patch " + path);
        }
    }
}

/* Slot to create a konfytSoundfont object from a filename by loading it into
 * Fluidsynth in order to extract soundfont info, returning it with a signal so
 * the rest of the application can continue during this potentially long
 * operation. */
void KonfytDatabaseWorker::requestSfontFromFile(QString filename, int source)
{
    KfSoundPtr newSfont = sfontFromFile(filename);
    emit sfontFromFileFinished(newSfont, source);
}

/* Create a soundfont object from a file. On error, the returned object is null.
 * The soundfont is loaded in a Fluidsynth engine in order to extract the
 * program information. */
KfSoundPtr KonfytDatabaseWorker::sfontFromFile(QString filename)
{
    KfSoundPtr ret;

    if (synth == NULL) {
        emit print("sfontFromFile: Fluidsynth is not initialised.");
        return ret;
    }

    // Load soundfont file
    int sfID = fluid_synth_sfload(synth, filename.toLocal8Bit().data(), 1);
    if (sfID == -1) {
        emit print("sfontFromFile: Fluidsynth failed to load soundfont " + filename);
        return ret;
    }

    ret.reset(new KonfytSound);
    ret->filename = filename;
    ret->name = QFileInfo(filename).fileName();

    // Get fluidsynth soundfont object
    fluid_sfont_t* sf = fluid_synth_get_sfont_by_id(synth, sfID);

    // Iterate through all the presets within the soundfont
#if FLUIDSYNTH_VERSION_MAJOR == 1
    fluid_preset_t* preset = new fluid_preset_t();
    sf->iteration_start(sf);
    int more = sf->iteration_next(sf, preset);
    while (more) {
        KonfytSoundPreset p;

        p.name = QString(QByteArray( preset->get_name(preset) ));
        p.bank = preset->get_banknum(preset);
        p.program = preset->get_num(preset);
        p.parent_soundfont = newSfont->filename;

        ret->programlist.append(p);
        more = sf->iteration_next(sf, preset);
    }
#else
    fluid_sfont_iteration_start(sf);
    fluid_preset_t* preset = fluid_sfont_iteration_next(sf);
    while (preset) {
        KonfytSoundPreset p;

        p.name = QString(QByteArray( fluid_preset_get_name(preset) ));
        p.bank = fluid_preset_get_banknum(preset);
        p.program = fluid_preset_get_num(preset);

        ret->presets.append(p);
        preset = fluid_sfont_iteration_next(sf);
    }
#endif

    // Unload soundfont to save memory
    fluid_synth_sfunload(synth, sfID, 1);
#if FLUIDSYNTH_VERSION_MAJOR == 1
    delete preset;
#endif

    return ret;
}



// ============================================================================
// konfytDatabase
// ============================================================================

konfytDatabase::konfytDatabase()
{
    connect(&worker, &KonfytDatabaseWorker::print,
            this, &konfytDatabase::userMessageFromWorker);

    connect(&worker, &KonfytDatabaseWorker::scanStatus,
            this, &konfytDatabase::scanStatusFromWorker);

    connect(&worker, &KonfytDatabaseWorker::scanFinished,
            this, &konfytDatabase::onScanFinished);

    connect(this, &konfytDatabase::start_scanDirs,
            &worker, &KonfytDatabaseWorker::scan);

    connect(this, &konfytDatabase::start_sfontFromFile,
            &worker, &KonfytDatabaseWorker::requestSfontFromFile);

    connect(&worker, &KonfytDatabaseWorker::sfontFromFileFinished,
            this, &konfytDatabase::sfontFromFileFinished);

    worker.moveToThread(&workerThread);
    workerThread.start();

    // Initialise fluidsynth
    if (initFluidsynth()) {
        emit print("konfytDatabase: Failed to initialise fluidsynth.");
    }
}

konfytDatabase::~konfytDatabase()
{
    workerThread.quit();
    workerThread.wait();
}

QList<KfSoundPtr> konfytDatabase::allSoundfonts()
{
    return mAllSoundfonts;
}

int konfytDatabase::soundfontCount()
{
    return mAllSoundfonts.count();
}

QList<KfSoundPtr> konfytDatabase::allPatches()
{
    return mAllPatches;
}

int konfytDatabase::patchCount()
{
    return mAllPatches.count();
}

QList<KfSoundPtr> konfytDatabase::allSfzs()
{
    return mAllSfzs;
}

int konfytDatabase::sfzCount()
{
    return mAllSfzs.count();
}

void konfytDatabase::setSoundfontsDir(QString path)
{
    mSfontsDir = path;
}

void konfytDatabase::setSfzDir(QString path)
{
    mSfzDir = path;
}

void konfytDatabase::setPatchesDir(QString path)
{
    mPatchesDir = path;
}

void konfytDatabase::userMessageFromWorker(QString msg)
{
    print(msg);
}

void konfytDatabase::scanStatusFromWorker(QString msg)
{
    emit scanStatus(msg);
}

/* Starts scanning directories and returns. Finished signal will be emitted
 * later when done. */
void konfytDatabase::scan()
{
    emit scanStatus("Starting scan...");

    if (mFluidSynth == NULL) {
        emit print("Scan Error: Fluidsynth not initialized.");
        emit scanFinished();
        return;
    }

    // Signal the worker to start scanning.
    // We pass the current sfontlist as ignoreList parameter to not reload
    // soundfonts we already have info on.
    worker.sfontDir = mSfontsDir;
    worker.sfontIgnoreList = mAllSoundfonts;
    worker.sfzDir = mSfzDir;
    worker.patchDir = mPatchesDir;
    emit start_scanDirs();
    // We now wait for the scanDirsFinished signal from the worker.
    // See the onScanFinished() slot.
}

void konfytDatabase::onScanFinished()
{
    // The worker has finished scanning

    // Soundfonts
    foreach (KfSoundPtr sf, worker.sfontResults) {
        addSfont(sf);
    }
    buildSfontTree();

    // SFZs
    foreach (KfSoundPtr sfz, worker.sfzResults) {
        addSfz(sfz);
    }
    buildSfzTree();

    // Patches
    foreach (KfSoundPtr patch, worker.patchResults) {
        addPatch(patch);
    }
    buildPatchTree();

    // Scanning has finished. Emit signal.
    emit scanFinished();
}

void konfytDatabase::sfontFromFileFinished(KfSoundPtr sfont, int source)
{
    if (source == konfytDatabaseSource_returnSfont) {
        // The original command was sent from returnSfont()
        emit returnSfont_finished(sfont);
    }
}

void konfytDatabase::addPatch(QString filename)
{
    KfSoundPtr patch = worker.patchFromFile(filename);
    if (patch) {
        addPatch(patch);
        buildPatchTree();
    }
}

void konfytDatabase::addSfont(KfSoundPtr sf)
{
    mAllSoundfonts.append(sf);
}

void konfytDatabase::addSfz(KfSoundPtr sfz)
{
    mAllSfzs.append(sfz);
}

void konfytDatabase::addPatch(KfSoundPtr patch)
{
    mAllPatches.append(patch);
}

/* Build a tree of all list items based on their directory structure. */
void konfytDatabase::buildTree(KonfytDbTree *tree, const QList<KfSoundPtr> &list,
                               QString rootPath)
{
    tree->clearTree();

    QDir rootDir(rootPath);

    // Add children to tree corresponding to directories in path
    foreach (KfSoundPtr sound, list) {
        QString relativePath = rootDir.relativeFilePath(sound->filename);
        QStringList pathList = relativePath.split("/");
        if (pathList.value(0, "default") == "") { pathList.removeFirst(); }
        QString pathStr = rootPath;
        KfDbTreeItemPtr item = tree->root;
        foreach (QString dir, pathList) {
            pathStr += "/" + dir;
            bool contains = false;
            foreach (KfDbTreeItemPtr child, item->children) {
                if (child->name == dir) {
                    contains = true;
                    item = child;
                    break;
                }
            }
            if (contains == false) {
                item = item->addChild(dir, pathStr, nullptr);
            }
        }
        // Last item is the leaf
        item->path = sound->filename;
        item->data = sound;
    }

    compactTree(tree->root);
}

/* Compact a tree, by combining branches with their children
 * if they only have single children. E.g. a->b->c.sfz becomes a/b->c.sfz */
void konfytDatabase::compactTree(KfDbTreeItemPtr item)
{
    foreach (KfDbTreeItemPtr c, item->children) {
        compactTree(c);
    }

    if (item->parent == nullptr) { return; } // Skip root tree item
    if (item->children.count() != 1) { return; }

    // Merge item with its child
    KfDbTreeItemPtr child = item->children.at(0);
    item->name = item->name + "/" + child->name;
    // NB remember to copy all of the child's data over
    item->path = child->path;
    item->data = child->data;
    item->children = child->children;
}

/* Clears the database soundfont list and results. */
void konfytDatabase::clearDatabase()
{
    // Clear soundfonts
    mAllSoundfonts.clear();
    sfontResults.clear();

    // Clear patches
    mAllPatches.clear();
    patchResults.clear();

    // Clear sfz
    mAllSfzs.clear();
    sfzResults.clear();
}

void konfytDatabase::clearDatabase_exceptSoundfonts()
{
    // Clear patches
    mAllPatches.clear();
    patchResults.clear();

    // Clear sfz
    mAllSfzs.clear();
    sfzResults.clear();
}

/* General use function to return a sfont object based on the filename.
 * The database is first search for the sfont. If it is not in the database,
 * the sfont is loaded from file and returned.
 * This function does not block and the soundfont is actually returned by the
 * returnSfont_finished() signal. */
void konfytDatabase::returnSfont(QString filename)
{
    // First check in database.
    foreach (KfSoundPtr sf, mAllSoundfonts) {
        if (filename == sf->filename) {
            emit returnSfont_finished(sf);
            return;
        }
    }
    // Not found in database. Load from file.
    emit start_sfontFromFile(filename, konfytDatabaseSource_returnSfont);
    // The worker thread will now load the soundfont and return a signal
    // when finished to the returnSfontFinished() slot.
}

/* Initialise Fluidsynth and return 0. Returns 1 on error. */
int konfytDatabase::initFluidsynth()
{
    if (mFluidsynthSettings != NULL) {
        delete_fluid_settings(mFluidsynthSettings);
    }
    if (mFluidSynth != NULL) {
        delete_fluid_synth(mFluidSynth);
    }
    mFluidsynthSettings = NULL;
    mFluidSynth = NULL;

    // Create settings object
    mFluidsynthSettings = new_fluid_settings();
    if (mFluidsynthSettings == NULL) {
        emit print("initFluidSynth: Failed to create fluidsynth settings.");
        return 1;
    }

    // Create the synthesizer
    mFluidSynth = new_fluid_synth(mFluidsynthSettings);
    if (mFluidSynth == NULL) {
        emit print("initFluidSynth: Failed to create fluidsnth synthesizer.");
        return 1;
    }

    worker.synth = mFluidSynth;

    return 0;
}

/* Save the database to an xml file. Returns 0 if success, 1 if not. */
bool konfytDatabase::saveDatabaseToFile(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit print("Failed to open file for saving database.");
        return false;
    }

    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeComment("This is a Konfyt database.");

    stream.writeStartElement(XML_DATABASE);

    // All the soundfonts
    foreach (KfSoundPtr sf, mAllSoundfonts) {

        stream.writeStartElement("soundfont");

        stream.writeAttribute("filename", sf->filename);
        stream.writeAttribute("name", sf->name);

        // All the programs ("presets")
        foreach (const KonfytSoundPreset &preset, sf->presets) {

            stream.writeStartElement("preset");

            stream.writeTextElement("bank", n2s(preset.bank));
            stream.writeTextElement("program", n2s(preset.program));
            stream.writeTextElement("name", preset.name);

            stream.writeEndElement();
        }

        stream.writeEndElement(); // soundfont
    }

    // All the patches
    foreach (KfSoundPtr patch, mAllPatches) {

        stream.writeStartElement("patch");
        stream.writeAttribute("filename", patch->filename);
        stream.writeAttribute("name", patch->name);

        // Layers
        foreach (const KonfytSoundPreset &preset, patch->presets) {
            stream.writeStartElement("layer");

            stream.writeTextElement("name", preset.name);

            stream.writeEndElement();
        }

        stream.writeEndElement(); // patch
    }

    // All the SFZs
    foreach (KfSoundPtr sfz, mAllSfzs) {

        stream.writeStartElement("sfz");
        stream.writeAttribute("filename", sfz->filename);
        stream.writeEndElement();

    }

    stream.writeEndElement(); // sfdatabase

    stream.writeEndDocument();

    file.close();
    return true;
}

/* Build a tree of all sfz items, based on their directory structure. */
void konfytDatabase::buildSfzTree()
{
    buildTree(&sfzTree, mAllSfzs, mSfzDir);
}

void konfytDatabase::buildSfzTree_results()
{
    buildTree(&sfzTree_results, sfzResults, mSfzDir);
}

void konfytDatabase::buildSfontTree()
{
    buildTree(&sfontTree, mAllSoundfonts, mSfontsDir);
}

void konfytDatabase::buildSfontTree_results()
{
    buildTree(&sfontTree_results, sfontResults, mSfontsDir);
}

void konfytDatabase::buildPatchTree()
{
    buildTree(&patchTree, mAllPatches, mPatchesDir);
}

void konfytDatabase::buildPatchTree_results()
{
    buildTree(&patchTree_results, patchResults, mPatchesDir);
}

/* Clears the database and loads it from a single saved database xml file. */
bool konfytDatabase::loadDatabaseFromFile(QString filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit print("Failed to open database file for reading.");
        return false;
    }

    QXmlStreamReader r(&file);
    r.setNamespaceProcessing(false);

    this->clearDatabase();

    while (r.readNextStartElement()) { // sfdatabase

        while (r.readNextStartElement()) { // soundfont or patch

            if (r.name() == "soundfont") {

                KfSoundPtr sf(new KonfytSound());
                sf->filename = r.attributes().value("filename").toString();
                sf->name = r.attributes().value("name").toString();

                while (r.readNextStartElement()) { // preset

                    if (r.name() == "preset") {

                        KonfytSoundPreset p;

                        while (r.readNextStartElement()) {
                            if (r.name() == "bank") {
                                p.bank = r.readElementText().toInt();
                            } else if (r.name() == "program") {
                                p.program = r.readElementText().toInt();
                            } else if (r.name() == "name") {
                                p.name = r.readElementText();
                            }
                        }

                        sf->presets.append(p);

                    } else {
                        r.skipCurrentElement();
                    }
                }

                addSfont(sf);

            } else if (r.name() == "patch") {

                KfSoundPtr patch(new KonfytSound());
                patch->filename = r.attributes().value("filename").toString();
                patch->name = r.attributes().value("name").toString();
                if (patch->name.isEmpty()) {
                    patch->name = QFileInfo(patch->filename).baseName();
                }

                while (r.readNextStartElement()) { // layer
                    if (r.name() == "layer") {
                        KonfytSoundPreset p;
                        while (r.readNextStartElement()) {
                            if (r.name() == "name") {
                                p.name = r.readElementText();
                            }
                        }
                        patch->presets.append(p);
                    } else {
                        r.skipCurrentElement();
                    }
                }
                addPatch(patch);

            } else if (r.name() == "sfz") {

                KfSoundPtr sfz(new KonfytSound());
                sfz->filename = r.attributes().value("filename").toString();
                sfz->name = QFileInfo(sfz->filename).fileName();
                addSfz(sfz);
                r.skipCurrentElement();

            } else {
                r.skipCurrentElement();
            }
        }
    }

    file.close();

    buildSfontTree();
    buildSfzTree();
    buildPatchTree();

    return true;
}

/* Search all soundfonts (and their programs), SFZs and patches for the specified
 * string. Results can be accessed with the getResults functions. */
void konfytDatabase::search(QString str)
{
    str = str.toLower();

    // Search all the soundfonts.
    // If a soundfont's filename matches the search string, the entire
    // soundfont (with all its programs) is included in the results.
    // Otherwise, the soundfont is only included if one or more of its
    // programs match the search string (and only those programs are included).
    sfontResults.clear();
    foreach (KfSoundPtr sf, mAllSoundfonts) {
        if (sf->filename.toLower().contains(str)) {
            // Soundfont name match. Include all programs
            sfontResults.append(sf);
        } else {
            // Search soundfont programs
            QList<KonfytSoundPreset> presetsFound;
            foreach (const KonfytSoundPreset& preset, sf->presets) {
                if (preset.name.toLower().contains(str)) {
                    presetsFound.append(preset);
                }
            }
            if (presetsFound.count()) {
                KfSoundPtr sfresult(new KonfytSound());
                sfresult->filename = sf->filename;
                sfresult->name = sf->name;
                sfresult->presets = presetsFound;
                sfontResults.append(sfresult);
            }
        }
    }
    buildSfontTree_results();

    // Search all the patchest
    patchResults.clear();
    foreach (KfSoundPtr patch, mAllPatches) {
        // Search filename. If it doesn't match, search layers
        if (patch->filename.toLower().contains(str)) {
            patchResults.append(patch);
        } else {
            // Search layers
            foreach (const KonfytSoundPreset& preset, patch->presets) {
                if (preset.name.toLower().contains(str)) {
                    patchResults.append(patch);
                    break;
                }
            }
        }
    }
    buildPatchTree_results();

    // Search all the SFZs
    sfzResults.clear();
    foreach (KfSoundPtr sfz, mAllSfzs) {
        if (sfz->filename.toLower().contains(str)) {
            sfzResults.append(sfz);
        }
    }
    buildSfzTree_results();
}

/* Returns a list of patches from the search results. */
QList<KfSoundPtr> konfytDatabase::getResultsPatches()
{
    return patchResults;
}

QList<KfSoundPtr> konfytDatabase::getResultsSfz()
{
    return sfzResults;
}

/* Returns a list of soundfonts from the search results.
 * The soundfonts only contain the programs matching the search. */
QList<KfSoundPtr> konfytDatabase::getResultsSfonts()
{
    return sfontResults;
}

int konfytDatabase::getNumSfontsResults()
{
    return sfontResults.count();
}

int konfytDatabase::getNumSfontProgramResults()
{
    int programs = 0;
    foreach (KfSoundPtr sf, sfontResults) {
        programs += sf->presets.count();
    }
    return programs;
}

int konfytDatabase::getNumPatchesResults()
{
    return patchResults.count();
}

int konfytDatabase::getNumSfzResults()
{
    return sfzResults.count();
}



