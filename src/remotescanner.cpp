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

#include "remotescanner.h"

#include <konfytDatabase.h>


RemoteScannerServer::RemoteScannerServer(QObject *parent) : QObject(parent)
{
    process.setProgram(qApp->arguments().value(0));
    process.setArguments({"--scan"});

    connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
           this, &RemoteScannerServer::onProcessFinished);

    connect(&server, &QLocalServer::newConnection,
            this, &RemoteScannerServer::onNewConnection);
}

void RemoteScannerServer::scan(QStringList soundfonts)
{
    sfontScanList = soundfonts;
    sfontScanIndex = 0;

    if (soundfonts.isEmpty()) {
        print("No soundfonts to scan.");
        emit finished();
        return;
    }

    QLocalServer::removeServer(REMOTE_SCANNER_SOCKET_NAME);
    if (server.listen(REMOTE_SCANNER_SOCKET_NAME)) {
        scanStatus("Starting scan process...");
    } else {
        print("Could not start server: " + server.errorString());
        print("Soundfont scanning failed.");
        emit finished();
        return;
    }

    running = true;
    process.start();
}

void RemoteScannerServer::sendSfont()
{
    // Send the file name of the next soundfont file to be scanned to the client.
    QString sf = sfontScanList.value(sfontScanIndex);
    scanStatus(sf);
    socket->write(QString("%1\n").arg(sf).toLocal8Bit());
}

void RemoteScannerServer::stopProcess()
{
    running = false;
    process.terminate();
}

void RemoteScannerServer::printFinished()
{
    scanStatus("Finished scanning soundfonts.");
    print("Finished scanning soundfonts.");
    print(QString("    Errors: %1").arg(errors));
    print(QString("    Successes: %1").arg(successes));
}

void RemoteScannerServer::onProcessFinished(int /*exitCode*/,
                                            QProcess::ExitStatus /*exitStatus*/)
{
    if (!running) { return; }

    print("Scan process crashed. Restarting...");
    print("Error loading soundfont: " + sfontScanList.value(sfontScanIndex));
    scanStatus("Scan process crashed. Restarting...");

    sfontScanIndex++;
    errors++;

    if (sfontScanIndex < sfontScanList.count()) {
        process.start();
    } else {
        printFinished();
        emit finished();
    }
}

void RemoteScannerServer::onNewConnection()
{
    scanStatus("New local socket connection");

    socket = server.nextPendingConnection();
    connect(socket, &QLocalSocket::readyRead,
            this, &RemoteScannerServer::onSocketReadyRead);

    if (sfontScanIndex < sfontScanList.count()) {
        sendSfont();
    } else {
        printFinished();
        stopProcess();
        emit finished();
    }
}

void RemoteScannerServer::onSocketReadyRead()
{
    while (!socket->atEnd()) {
        QByteArray line = socket->readLine();
        line.replace("\n", "");

        // See RemoteScannerClient::onSocketReadyRead() for format of message

        if (line.startsWith("soundfont")) {
            int len = line.split(' ').value(1).toInt();

            if (len == 0) {
                print("Error loading soundfont: " + sfontScanList.value(sfontScanIndex));
                errors++;
            } else {
                successes++;
                QByteArray xml = socket->read(len);
                QXmlStreamReader r(xml);
                r.readNextStartElement();
                KfSoundPtr s = KonfytDatabase::soundfontFromXml(&r);
                emit newSoundfont(s);
            }

            sfontScanIndex++;
            if (sfontScanIndex < sfontScanList.count()) {
                sendSfont();
            } else {
                printFinished();
                stopProcess();
                emit finished();
            }
        }
    }
}

RemoteScannerClient::RemoteScannerClient(QObject *parent) : QObject(parent)
{
    connect(&socket, &QLocalSocket::readyRead,
            this, &RemoteScannerClient::onSocketReadyRead);
    connect(&timer, &QTimer::timeout, this, &RemoteScannerClient::onTimerTick);
}

void RemoteScannerClient::connectToServer()
{
    socket.connectToServer(REMOTE_SCANNER_SOCKET_NAME);
    elapsed.start();
    timer.start(1000);
}

void RemoteScannerClient::onSocketReadyRead()
{
    QByteArray xml;
    QByteArray line = socket.readLine();
    line.replace("\n", "");
    KfSoundPtr sf = fluidsynth.soundfontFromFile(line);
    if (sf) {
        QXmlStreamWriter w(&xml);
        KonfytDatabase::soundfontToXml(sf, &w);
    }
    // Send "soundfont x\n" where x is the length of the XML data representing
    // the soundfont object, followed by the XML data.
    // If the soundfont could not be loaded, x is zero and no XML data is sent.
    QByteArray tosend;
    tosend.append(QString("soundfont %1\n").arg(xml.length()).toLocal8Bit());
    tosend.append(xml);
    socket.write(tosend);

    elapsed.start(); // Reset comms timeout
}

void RemoteScannerClient::onTimerTick()
{
    if (elapsed.hasExpired(10000)) {
        emit timeout();
    }
}
