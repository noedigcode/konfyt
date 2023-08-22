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

#ifndef KONFYT_PROCESS_H
#define KONFYT_PROCESS_H

#include "konfytDefines.h"
#include "konfytProject.h"

#include <QProcess>
#include <QFileInfo>
#include <QListWidget>
#include <QTimer>

// =============================================================================

/* Class for running an external application. */
class KonfytProcess : public QObject
{
    Q_OBJECT

public:
    KonfytProcess();
    ~KonfytProcess();

    enum State { NOT_STARTED, STARTING, RUNNING, FINISHED, STOPPING, STOPPED, CRASHED, ERROR };

    QProcess process;
    ExternalApp appInfo;
    QString projectDir;

    QString expandedAppName();

    void start();
    void stop();
    State state();
    bool isRunning();

signals:
    void stateChanged();
    void errorOccurrred(QString errorString);
    void finished();

private:
    State mState = NOT_STARTED;

private slots:
    void onProcessStarted();
    void onErrorOccurred(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
};

// =============================================================================

class ExternalAppRunner : public QObject
{
    Q_OBJECT

public:
    typedef QSharedPointer<KonfytProcess> ProcessPtr;

    void setProject(ProjectPtr project);
    void runApp(int id);
    void stopApp(int id);
    KonfytProcess::State getAppState(int id);

signals:
    void print(QString msg);
    void appStateChanged(int id);

private:
    ProjectPtr mProject;
    QMap<int, ProcessPtr> processes;

    ProcessPtr addNewProcess(int id);

private slots:
    void onAppRemoved(int id);
    void onAppModified(int id);
    void onProcessStateChanged(int id);
    void onProcessFinished(int id);
    void onProcessErrorOccurred(int id, QString errorString);
};

// =============================================================================

class ExternalAppsListAdapter : public QObject
{
    Q_OBJECT

public:

    ExternalAppsListAdapter(QListWidget* listWidget, ExternalAppRunner* runner);

    void setProject(ProjectPtr project);
    int selectedAppId();
    void selectAppInList(int id);

signals:
    void listSelectionChanged(int id);
    void listItemClicked(int id);
    void listItemDoubleClicked(int id);
    void listItemCountChanged(int count);

private:
    QListWidget* mListWidget = nullptr;
    ProjectPtr mProject;
    ExternalAppRunner* mRunner = nullptr;
    QMap<int, QListWidgetItem*> itemIdMap;

    void updateItem(QListWidgetItem* item, ExternalApp app,
                    KonfytProcess::State state);

private slots:
    void onListWidgetCurrentItemChanged(QListWidgetItem* current,
                                        QListWidgetItem* last);
    void onListWidgetItemClicked(QListWidgetItem* item);
    void onListWidgetItemDoubleClicked(QListWidgetItem* item);

    void onAppAdded(int id);
    void onAppRemoved(int id);
    void onAppModified(int id);
    void onAppStateChanged(int id);
};

#endif // KONFYT_PROCESS_H
