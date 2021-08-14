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

#ifndef KONFYT_PROCESS_H
#define KONFYT_PROCESS_H

#include "konfytDefines.h"

#include <QProcess>
#include <QFileInfo>


/* Class for running an external application. */
class KonfytProcess : public QObject
{
    Q_OBJECT

public:
    KonfytProcess();

    enum State { NOT_STARTED, RUNNING, FINISHED, STOPPED };

    QProcess process;
    QString appname;
    QString projectDir;

    void start();
    void stop();
    State state();
    bool isRunning();

    QString toString();

public slots:
    void processStarted();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

signals:
    void started(KonfytProcess* process);
    void finished(KonfytProcess* process);

private:
    State mState = NOT_STARTED;

};

#endif // KONFYT_PROCESS_H
