#ifndef SCANFOLDERSTHREAD_H
#define SCANFOLDERSTHREAD_H

#include <QThread>
#include <fluidsynth.h>


class scanFoldersThread : public QThread
{
    Q_OBJECT
public:
    explicit scanFoldersThread(QObject *parent = 0);

    fluid_synth_t* synth;
    QString filename;
    void run();
    
signals:
    void finished();
    void userMessage(QString msg);
    
public slots:
    
};

#endif // SCANFOLDERSTHREAD_H
