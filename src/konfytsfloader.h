/******************************************************************************
 *
 * Copyright 2017 Gideon van der Kolf
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

#ifndef KONFYTSFLOADER_H
#define KONFYTSFLOADER_H

#include <QThread>
#include <fluidsynth.h>

class konfytSfLoaderRunner : public QObject
{
    Q_OBJECT
public:
    explicit konfytSfLoaderRunner();

    fluid_synth_t* synth;
    QString filename;

public slots:
    void run();

signals:
    void finished();
    void userMessage(QString msg);

public slots:


};

class konfytSfLoader : public QObject
{
    Q_OBJECT
public:
    explicit konfytSfLoader(QObject *parent = 0);

    void loadSoundfont(fluid_synth_t* synth, QString filename);

private:
    QThread workerThread;
    konfytSfLoaderRunner *runner;

private slots:
    void on_runner_finished();
    void on_runner_userMessage(QString msg);

signals:
    void finished();
    void userMessage(QString msg);
    void operate();

public slots:

};

#endif // KONFYTSFLOADER_H
