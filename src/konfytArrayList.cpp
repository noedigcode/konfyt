#ifndef KONFYT_ARLIST_CPP
#define KONFYT_ARLIST_CPP

#include "konfytArrayList.h"


template <typename T>
konfytArrayList<T>::konfytArrayList()
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
void konfytArrayList<T>::add(T item)
{
    int i = getFreeIndex();
    array[i].data = item;
}

template <typename T>
int konfytArrayList<T>::count()
{
    return arrayCount;
}

template <typename T>
T konfytArrayList<T>::at(int index)
{
    if ( (index<0) || (index>=arrayCount)) { abort(); }

    return array[ ptrArray[index] ].data;
}

template <typename T>
T* konfytArrayList<T>::at_ptr(int index)
{
    if ( (index<0) || (index>=arrayCount)) { abort(); }

    return &( array[ ptrArray[index] ].data );
}

template <typename T>
void konfytArrayList<T>::remove(int index)
{
    if ( (index<0) || (index>=arrayCount)) { abort(); }

    releaseIndex( ptrArray[index] );
    if (index < arrayCount) {
        // Move last item in ptrArray to current index to prevent gaps
        ptrArray[index] = ptrArray[arrayCount];
    }
}

template <typename T>
int konfytArrayList<T>::getFreeIndex()
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
void konfytArrayList<T>::releaseIndex(int i)
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
