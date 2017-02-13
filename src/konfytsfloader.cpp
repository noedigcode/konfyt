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

#include "konfytsfloader.h"

konfytSfLoaderRunner::konfytSfLoaderRunner()
{
}

void konfytSfLoaderRunner::run()
{
    int sfid = fluid_synth_sfload(synth, filename.toLocal8Bit().data(), 1);
    if (sfid == -1) {
        userMessage("THREAD: error loading soundfont");
    }
    fluid_sfont_t* sf = fluid_synth_get_sfont_by_id(synth,sfid);

    // Iterate through all the presets within the soundfont
    int more = 1;
    fluid_preset_t* preset = new fluid_preset_t();
    // Reset the iteration
    sf->iteration_start(sf);
    while (more) {
        more = sf->iteration_next(sf, preset);
        if (more) {
            // Get preset name
            char* presetname = preset->get_name(preset);
            QString qpresetname = QString::fromAscii(presetname);
            int banknum = preset->get_banknum(preset);
            int num = preset->get_num(preset);
            userMessage("Preset " + qpresetname + " " + QString::number(banknum)
                        + ", " + QString::number(num));
        }
    }

    // Unload soundfont to save memory
    fluid_synth_sfunload(synth,sfid,1);
    delete preset;

    emit finished();
}


konfytSfLoader::konfytSfLoader(QObject *parent)
{
}

void konfytSfLoader::loadSoundfont(fluid_synth_t *synth, QString filename)
{
    static bool firstRun = true;
    if (firstRun) {
        runner = new konfytSfLoaderRunner();
        connect(runner, SIGNAL(finished()), this, SLOT(on_runner_finished()));
        connect(runner, SIGNAL(userMessage(QString)), this, SLOT(on_runner_userMessage(QString)));
        connect(this, SIGNAL(operate()), runner, SLOT(run()));
        runner->moveToThread(&workerThread);
        workerThread.start();
        firstRun = false;
        userMessage("In FirstRun");
    }

    runner->synth = synth;
    runner->filename = filename;
    emit operate();
}

void konfytSfLoader::on_runner_finished()
{
    emit finished();
}

void konfytSfLoader::on_runner_userMessage(QString msg)
{
    emit userMessage(msg);
}
