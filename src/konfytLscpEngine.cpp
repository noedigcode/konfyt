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

#include "konfytLscpEngine.h"

KonfytLscpEngine::KonfytLscpEngine(QObject *parent) : KonfytBaseSoundEngine(parent)
{
    connect(&ls, &GidLs::print, this, &KonfytLscpEngine::print);
    connect(&ls, &GidLs::initialised, this, &KonfytLscpEngine::onLsInitialised);
}

KonfytLscpEngine::~KonfytLscpEngine()
{
    ls.deinit();
}

QString KonfytLscpEngine::engineName()
{
    return "LscpEngine";
}

void KonfytLscpEngine::initEngine(KonfytJackEngine *jackEngine)
{
    mJackEngine = jackEngine;

    ls.init();
}

QString KonfytLscpEngine::jackClientName()
{
    return ls.jackClientName();
}

int KonfytLscpEngine::addSfz(QString path)
{
    return ls.addSfzChannelAndPorts(path);
}

QString KonfytLscpEngine::pluginName(int id)
{
    return "LS_sfz_" + n2s(id);
}

QString KonfytLscpEngine::midiInJackPortName(int id)
{
    GidLsChannel info = ls.getSfzChannelInfo(id);
    return info.midiJackPort;
}

QStringList KonfytLscpEngine::audioOutJackPortNames(int id)
{
    GidLsChannel info = ls.getSfzChannelInfo(id);
    QStringList ret;
    ret.append(info.audioLeftJackPort);
    ret.append(info.audioRightJackPort);
    return ret;
}

void KonfytLscpEngine::removeSfz(int id)
{
    ls.removeSfzChannel(id);
}

void KonfytLscpEngine::setGain(int id, float newGain)
{
    print("TODO: setGain " + n2s(id) + " to " + n2s(newGain));
}

void KonfytLscpEngine::onLsInitialised(bool error, QString errMsg)
{
    if (error) {
        print("LSCP engine initialisation error:");
        print(errMsg);
        emit initDone(errMsg);
        return;
    }

    // Clean up:
    // Check for all Linuxsampler clients starting with basename and ending with
    // "end". If any exist without corresponding Konfyt JACK clients, they are
    // orphaned from a previous Konfyt session (crash) and can be deleted.

    QString basename = mJackEngine->clientBaseName();
    static QString end = "_LS";
    QSet<QString> clients = mJackEngine->getJackClientsList();

    QStringList orphaned;

    foreach (const QString& client, clients) {
        // Check for Konfyt LS client (starts with basename, ends with "end")
        if (client.startsWith(basename) && (client.endsWith(end))) {
            QString konfytName = client.left(client.length() - end.length());
            // Check for corresponding Konfyt client
            if (!clients.contains(konfytName)) {
                orphaned.append(client);
                print("Orphaned LS client detected: " + client);
            } else {
                print("Other LS instance pair detected: " + konfytName + ", " + client);
            }
        }
    }

    foreach (const QString& client, orphaned) {
        ls.removeAllRelatedToClient(client);
    }

    // Device setup
    ls.setupDevices(mJackEngine->clientName() + end);

    emit initDone("");
}
