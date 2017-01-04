#ifndef KONFYTSFLOADER_H
#define KONFYTSFLOADER_H

#include <QThread>
#include <fluidsynth.h>

class konfytSfLoaderRunner : public QObject
{
    Q_OBJECT
public:
    explicit konfytSfLoaderRunner();

    fluid_synth_t* synth;
    QString filename;

public slots:
    void run();

signals:
    void finished();
    void userMessage(QString msg);

public slots:


};

class konfytSfLoader : public QObject
{
    Q_OBJECT
public:
    explicit konfytSfLoader(QObject *parent = 0);

    void loadSoundfont(fluid_synth_t* synth, QString filename);

private:
    QThread workerThread;
    konfytSfLoaderRunner *runner;

private slots:
    void on_runner_finished();
    void on_runner_userMessage(QString msg);

signals:
    void finished();
    void userMessage(QString msg);
    void operate();

public slots:

};

#endif // KONFYTSFLOADER_H
