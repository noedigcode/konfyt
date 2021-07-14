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

#include "konfytDefines.h"

#ifdef KONFYT_USE_CARLA
    #include <carla/CarlaHost.h>
#endif
#include <fluidsynth.h>


int wrapIndex(int index, int listLength)
{
    if (index < 0) { index = listLength - 1; }
    if (index >= listLength) { index = 0; }
    return index;
}

QString sanitiseFilename(QString filename)
{
    static QString invalid = "<>:\"/\\|?*";

    QString ret = filename;
    ret.remove(QChar('\0'));
    for (int i=0; i < invalid.length(); i++) {
        ret.remove(invalid.at(i));
    }
    ret = ret.simplified();

    return ret;
}

QString getCompileVersionText()
{
    QString txt;
    txt.append( "Compiled with Qt " + QString(QT_VERSION_STR));
    txt.append("\n");
    txt.append( "Compiled with Fluidsynth " + QString(fluid_version_str()));
    txt.append("\n");
#ifdef KONFYT_USE_CARLA
    txt.append( "Compiled with Carla " + QString(CARLA_VERSION_STRING));
#else
    txt.append( "Compiled without Carla support" );
#endif
    return txt;
}

void konfytAssertMsg(const char *file, int line, const char *func, const char *text)
{
    std::cout << "KONFYT ASSERT: FILE " << file << ", LINE " << line
              << ", FUNCTION " << func << ", " << text << std::endl << std::flush;
}
