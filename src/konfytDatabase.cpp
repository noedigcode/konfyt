/******************************************************************************
 *
 * Copyright 2022 Gideon van der Kolf
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
// KonfytDatabaseWorker
// ============================================================================

KonfytDatabaseWorker::KonfytDatabaseWorker()
{

}

void KonfytDatabaseWorker::setSfontIgnoreList(QList<KfSoundPtr> sfonts)
{
    sfontIgnoreList.clear();
    foreach (KfSoundPtr sf, sfonts) {
        sfontIgnoreList.append(sf->filename);
    }
}

KfSoundPtr KonfytDatabaseWorker::patchFromFile(QString filename)
{
    KfSoundPtr ret;

    KonfytPatch p;
    if (p.loadPatchFromFile(filename)) {
        ret.reset(new KonfytSound(KfSoundTypePatch));
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
void KonfytDatabaseWorker::scan(QObject* context,
                                std::function<void ()> callback)
{
    runInThread(this, [=]()
    {
        scanSfzs();
        scanPatches();
        scanSfonts();

        // Create remote scanner and scan all soundfonts discovered by scanSfonts().

        RemoteScannerServer* scanner = new RemoteScannerServer();
        connect(scanner, &RemoteScannerServer::print,
                this, &KonfytDatabaseWorker::print);
        connect(scanner, &RemoteScannerServer::scanStatus,
                this, &KonfytDatabaseWorker::scanStatus);
        connect(scanner, &RemoteScannerServer::finished, this, [=]()
        {
            scanner->deleteLater();
            runInThread(context, callback);
        });
        connect(scanner, &RemoteScannerServer::newSoundfont, this, [=](KfSoundPtr s)
        {
            sfontResults.append(s);
        });
        scanner->scan(sfontsToLoad);
    });
}

/* Create a konfytSoundfont object from a filename by loading it into
 * Fluidsynth in order to extract soundfont info, returning it with a signal so
 * the rest of the application can continue during this potentially long
 * operation. */
void KonfytDatabaseWorker::requestSfontFromFile(QString filename)
{
    runInThread(this, [=]()
    {
        KfSoundPtr newSfont = fluidsynth.soundfontFromFile(filename);
        emit sfontFromFileFinished(newSfont);
    });
}

/* Scans directory and subdirectories recursively and add all files with
 * specified suffixes to the list. */
void KonfytDatabaseWorker::scanDirForFiles(QString dirname, QStringList suffixes,
                                           QStringList &list)
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

/* Scans sfontDir for soundfont files. All discovered soundfont file names are
 * added to the sfontsToLoad list which will be used by the remote scanner
 * elsewhere to actually extract info from the files.
 * Any files in the ignore list will not be added. */
void KonfytDatabaseWorker::scanSfonts()
{
    QStringList sfontSuffix = {"sf2", "sf3"};
    sfontResults.clear();
    sfontsToLoad.clear();

    emit scanStatus("Scanning for soundfonts in " + sfontDir);
    QStringList sfontPaths;
    scanDirForFiles(sfontDir, sfontSuffix, sfontPaths);

    emit scanStatus("Loading new soundfonts...");
    // Load each soundfont that is not in the ignore list.
    foreach (QString path, sfontPaths) {
        if (!sfontIgnoreList.contains(path)) {
            sfontsToLoad.append(path);
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
        KfSoundPtr sfz = KfSoundPtr(new KonfytSound(KfSoundTypeSfz));
        sfz->filename = path;
        sfz->name = QFileInfo(sfz->filename).fileName();
        sfzResults.append(sfz);
    }
}

void KonfytDatabaseWorker::scanPatches()
{
    QStringList patchSuffix = {KONFYT_PATCH_SUFFIX};
    patchResults.clear();

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

void KonfytDatabaseWorker::runInThread(QObject *context,
                                       std::function<void ()> func)
{
    QMetaObject::invokeMethod(context, func, Qt::QueuedConnection);
}



// ============================================================================
// KonfytDatabase
// ============================================================================

KonfytDatabase::KonfytDatabase()
{
    connect(&worker, &KonfytDatabaseWorker::print,
            this, &KonfytDatabase::userMessageFromWorker);

    connect(&worker, &KonfytDatabaseWorker::scanStatus,
            this, &KonfytDatabase::scanStatusFromWorker);

    qRegisterMetaType<KfSoundPtr>("KfSoundPtr");
    connect(&worker, &KonfytDatabaseWorker::sfontFromFileFinished,
            this, &KonfytDatabase::onSfontInfoLoadedFromFile);

    worker.moveToThread(&workerThread);
    workerThread.start();
}

KonfytDatabase::~KonfytDatabase()
{
    workerThread.quit();
    workerThread.wait();
}

QList<KfSoundPtr> KonfytDatabase::allSoundfonts()
{
    return mAllSoundfonts;
}

int KonfytDatabase::soundfontCount()
{
    return mAllSoundfonts.count();
}

QList<KfSoundPtr> KonfytDatabase::allPatches()
{
    return mAllPatches;
}

int KonfytDatabase::patchCount()
{
    return mAllPatches.count();
}

QList<KfSoundPtr> KonfytDatabase::allSfzs()
{
    return mAllSfzs;
}

int KonfytDatabase::sfzCount()
{
    return mAllSfzs.count();
}

void KonfytDatabase::setSoundfontsDir(QString path)
{
    mSfontsDir = path;
}

void KonfytDatabase::setSfzDir(QString path)
{
    mSfzDir = path;
}

void KonfytDatabase::setPatchesDir(QString path)
{
    mPatchesDir = path;
}

void KonfytDatabase::userMessageFromWorker(QString msg)
{
    print(msg);
}

void KonfytDatabase::scanStatusFromWorker(QString msg)
{
    emit scanStatus(msg);
}

/* Starts scanning directories and returns. Finished signal will be emitted
 * later when done. */
void KonfytDatabase::scan()
{
    emit scanStatus("Starting scan...");

    // Signal the worker to start scanning.
    // We pass the current sfontlist as ignoreList parameter to not reload
    // soundfonts we already have info on.
    worker.sfontDir = mSfontsDir;
    worker.setSfontIgnoreList(mAllSoundfonts);
    worker.sfzDir = mSfzDir;
    worker.patchDir = mPatchesDir;
    worker.scan(this, [=](){ onScanFinished(); });
    // We now wait for the scanDirsFinished signal from the worker.
    // See the onScanFinished() slot.
}

void KonfytDatabase::onScanFinished()
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

void KonfytDatabase::onSfontInfoLoadedFromFile(KfSoundPtr sfont)
{
    emit sfontInfoLoadedFromFile(sfont);
}

void KonfytDatabase::addPatch(QString filename)
{
    KfSoundPtr patch = worker.patchFromFile(filename);
    if (patch) {
        addPatch(patch);
        buildPatchTree();
    }
}

void KonfytDatabase::removePatch(KfSoundPtr patch)
{
    mAllPatches.removeAll(patch);
    buildPatchTree();
}

void KonfytDatabase::addSfont(KfSoundPtr sf)
{
    mAllSoundfonts.append(sf);
}

void KonfytDatabase::addSfz(KfSoundPtr sfz)
{
    mAllSfzs.append(sfz);
}

void KonfytDatabase::addPatch(KfSoundPtr patch)
{
    mAllPatches.append(patch);
}

/* Build a tree of all list items based on their directory structure. */
void KonfytDatabase::buildTree(KonfytDbTree *tree, const QList<KfSoundPtr> &list,
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
        item->name = sound->name;
        item->path = sound->filename;
        item->data = sound;
    }

    compactTree(tree->root);
}

/* Compact a tree, by combining branches with their children
 * if they only have single children. E.g. a->b->c.sfz becomes a/b->c.sfz */
void KonfytDatabase::compactTree(KfDbTreeItemPtr item)
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
void KonfytDatabase::clearDatabase()
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

void KonfytDatabase::clearDatabase_exceptSoundfonts()
{
    // Clear patches
    mAllPatches.clear();
    patchResults.clear();

    // Clear sfz
    mAllSfzs.clear();
    sfzResults.clear();
}

/* Loads soundfont info from file in worker thread. Signal is emitted when done. */
void KonfytDatabase::loadSfontInfoFromFile(QString filename)
{
    worker.requestSfontFromFile(filename);
    // The worker thread will now load the soundfont and emit a signal when done
}

/* Save the database to an xml file. Returns true if success, false if not. */
bool KonfytDatabase::saveDatabaseToFile(QString filename)
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
        soundfontToXml(sf, &stream);
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

    stream.writeEndElement(); // database

    stream.writeEndDocument();

    file.close();
    return true;
}

/* Build a tree of all sfz items, based on their directory structure. */
void KonfytDatabase::buildSfzTree()
{
    buildTree(&sfzTree, mAllSfzs, mSfzDir);
}

void KonfytDatabase::buildSfzTree_results()
{
    buildTree(&sfzTree_results, sfzResults, mSfzDir);
}

void KonfytDatabase::buildSfontTree()
{
    buildTree(&sfontTree, mAllSoundfonts, mSfontsDir);
}

void KonfytDatabase::buildSfontTree_results()
{
    buildTree(&sfontTree_results, sfontResults, mSfontsDir);
}

void KonfytDatabase::buildPatchTree()
{
    buildTree(&patchTree, mAllPatches, mPatchesDir);
}

void KonfytDatabase::buildPatchTree_results()
{
    buildTree(&patchTree_results, patchResults, mPatchesDir);
}

/* Clears the database and loads it from a single saved database xml file. */
bool KonfytDatabase::loadDatabaseFromFile(QString filename)
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

                KfSoundPtr sf = soundfontFromXml(&r);
                addSfont(sf);

            } else if (r.name() == "patch") {

                KfSoundPtr patch(new KonfytSound(KfSoundTypePatch));
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

                KfSoundPtr sfz(new KonfytSound(KfSoundTypeSfz));
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

void KonfytDatabase::soundfontToXml(KfSoundPtr sf, QXmlStreamWriter *stream)
{
    stream->writeStartElement("soundfont");

    stream->writeAttribute("filename", sf->filename);
    stream->writeAttribute("name", sf->name);

    // All the programs ("presets")
    foreach (const KonfytSoundPreset &preset, sf->presets) {

        stream->writeStartElement("preset");

        stream->writeTextElement("bank", n2s(preset.bank));
        stream->writeTextElement("program", n2s(preset.program));
        stream->writeTextElement("name", preset.name);

        stream->writeEndElement();
    }

    stream->writeEndElement();
}

KfSoundPtr KonfytDatabase::soundfontFromXml(QXmlStreamReader *r)
{
    KfSoundPtr sf(new KonfytSound(KfSoundTypeSoundfont));
    sf->filename = r->attributes().value("filename").toString();
    sf->name = r->attributes().value("name").toString();

    while (r->readNextStartElement()) { // preset

        if (r->name() == "preset") {

            KonfytSoundPreset p;

            while (r->readNextStartElement()) {
                if (r->name() == "bank") {
                    p.bank = r->readElementText().toInt();
                } else if (r->name() == "program") {
                    p.program = r->readElementText().toInt();
                } else if (r->name() == "name") {
                    p.name = r->readElementText();
                }
            }

            sf->presets.append(p);

        } else {
            r->skipCurrentElement();
        }
    }
    return sf;
}

/* Search all soundfonts (and their programs), SFZs and patches for the specified
 * string. Results can be accessed with the getResults functions. */
void KonfytDatabase::search(QString str)
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
                KfSoundPtr sfresult(new KonfytSound(KfSoundTypeSoundfont));
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
QList<KfSoundPtr> KonfytDatabase::getResultsPatches()
{
    return patchResults;
}

QList<KfSoundPtr> KonfytDatabase::getResultsSfz()
{
    return sfzResults;
}

/* Returns a list of soundfonts from the search results.
 * The soundfonts only contain the programs matching the search. */
QList<KfSoundPtr> KonfytDatabase::getResultsSfonts()
{
    return sfontResults;
}

int KonfytDatabase::getNumSfontsResults()
{
    return sfontResults.count();
}

int KonfytDatabase::getNumSfontProgramResults()
{
    int programs = 0;
    foreach (KfSoundPtr sf, sfontResults) {
        programs += sf->presets.count();
    }
    return programs;
}

int KonfytDatabase::getNumPatchesResults()
{
    return patchResults.count();
}

int KonfytDatabase::getNumSfzResults()
{
    return sfzResults.count();
}



