/******************************************************************************
 *
 * Copyright 2020 Gideon van der Kolf
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
    connect(&ls, &GidLs::print, this, &KonfytLscpEngine::userMessage);
    connect(&ls, &GidLs::initialised, [this](bool error, QString errMsg){
        if (error) {
            userMessage("LSCP engine initialisation error:");
            userMessage(errMsg);
        } else {
            userMessage("LSCP engine initialised!");
        }
    });
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
    ls.init( jackEngine->clientName() + "_LS" );
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
    userMessage("TODO: setGain " + n2s(id) + " to " + n2s(newGain));
}
