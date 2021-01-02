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

#include "konfytProcess.h"

konfytProcess::konfytProcess()
{
    connect(&process, &QProcess::started, this, &konfytProcess::processStarted);
    connect(&process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &konfytProcess::processFinished);

    state = KONFYTPROCESS_NOT_STARTED;
}

/* Slot for signal from process when it is started. */
void konfytProcess::processStarted()
{
    state = KONFYTPROCESS_RUNNING;
    emit started(this);
}

/* Slot for signal from process when it is finished. */
void konfytProcess::processFinished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/)
{
    if (state != KONFYTPROCESS_STOPPED) {
        state = KONFYTPROCESS_FINISHED;
    }
    emit finished(this);
}

void konfytProcess::start()
{
    // Replace variables in strings
    QString runAppname = appname;
    runAppname.replace(STRING_PROJECT_DIR, projectDir);

    process.start(runAppname);
}

void konfytProcess::stop()
{
    if (state == KONFYTPROCESS_RUNNING) {
        state = KONFYTPROCESS_STOPPED;
        process.kill();
    }
}

konfytProcessState konfytProcess::getState()
{
    return state;
}

bool konfytProcess::isRunning()
{
    if (state == KONFYTPROCESS_RUNNING) {
        return true;
    } else {
        return false;
    }
}

QString konfytProcess::toString_appAndArgs()
{
    QString out = appname;
    return out;
}
