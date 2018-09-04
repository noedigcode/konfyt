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

#ifndef KONFYTLSCPENGINE_H
#define KONFYTLSCPENGINE_H

#include <QObject>

#include "konfytBaseSoundEngine.h"

#include "gidls.h"

class KonfytLscpEngine : public KonfytBaseSoundEngine
{
public:
    KonfytLscpEngine(QObject *parent = 0);
    ~KonfytLscpEngine();

    void initEngine(KonfytJackEngine *jackEngine);
    QString jackClientName();
    int addSfz(QString path);
    QString pluginName(int id);
    QString midiInJackPortName(int id);
    QStringList audioOutJackPortNames(int id);
    void removeSfz(int id);
    void setGain(int id, float newGain);

private:
    GidLs ls;

};

#endif // KONFYTLSCPENGINE_H
