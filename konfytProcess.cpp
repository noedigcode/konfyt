#include "konfytProcess.h"
#include <QFileInfo>

konfytProcess::konfytProcess()
{
    connect(&process, SIGNAL(started()), this, SLOT(processStarted()));
    connect(&process, SIGNAL(finished(int)), this, SLOT(processFinished(int)));

    state = KONFYTPROCESS_NOT_STARTED;
}

// Slot for signal from process when it is started.
void konfytProcess::processStarted()
{
    state = KONFYTPROCESS_RUNNING;
    emit started(this);
}

// Slot for signal from process when it is finished.
void konfytProcess::processFinished(int exitCode)
{
    if (state != KONFYTPROCESS_STOPPED) {
        state = KONFYTPROCESS_FINISHED;
    }
    emit finished(this);
}

void konfytProcess::start()
{
    QFileInfo fi(dir);
    process.setWorkingDirectory(fi.absolutePath());

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
    for (int i=0; i<args.count(); i++) {
        out += " " + args.at(i);
    }
    return out;
}
