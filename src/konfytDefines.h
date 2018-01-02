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

#ifndef KONFYT_DEFINES_H
#define KONFYT_DEFINES_H

#include <QString>

#define APP_NAME "Konfyt"
#define APP_VERSION "1.0.0-testing"

#define n2s(x) QString::number(x)
#define STRING_PROJECT_DIR "$PROJ_DIR$"

#define KONFYT_PATCH_SUFFIX "konfytpatch"

QString sanitiseFilename(QString path);


#endif // KONFYT_DEFINES_H
