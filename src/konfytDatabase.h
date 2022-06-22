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

#ifndef KONFYT_DATABASE_H
#define KONFYT_DATABASE_H

#include "remotescanner.h"
#include "konfytDbTree.h"
#include "konfytFluidsynthEngine.h"
#include "konfytPatch.h"

#include <QDir>
#include <QList>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QThread>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define XML_DATABASE "database"


// ============================================================================
// KonfytDatabaseWorker
// ============================================================================

class KonfytDatabaseWorker : public QObject
{
    Q_OBJECT
public:
    explicit KonfytDatabaseWorker();

    KonfytFluidsynthEngine fluidsynth;

    QString sfontDir;
    QString sfzDir;
    QString patchDir;

    void setSfontIgnoreList(QList<KfSoundPtr> sfonts);

    QList<KfSoundPtr> sfontResults;
    QList<KfSoundPtr> sfzResults;
    QList<KfSoundPtr> patchResults;

    KfSoundPtr patchFromFile(QString filename);

signals:
    // Signals emitted by this class
    void print(QString msg);
    void scanFinished();
    void scanStatus(QString msg);
    void sfontFromFileFinished(KfSoundPtr sfont);
    // Signals to trigger work in this class/thread
    void scan();
    void requestSfontFromFile(QString filename);

private slots:
    void doScan();
    void doRequestSfontFromFile(QString filename);

private:
    QStringList sfontsToLoad;
    QStringList sfontIgnoreList;

    void scanDirForFiles(QString dirname, QStringList suffixes, QStringList &list);
    void scanSfonts();
    void scanSfzs();
    void scanPatches();
};

// ============================================================================
// KonfytDatabase
// ============================================================================

class KonfytDatabase : public QObject
{
    Q_OBJECT

public:
    KonfytDatabase();
    ~KonfytDatabase();
class
    QList<KfSoundPtr> allSoundfonts();
    int soundfontCount();
    QList<KfSoundPtr> allPatches();
    int patchCount();
    QList<KfSoundPtr> allSfzs();
    int sfzCount();
    KonfytDbTree sfzTree;
    KonfytDbTree sfzTree_results;
    KonfytDbTree sfontTree;
    KonfytDbTree sfontTree_results;
    KonfytDbTree patchTree;
    KonfytDbTree patchTree_results;

    void setSoundfontsDir(QString path);
    void setSfzDir(QString path);
    void setPatchesDir(QString path);
    void scan();

    void loadSfontInfoFromFile(QString filename);

    void clearDatabase();
    void clearDatabase_exceptSoundfonts();

    bool saveDatabaseToFile(QString filename);
    bool loadDatabaseFromFile(QString filename);

    static void soundfontToXml(KfSoundPtr sf, QXmlStreamWriter* stream);
    static KfSoundPtr soundfontFromXml(QXmlStreamReader* r);

    // Search functionality
    void search(QString str);
    int getNumSfontsResults();
    int getNumSfontProgramResults();
    QList<KfSoundPtr> getResultsSfonts();
    int getNumPatchesResults();
    QList<KfSoundPtr> getResultsPatches();
    int getNumSfzResults();
    QList<KfSoundPtr> getResultsSfz();

    void addPatch(QString filename);
    void removePatch(KfSoundPtr patch);

signals:
    // Signals intended for outside world
    void print(QString message);
    void scanStatus(QString msg);
    void scanFinished();
    void sfontInfoLoadedFromFile(KfSoundPtr sf);

private slots:
    void onScanFinished();
    void onSfontInfoLoadedFromFile(KfSoundPtr sfont);
    void userMessageFromWorker(QString msg);
    void scanStatusFromWorker(QString msg);

private:
    QList<KfSoundPtr> mAllSoundfonts;
    QList<KfSoundPtr> mAllPatches;
    QList<KfSoundPtr> mAllSfzs;

    QList<KfSoundPtr> sfontResults;
    QList<KfSoundPtr> patchResults;
    QList<KfSoundPtr> sfzResults;

    QThread workerThread;
    KonfytDatabaseWorker worker;
    QString mSfontsDir;
    QString mSfzDir;
    QString mPatchesDir;

    void addSfont(KfSoundPtr sf);
    void addSfz(KfSoundPtr sfz);
    void addPatch(KfSoundPtr patch);

    void buildTree(KonfytDbTree* tree, const QList<KfSoundPtr> &list, QString rootPath);
    void buildSfzTree();
    void buildSfzTree_results();
    void buildSfontTree();
    void buildSfontTree_results();
    void buildPatchTree();
    void buildPatchTree_results();
    void compactTree(KfDbTreeItemPtr item);
};

#endif // KONFYT_DATABASE_H
