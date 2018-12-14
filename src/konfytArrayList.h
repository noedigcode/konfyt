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

#ifndef KONFYT_JACK_ARRAYLIST_H
#define KONFYT_JACK_ARRAYLIST_H

#define KONFYT_ARLIST_SIZE 1000

template <typename T>
class KonfytArrayList
{
public:

    KonfytArrayList();
    bool add(T item);
    int count();
    T at(int index);
    T* at_ptr(int index);
    void remove(int index);

private:

    struct Item
    {
        bool used;
        T data;
    };

    int ptrArray[KONFYT_ARLIST_SIZE];
    Item array[KONFYT_ARLIST_SIZE];
    int arrayEnd;
    int arrayStart;
    int arrayCount;

    int freeIndexes[KONFYT_ARLIST_SIZE];
    int freeCount;


    int getFreeIndex();
    void releaseIndex(int i);

};

#include "konfytArrayList.cpp"


#endif // KONFYT_JACK_ARRAYLIST_H

