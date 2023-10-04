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
