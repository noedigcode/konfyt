/******************************************************************************
 *
 * Copyright 2018 Gideon van der Kolf
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

#include "konfytBridgeEngine.h"

KonfytBridgeEngine::KonfytBridgeEngine(QObject *parent) :
    KonfytBaseSoundEngine(parent),
    jack(NULL),
    idCounter(300)
{

}

KonfytBridgeEngine::~KonfytBridgeEngine()
{
    // Stop all processes
    QList<int> ids = items.keys();
    for (int i=0; i < ids.count(); i++) {
        int id = ids[i];
        KonfytBridgeItem &item = items[id];
        item.process->blockSignals(true);
        item.process->kill();
        delete item.process;
    }
}

void KonfytBridgeEngine::setKonfytExePath(QString path)
{
    exePath = path;
}

void KonfytBridgeEngine::initEngine(KonfytJackEngine *jackEngine)
{
    jack = jackEngine;
    userMessage("KonfytBridgeEngine initialised.");
}

int KonfytBridgeEngine::addSfz(QString soundfilePath)
{
    KonfytBridgeItem item;

    item.soundfilePath = soundfilePath;

    int id = idCounter++;
    item.jackname = jack->clientName() + "_subclient_" + n2s(id);

    item.cmd = exePath + " -q -c -j " + item.jackname + " \"" + soundfilePath + "\"";

    item.process = new QProcess();
    connect(item.process, &QProcess::started, [this, id](){
        KonfytBridgeItem &item = items[id];
        item.state = "Started";
        sendAllStatusInfo();
        userMessage("Bridge client " + n2s(id) + " started.");
    });
    connect(item.process, static_cast<void (QProcess::*)(int)>(&QProcess::finished),
            [this, id](int returnCode){
        // Process has finished. Restart.
        userMessage("Bridge client " + n2s(id) + " stopped: " + n2s(returnCode) + ". Restarting...");
        startProcess(id);
    });

    items.insert(id, item);
    startProcess(id);

    return id;
}

QString KonfytBridgeEngine::pluginName(int id)
{
    return "bridge_" + n2s(id);
}

QString KonfytBridgeEngine::midiInJackPortName(int id)
{
    if (!items.contains(id)) {
        // TODO BRIDGE error_abort()
        return "Invalid id " + n2s(id);
    }
    KonfytBridgeItem &item = items[id];
    return item.jackname + ":" + "midi_in_0"; // TODO BRIDGE Detect ports with less fragility
}

QStringList KonfytBridgeEngine::audioOutJackPortNames(int id)
{
    QStringList ret;

    if (!items.contains(id)) {
        // TODO BRIDGE error_abort()
        return ret;
    }
    KonfytBridgeItem &item = items[id];
    ret.append( item.jackname + ":" + "bus_0_L" ); // TODO BRIDGE Detect ports with less fragility
    ret.append( item.jackname + ":" + "bus_0_R" ); // TODO BRIDGE Detect ports with less fragility

    return ret;
}

void KonfytBridgeEngine::removeSfz(int id)
{
    // TODO BRIDGE
    userMessage("TODO BRIDGE: removeSfz " + n2s(id));
}

void KonfytBridgeEngine::setGain(int id, float newGain)
{
    // TODO BRIDGE
    userMessage("TODO BRIDGE: setGain of " + n2s(id) + " to " + n2s(newGain));
}

QString KonfytBridgeEngine::getAllStatusInfo()
{
    int crashes = 0;
    QList<int> ids = items.keys();
    for (int i=0; i < ids.count(); i++) {
        KonfytBridgeItem &item = items[ids[i]];
        if (item.startCount > 1) {
            crashes += item.startCount-1;
        }
    }

    QString ret;
    ret += "Subclients: " + n2s(ids.count()) + "\n";
    ret += "Crashes: " + n2s(crashes) + "\n";
    return ret;
}

void KonfytBridgeEngine::startProcess(int id)
{
    KonfytBridgeItem &item = items[id];

    userMessage("Bridge client " + n2s(id) + " starting:");
    userMessage(   item.cmd);

    item.startCount++;
    item.state = "Starting...";
    sendAllStatusInfo();

    item.process->start(item.cmd);
}

QString KonfytBridgeEngine::getStatusInfo(int id)
{
    if (!items.contains(id)) { return "Invalid id " + n2s(id); }
    KonfytBridgeItem &item = items[id];

    QString ret;
    ret += "Subclient " + n2s(id) + "\n";
    ret += "   State: " + item.state + "\n";
    ret += "   StartCount: " + n2s(item.startCount) + "\n";
    ret += "   Soundfile: " + item.soundfilePath + "\n";

    return ret;
}

void KonfytBridgeEngine::sendAllStatusInfo()
{
    emit statusInfo( getAllStatusInfo() );
}
