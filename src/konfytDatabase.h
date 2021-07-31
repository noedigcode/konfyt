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

#ifndef SFDATABASE_H
#define SFDATABASE_H

#include "konfytDbTree.h"
#include "konfytPatch.h"

#include <fluidsynth.h>

#include <QDir>
#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define XML_DATABASE "database"


// ============================================================================
// konfytDatabaseWorker
// ============================================================================

class KonfytDatabaseWorker : public QObject
{
    Q_OBJECT
public:
    explicit KonfytDatabaseWorker();

    fluid_synth_t* synth = NULL;

    QString sfontDir;
    QList<KfSoundPtr> sfontIgnoreList;
    QString sfzDir;
    QString patchDir;

    QList<KfSoundPtr> sfontResults;
    QList<KfSoundPtr> sfzResults;
    QList<KfSoundPtr> patchResults;

    KfSoundPtr patchFromFile(QString filename);

public slots:
    void scan();
    void requestSfontFromFile(QString filename);

signals:
    void print(QString msg);
    void scanFinished();
    void scanStatus(QString msg);
    void sfontFromFileFinished(KfSoundPtr sfont);

private:
    void scanDirForFiles(QString dirname, QStringList suffixes, QStringList &list);
    void scanSfonts();
    void scanSfzs();
    void scanPatches();

    KfSoundPtr sfontFromFile(QString filename);
};

// ============================================================================
// konfytDatabase
// ============================================================================

class konfytDatabase : public QObject
{
    Q_OBJECT

public:
    konfytDatabase();
    ~konfytDatabase();

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

signals:
    // Signals intended for outside world
    void print(QString message);
    void scanStatus(QString msg);
    void scanFinished();
    void sfontInfoLoadedFromFile(KfSoundPtr sf);
    // Signals intended for worker thread
    void start_scanDirs();
    void start_sfontFromFile(QString filename);

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

    fluid_settings_t* mFluidsynthSettings = NULL;
    fluid_synth_t* mFluidSynth = NULL;
    int initFluidsynth();

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

#endif // SFDATABASE_H
