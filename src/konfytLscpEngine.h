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

#ifndef KONFYTLSCPENGINE_H
#define KONFYTLSCPENGINE_H

#include "gidls.h"
#include "konfytBaseSoundEngine.h"

#include <QObject>

class KonfytLscpEngine : public KonfytBaseSoundEngine
{
public:
    KonfytLscpEngine(QObject *parent = 0);
    ~KonfytLscpEngine();

    QString engineName() override;
    void initEngine(KonfytJackEngine *mJackEngine) override;
    QString jackClientName() override;
    int addSfz(QString path) override;
    QString pluginName(int id) override;
    QString midiInJackPortName(int id) override;
    QStringList audioOutJackPortNames(int id) override;
    void removeSfz(int id) override;
    void setGain(int id, float newGain) override;

private:
    GidLs ls;
    KonfytJackEngine* mJackEngine = nullptr;

private slots:
    void onLsInitialised(bool error, QString errMsg);

};

#endif // KONFYTLSCPENGINE_H
