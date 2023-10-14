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

#ifndef SLEEPYRINGBUFFER_H
#define SLEEPYRINGBUFFER_H

#include <QList>
#include <QSemaphore>

template <typename T>
class SleepyRingBuffer
{
    QList<T> buffer;
    int mSize;
    int iw = 0;
    int ir = 0;
    QSemaphore freeSlots;
    QSemaphore usedSlots;

public:
    SleepyRingBuffer(int size)
    {
        mSize = size;
        buffer.reserve(size);
        freeSlots.release(size); // Make all available
        // Used slots n = 0, so none are available
    }

    bool tryWrite(T& data)
    {
        if (freeSlots.tryAcquire()) {
            if (buffer.count() <= iw) {
                buffer.append(data);
            } else {
                buffer[iw] = data;
            }
            iw = (iw + 1) % mSize;
            usedSlots.release();
            return true;
        } else {
            return false;
        }
    }

    T read()
    {
        usedSlots.acquire(); // Blocks (sleeps) until available
        T data = buffer[ir];
        ir = (ir + 1) % mSize;
        freeSlots.release();
        return data;
    }

    int availableToRead()
    {
        return usedSlots.available();
    }
};

#endif // SLEEPYRINGBUFFER_H
