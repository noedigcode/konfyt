#ifndef KONFYT_PROCESS_H
#define KONFYT_PROCESS_H

#include <QProcess>
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
