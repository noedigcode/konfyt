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

#ifndef KONFYTBASESOUNDENGINE_H
#define KONFYTBASESOUNDENGINE_H

#include <QObject>

#include "konfytJackEngine.h"

class KonfytBaseSoundEngine : public QObject
{
    Q_OBJECT
public:
    explicit KonfytBaseSoundEngine(QObject *parent = 0);
    virtual ~KonfytBaseSoundEngine();

    virtual QString engineName() = 0;
    virtual void initEngine(KonfytJackEngine* jackEngine) = 0;
    virtual QString jackClientName() { return ""; }
    virtual int addSfz(QString path) = 0;
    virtual QString pluginName(int id) = 0;
    virtual QString midiInJackPortName(int id) = 0;
    virtual QStringList audioOutJackPortNames(int id) = 0;
    virtual void removeSfz(int id) = 0;
    virtual void setGain(int id, float newGain) = 0;

signals:
    void userMessage(QString msg);
    void statusInfo(QString msg);

public slots:
};

#endif // KONFYTBASESOUNDENGINE_H
