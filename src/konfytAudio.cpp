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

#include "konfytAudio.h"

#include <math.h>


/* Converts gain between 0 and 1 from linear to an exponential function that is
 * better suited for human hearing. Input is clipped between 0 and 1. */
float konfytConvertGain(float linearGain)
{
    if (linearGain < 0) { linearGain = 0; }
    if (linearGain > 1) { linearGain = 1; }

    return pow(linearGain, 3.0); // x^3
}
