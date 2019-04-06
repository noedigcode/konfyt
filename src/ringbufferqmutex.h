#ifndef RINGBUFFERQMUTEX_H
#define RINGBUFFERQMUTEX_H

#include <stdlib.h>
#include <QList>
#include <QMutex>

template <class T>
class RingbufferQMutex
{
public:
    RingbufferQMutex (int bufferSize) :
        iread(0),
        iwrite(0),
        iwriteEnd(0),
        stashed(false)
    {
        size = bufferSize;
        buffer = (T*)malloc(sizeof(T)*size);

        lockedData.readEnd = 0;
        lockedData.writeEnd = 0;
    }

    ~RingbufferQMutex ()
    {
        free(buffer);
    }

    bool stash(T val)
    {
        int nextIwrite = incr(iwrite);
        if (nextIwrite != iwriteEnd) {
            buffer[iwrite] = val;
            iwrite = nextIwrite;
            stashed = true;
            return true;
        }
        return false;
    }

    bool commit()
    {
        lockedData.mutex.lock();

        lockedData.readEnd = iwrite;
        iwriteEnd = lockedData.writeEnd;

        lockedData.mutex.unlock();

        bool ret = stashed;
        stashed = false;
        return ret;
    }

    QList<T> readAll()
    {
        lockedData.mutex.lock();
        int ireadEnd = lockedData.readEnd;
        lockedData.mutex.unlock();

        QList<T> ret;
        while (iread != ireadEnd) {
            ret.append(buffer[iread]);
            iread = incr(iread);
        }

        lockedData.mutex.lock();
        lockedData.writeEnd = iread;
        lockedData.mutex.unlock();

        return ret;
    }


private:
    int size;
    T* buffer;

    int iread;
    int iwrite;
    int iwriteEnd;
    bool stashed;

    struct LockedData {
        QMutex mutex;
        int readEnd;
        int writeEnd;
    } lockedData;

    int incr(int val)
    {
        val++;
        if (val >= size) { val = 0; }
        return val;
    }

};

#endif // RINGBUFFERQMUTEX_H
