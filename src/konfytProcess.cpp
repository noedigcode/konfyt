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

#include "konfytProcess.h"

#include <QDebug>

// =============================================================================
// KonfytProcess
// =============================================================================

KonfytProcess::KonfytProcess()
{
    connect(&process, &QProcess::started,
            this, &KonfytProcess::onProcessStarted);
    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &KonfytProcess::onProcessFinished);
    connect(&process, &QProcess::errorOccurred,
            this, &KonfytProcess::onErrorOccurred);
}

KonfytProcess::~KonfytProcess()
{
    // Prevent crashes at shutdown
    this->blockSignals(true);
}

/* Returns the app name with tags (e.g. $PROJ_DIR$) replaced with actual values. */
QString KonfytProcess::expandedAppName()
{
    QString ret = appInfo.command;
    ret.replace(STRING_PROJECT_DIR, projectDir);
    return ret;
}

void KonfytProcess::start()
{
    mState = STARTING;
    emit stateChanged();

    process.start(expandedAppName());
}

void KonfytProcess::stop()
{
    // First time, try to terminate gracefully.
    // Second time, kill.

    if (mState == STOPPING) {
        // Already tried to stop. Now use kill.
        emit stateChanged();
        process.kill();
    } else {
        // Not stopping already. Try terminate.
        mState = STOPPING;
        emit stateChanged();
        process.terminate();
    }
}

KonfytProcess::State KonfytProcess::state()
{
    return mState;
}

bool KonfytProcess::isRunning()
{
    if (mState == RUNNING) {
        return true;
    } else {
        return false;
    }
}

void KonfytProcess::onProcessStarted()
{
    mState = RUNNING;
    emit stateChanged();
}

void KonfytProcess::onErrorOccurred(QProcess::ProcessError error)
{
    // Do not react to Crashed, as this is handled in onProcessFinished().
    if (error != QProcess::Crashed) {
        mState = ERROR;
        emit stateChanged();
        emit errorOccurrred(process.errorString());
    }
}

void KonfytProcess::onProcessFinished(int /*exitCode*/,
                                      QProcess::ExitStatus exitStatus)
{
    // Ensure that when app was stopped intentionally by user, then state should
    // be STOPPED.
    // Otherwise, it should either be FINISHED or CRASHED.

    if (mState == STOPPING) { mState = STOPPED; }

    if (mState != STOPPED) {
        if (exitStatus == QProcess::CrashExit) {
            mState = CRASHED;
        } else {
            mState = FINISHED;
        }
    }

    emit stateChanged();
    emit finished();
}

// =============================================================================
// ExternalAppRunner
// =============================================================================

void ExternalAppRunner::setProject(ProjectPtr project)
{
    if (mProject) {
        disconnect(mProject.data(), nullptr, this, nullptr);
        processes.clear();
    }
    mProject = project;
    connect(mProject.data(), &KonfytProject::externalAppRemoved,
            this, &ExternalAppRunner::onAppRemoved);
    connect(mProject.data(), &KonfytProject::externalAppModified,
            this, &ExternalAppRunner::onAppModified);

    // Run all apps which are set to run at startup
    foreach (int id, mProject->getExternalAppIds()) {
        ExternalApp app = mProject->getExternalApp(id);
        if (app.runAtStartup) {
            print("Auto-starting external app: " + app.friendlyName);
            runApp(id);
        }
    }
}

void ExternalAppRunner::runApp(int id)
{
    if (!mProject) {
        print("ERROR: No project set.");
        return;
    }
    if (!mProject->hasExternalAppWithId(id)) {
        print(QString("ERROR: Project does not contain an external app with id %1.")
              .arg(id));
        return;
    }

    ProcessPtr p = processes.value(id);
    if (!p) { p = addNewProcess(id); }

    if (!p->isRunning()) {
        print("Running external app: " + p->appInfo.friendlyName);
        print("with command: " + p->appInfo.command);
        p->start();
    }
}

void ExternalAppRunner::stopApp(int id)
{
    ProcessPtr p = processes.value(id);
    if (!p) { return; }

    p->stop();
}

KonfytProcess::State ExternalAppRunner::getAppState(int id)
{
    KonfytProcess::State ret = KonfytProcess::NOT_STARTED;

    ProcessPtr p = processes.value(id);
    if (p) {
        ret = p->state();
    }

    return ret;
}

ExternalAppRunner::ProcessPtr ExternalAppRunner::addNewProcess(int id)
{
    ProcessPtr p(new KonfytProcess());

    if (mProject) {
        p->projectDir = mProject->getDirPath();
        if (mProject->hasExternalAppWithId(id)) {
            p->appInfo = mProject->getExternalApp(id);
        } else {
            print(QString("ERROR: addNewProcess: project has no external app id %1").arg(id));
        }
    } else {
        print("ERROR: addNewProcess: no project set.");
    }

    connect(p.data(), &KonfytProcess::stateChanged, this, [=]()
    {
        onProcessStateChanged(id);
    });
    connect(p.data(), &KonfytProcess::finished, this, [=]()
    {
        onProcessFinished(id);
    });
    connect(p.data(), &KonfytProcess::errorOccurrred, this, [=](QString errorString)
    {
        onProcessErrorOccurred(id, errorString);
    });

    processes.insert(id, p);
    return p;
}

void ExternalAppRunner::onAppRemoved(int id)
{
    processes.remove(id);
}

void ExternalAppRunner::onAppModified(int id)
{
    ProcessPtr p = processes.value(id);
    if (!p) { return; }

    if (!mProject) { return; }

    p->appInfo = mProject->getExternalApp(id);
}

void ExternalAppRunner::onProcessStateChanged(int id)
{
    ProcessPtr p = processes.value(id);
    if (!p) { return; }
    emit appStateChanged(id);
}

void ExternalAppRunner::onProcessFinished(int id)
{
    ProcessPtr p = processes.value(id);
    if (!p) { return; }

    // STOPPED means the user chose to stop the app. Only restart the app if
    // FINISHED, CRASHED or ERROR.
    if (p->appInfo.autoRestart) {
        if ( (p->state() == KonfytProcess::FINISHED) ||
             (p->state() == KonfytProcess::CRASHED) ||
             (p->state() == KonfytProcess::ERROR) ) {

            QString name = p->appInfo.friendlyName;
            int delay = 1000;
            print(QString("External app will be auto-restarted in %1ms: %2")
                  .arg(delay).arg(name));
            QTimer::singleShot(1000, this, [=]()
            {
                print("Auto-restarting external app: " + name);
                runApp(id);
            });
        }
    }
}

void ExternalAppRunner::onProcessErrorOccurred(int id, QString errorString)
{
    ProcessPtr p = processes.value(id);
    if (!p) { return; }
    print(QString("App error: %1: %2").arg(p->appInfo.command).arg(errorString));
}

// =============================================================================
// ExternalAppsListAdapter
// =============================================================================

ExternalAppsListAdapter::ExternalAppsListAdapter(QListWidget *listWidget, ExternalAppRunner *runner) :
    mListWidget(listWidget), mRunner(runner)
{
    connect(mListWidget, &QListWidget::currentItemChanged,
            this, &ExternalAppsListAdapter::onListWidgetCurrentItemChanged);
    connect(mListWidget, &QListWidget::itemClicked,
            this, &ExternalAppsListAdapter::onListWidgetItemClicked);
    connect(mListWidget, &QListWidget::itemDoubleClicked,
            this, &ExternalAppsListAdapter::onListWidgetItemDoubleClicked);

    connect(runner, &ExternalAppRunner::appStateChanged,
            this, &ExternalAppsListAdapter::onAppStateChanged);
}

void ExternalAppsListAdapter::setProject(ProjectPtr project)
{
    if (mProject) {
        disconnect(mProject.data(), nullptr, this, nullptr);
    }
    mProject = project;
    connect(mProject.data(), &KonfytProject::externalAppAdded,
            this, &ExternalAppsListAdapter::onAppAdded);
    connect(mProject.data(), &KonfytProject::externalAppRemoved,
            this, &ExternalAppsListAdapter::onAppRemoved);
    connect(mProject.data(), &KonfytProject::externalAppModified,
            this, &ExternalAppsListAdapter::onAppModified);

    // Clear items and refill from project
    itemIdMap.clear();
    mListWidget->clear();
    emit listItemCountChanged(0);
    foreach (int id, mProject->getExternalAppIds()) {
        onAppAdded(id);
    }
}

int ExternalAppsListAdapter::selectedAppId()
{
    return itemIdMap.key(mListWidget->currentItem(), -1);
}

void ExternalAppsListAdapter::selectAppInList(int id)
{
    mListWidget->setCurrentItem( itemIdMap.value(id) );
}

void ExternalAppsListAdapter::updateItem(QListWidgetItem *item, ExternalApp app, KonfytProcess::State state)
{
    QString text = app.friendlyName;
    if (text.trimmed().isEmpty()) {
        text = app.command;
    }
    if (text.trimmed().isEmpty()) {
        text = "(no name)";
    }

    if (state == KonfytProcess::RUNNING) {
        text.prepend("[running] ");
    } else if (state == KonfytProcess::STARTING) {
        text.prepend("[starting] ");
    } else if (state == KonfytProcess::FINISHED) {
        text.prepend("[finished] ");
    } else if (state == KonfytProcess::STOPPING) {
        text.prepend("[stopping] ");
    } else if (state == KonfytProcess::STOPPED) {
        text.prepend("[stopped] ");
    } else if (state == KonfytProcess::CRASHED) {
        text.prepend("[crashed] ");
    } else if (state == KonfytProcess::ERROR) {
        text.prepend("[error] ");
    }

    item->setText(text);
}

void ExternalAppsListAdapter::onListWidgetCurrentItemChanged(
        QListWidgetItem *current, QListWidgetItem* /*last*/)
{
    int id = itemIdMap.key(current, -1);
    emit listSelectionChanged(id);
}

void ExternalAppsListAdapter::onListWidgetItemClicked(QListWidgetItem *item)
{
    int id = itemIdMap.key(item, -1);
    if (id >= 0) {
        emit listItemClicked(id);
    }
}

void ExternalAppsListAdapter::onListWidgetItemDoubleClicked(QListWidgetItem *item)
{
    int id = itemIdMap.key(item, -1);
    if (id >= 0) {
        emit listItemDoubleClicked(id);
    }
}

void ExternalAppsListAdapter::onAppAdded(int id)
{
    ExternalApp app = mProject->getExternalApp(id);
    QListWidgetItem* item = new QListWidgetItem();
    updateItem(item, app, mRunner->getAppState(id));
    itemIdMap.insert(id, item);
    mListWidget->addItem(item);
    emit listItemCountChanged(itemIdMap.count());
}

void ExternalAppsListAdapter::onAppRemoved(int id)
{
    QListWidgetItem* item = itemIdMap.value(id);
    itemIdMap.remove(id);
    if (item) { delete item; }
    emit listItemCountChanged(itemIdMap.count());
}

void ExternalAppsListAdapter::onAppModified(int id)
{
    QListWidgetItem* item = itemIdMap.value(id);
    if (!item) { return; }
    ExternalApp app = mProject->getExternalApp(id);
    updateItem(item, app, mRunner->getAppState(id));
}

void ExternalAppsListAdapter::onAppStateChanged(int id)
{
    QListWidgetItem* item = itemIdMap.value(id);
    if (!item) { return; }
    ExternalApp app = mProject->getExternalApp(id);
    updateItem(item, app, mRunner->getAppState(id));
}

