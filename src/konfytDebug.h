/******************************************************************************
 *
 * Copyright 2023 Gideon van der Kolf
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

#ifndef KONFYTDEBUG_H
#define KONFYTDEBUG_H

#include <QDebug>
#include <QElapsedTimer>

#define KONFYT_DEBUG_TIMER_START(name) QElapsedTimer name; name.start();
#define KONFYT_DEBUG_TIMER_STOP(name) qDebug() << "Debug timer " << #name << ": " << name.elapsed() << " ms";

#endif // KONFYTDEBUG_H
