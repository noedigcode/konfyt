#ifndef RINGBUFFERQMUTEX_H
#define RINGBUFFERQMUTEX_H

#include <stdlib.h>

#include <QList>
#include <QMutex>

template <class T>
class RingbufferQMutex
{
public:
    RingbufferQMutex (int bufferSize)
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

    /* Reads all the data and returns a QList. This conveniently combines
     * startRead(), hasNext(), readNext() and endRead() and should be called
     * stand-alone. Note however that a QList is constructed. */
    QList<T> readAll()
    {
        startRead();

        QList<T> ret;
        while (hasNext()) {
            ret.append(readNext());
        }

        endRead();

        return ret;
    }

    /* To be used in combination with hasNext(), readNext() and endRead() as an
     * alternative to readAll() in order to read elements but not allocate a
     * QList. */
    void startRead()
    {
        lockedData.mutex.lock();
        ireadEnd = lockedData.readEnd;
        lockedData.mutex.unlock();
    }

    bool hasNext()
    {
        return (iread != ireadEnd);
    }

    /* Only call this if hasNext() returns true. */
    T readNext()
    {
        T ret = buffer[iread];
        iread = incr(iread);
        return ret;
    }

    void endRead()
    {
        lockedData.mutex.lock();
        lockedData.writeEnd = iread;
        lockedData.mutex.unlock();
    }


private:
    int size;
    T* buffer;

    int iread = 0;
    int ireadEnd = 0;
    int iwrite = 0;
    int iwriteEnd = 0;
    bool stashed = false;

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
