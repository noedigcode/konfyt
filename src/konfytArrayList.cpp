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

#ifndef KONFYT_ARLIST_CPP
#define KONFYT_ARLIST_CPP

#include "konfytArrayList.h"


template <typename T>
KonfytArrayList<T>::KonfytArrayList()
{
    arrayEnd = 0;
    arrayStart = 0;
    arrayCount = 0;
    freeCount = 0;
    for (int i=0; i<KONFYT_ARLIST_SIZE; i++) {
        array[i].used = false;
    }
}

template <typename T>
bool KonfytArrayList<T>::add(T item)
{
    if (arrayCount >= KONFYT_ARLIST_SIZE) {
        return false;
    } else {
        int i = getFreeIndex();
        array[i].data = item;
        return true;
    }
}

template <typename T>
int KonfytArrayList<T>::count()
{
    return arrayCount;
}

template <typename T>
T KonfytArrayList<T>::at(int index)
{
    if ( (index<0) || (index>=arrayCount)) { abort(); }

    return array[ ptrArray[index] ].data;
}

template <typename T>
T* KonfytArrayList<T>::at_ptr(int index)
{
    if ( (index<0) || (index>=arrayCount)) { abort(); }

    return &( array[ ptrArray[index] ].data );
}

template <typename T>
void KonfytArrayList<T>::remove(int index)
{
    if ( (index<0) || (index>=arrayCount)) { abort(); }

    releaseIndex( ptrArray[index] );
    if (index < arrayCount) {
        // Move last item in ptrArray to current index to prevent gaps
        ptrArray[index] = ptrArray[arrayCount];
    }
}

template <typename T>
int KonfytArrayList<T>::getFreeIndex()
{
    int i;
    if (freeCount>0) {
        freeCount--;
        i = freeIndexes[freeCount];
    } else {
        if (arrayStart>0) {
            arrayStart--;
            i = arrayStart;
        } else {
            i = arrayEnd;
            arrayEnd++;
        }
    }
    ptrArray[arrayCount] = i;
    arrayCount++;
    array[i].used = true;
    return i;
}

template <typename T>
void KonfytArrayList<T>::releaseIndex(int i)
{
    if (i == (arrayEnd-1)) {
        arrayEnd--;
    } else {
        if (i==arrayStart) {
            arrayStart++;
        } else {
            freeIndexes[freeCount] = i;
            freeCount++;
        }
    }
    arrayCount--;
    array[i].used = false;
}

#endif // KONFYT_ARLIST_CPP
