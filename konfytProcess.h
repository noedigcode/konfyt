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

#ifndef KONFYT_PROCESS_H
#define KONFYT_PROCESS_H

#include <QProcess>
#include <QFileInfo>
#include "konfytDefines.h"
#include <iostream>

enum konfytProcessState {
    KONFYTPROCESS_NOT_STARTED = 0,
    KONFYTPROCESS_RUNNING = 1,
    KONFYTPROCESS_FINISHED = 2,
    KONFYTPROCESS_STOPPED = 3
};


/* Class for running an external application. */
class konfytProcess : public QObject
{
    Q_OBJECT

public:
    konfytProcess();

    QProcess process;
    QString appname;
    QString dir;
    QString projectDir;
    QStringList args;

    void start();
    void stop();
    konfytProcessState getState();
    bool isRunning();

    QString toString_appAndArgs();

private:
    konfytProcessState state;

public slots:
    void processStarted();
    void processFinished(int exitCode);

signals:
    void started(konfytProcess* process);
    void finished(konfytProcess* process);

};

#endif // KONFYT_PROCESS_H
