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

#ifndef REMOTESCANNER_H
#define REMOTESCANNER_H

#include "konfytFluidsynthEngine.h"
#include "konfytStructs.h"

#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>

#define REMOTE_SCANNER_SOCKET_NAME "konfyt_scanner"

/* RemoteScannerServer starts a Konfyt process with the --scan option and also
 * starts a QLocalServer.
 * In the separate Konfyt process, RemoteScannerClient connects to the server
 * with QLocalSocket.
 * The server then sends a soundfont file name and waits for a reply.
 * The client loads the soundfont file, extracts info, and replies:
 * "soundfont x\n" followed by XML data representing the soundfont file, where
 * x is the length of the XML data.
 * If an error occurred, x is zero and no XML data is sent.
 * After receiving the soundfont reply, the server continues by sending the next
 * soundfont file name to be scanned.
 * If the separate Konfyt process crashes, the server automatically restarts
 * the process and assumes that the last sent soundfont file name was the cause
 * of the crash and cannot be loaded.
 */

class RemoteScannerServer : public QObject
{
    Q_OBJECT
public:
    explicit RemoteScannerServer(QObject *parent = nullptr);

    void scan(QStringList soundfonts);

signals:
    void print(QString msg);
    void scanStatus(QString msg);
    void finished();
    void newSoundfont(KfSoundPtr s);

private:
    QLocalServer server;
    QLocalSocket* socket = nullptr;
    QProcess process;
    bool running = false;

    QStringList sfontScanList;
    int sfontScanIndex = 0;
    int errors = 0;
    int successes = 0;

    void sendSfont();
    void stopProcess();
    void printFinished();

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onNewConnection();
    void onSocketReadyRead();
};

class RemoteScannerClient : public QObject
{
    Q_OBJECT
public:
    explicit RemoteScannerClient(QObject *parent = nullptr);

    void connectToServer();

signals:
    void timeout();

private:
    QLocalSocket socket;
    QElapsedTimer elapsed;
    KonfytFluidsynthEngine fluidsynth;
    QTimer timer;

private slots:
    void onSocketReadyRead();
    void onTimerTick();
};

#endif // REMOTESCANNER_H
