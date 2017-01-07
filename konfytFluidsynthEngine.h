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

#ifndef KONFYT_FLUIDSYNTH_ENGINE_H
#define KONFYT_FLUIDSYNTH_ENGINE_H

#include <QObject>
#include <fluidsynth.h>
#include "konfytDefines.h"
#include "konfytDatabase.h"
#include <QMutex>


class konfytFluidsynthEngine : public QObject
{
    Q_OBJECT
public:

    struct konfytFluidSynthData
    {
        int ID; // ID in konfytFluidsynthEngine
        fluid_synth_t* synth;
        fluid_settings_t* settings;
        konfytSoundfontProgram program;
        int soundfontIDinSynth;
    };

    explicit konfytFluidsynthEngine(QObject *parent = 0);

    void InitFluidsynth(double SampleRate);

    QMutex mutex;
    void processJackMidi(int ID, const konfytMidiEvent* ev);
    int fluidsynthWriteFloat(int ID, void* leftBuffer, void* rightBuffer, int len);

    int addSoundfontProgram(konfytSoundfontProgram p);
    void removeSoundfontProgram(int ID);

    float getGain(int ID);
    void setGain(int ID, float newGain);

    void error_abort(QString msg);

private:
    int synthUniqueIDCounter;
    QMap<int, konfytFluidSynthData> synthDataMap;
    double samplerate;
    
signals:
    void userMessage(QString msg);
    
public slots:
    
};

#endif // KONFYT_FLUIDSYNTH_ENGINE_H
