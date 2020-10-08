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

#ifndef KONFYTBRIDGEENGINE_H
#define KONFYTBRIDGEENGINE_H

#include "konfytBaseSoundEngine.h"

#include <QObject>
#include <QProcess>


class KonfytBridgeEngine : public KonfytBaseSoundEngine
{
    Q_OBJECT
public:

    struct KonfytBridgeItem
    {
        QString state;
        int startCount;

        QProcess* process;
        QString soundfilePath;
        QString jackname;
        QString cmd;

        KonfytBridgeItem() : startCount(0) {}
    };

    explicit KonfytBridgeEngine(QObject *parent = 0);
    ~KonfytBridgeEngine();

    QString engineName();
    void setKonfytExePath(QString path);
    void initEngine(KonfytJackEngine* jackEngine);
    int addSfz(QString soundfilePath);
    QString pluginName(int id);
    QString midiInJackPortName(int id);
    QStringList audioOutJackPortNames(int id);
    void removeSfz(int id);
    void setGain(int id, float newGain);
    QString getAllStatusInfo();

private:
    KonfytJackEngine* jack = nullptr;
    QString exePath;
    int idCounter = 300;
    QMap<int, KonfytBridgeItem> items;

    void startProcess(int id);
    QString getStatusInfo(int id);
    void sendAllStatusInfo();
};

#endif // KONFYTBRIDGEENGINE_H


