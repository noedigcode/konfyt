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

KonfytProcess::KonfytProcess()
{
    connect(&process, &QProcess::started, this, &KonfytProcess::processStarted);
    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &KonfytProcess::processFinished);
}

/* Slot for signal from process when it is started. */
void KonfytProcess::processStarted()
{
    mState = RUNNING;
    emit started(this);
}

/* Slot for signal from process when it is finished. */
void KonfytProcess::processFinished(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/)
{
    if (mState != STOPPED) {
        mState = FINISHED;
    }
    emit finished(this);
}

void KonfytProcess::start()
{
    // Replace variables in strings
    QString runAppname = appname;
    runAppname.replace(STRING_PROJECT_DIR, projectDir);

    process.start(runAppname);
}

void KonfytProcess::stop()
{
    if (mState == RUNNING) {
        mState = STOPPED;
        process.kill();
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

QString KonfytProcess::toString()
{
    QString out = appname;
    return out;
}
