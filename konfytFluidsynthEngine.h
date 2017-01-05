#ifndef KONFYT_FLUIDSYNTH_ENGINE_H
#define KONFYT_FLUIDSYNTH_ENGINE_H

#include <QObject>
#include <fluidsynth.h>
#include "konfytDefines.h"
#include "konfytDatabase.h"
#include <QMutex>


class konfytFluidsynthEngine : public QObject
{
    Q_OBJECT
public:

    struct konfytFluidSynthData
    {
        int ID; // ID in konfytFluidsynthEngine
        fluid_synth_t* synth;
        fluid_settings_t* settings;
        konfytSoundfontProgram program;
        int soundfontIDinSynth;
    };

    explicit konfytFluidsynthEngine(QObject *parent = 0);

    void InitFluidsynth(double SampleRate);

    QMutex mutex;
    void processJackMidi(int ID, const konfytMidiEvent* ev);
    int fluidsynthWriteFloat(int ID, void* leftBuffer, void* rightBuffer, int len);

    int addSoundfontProgram(konfytSoundfontProgram p);
    void removeSoundfontProgram(int ID);

    float getGain(int ID);
    void setGain(int ID, float newGain);

    void error_abort(QString msg);

private:
    int synthUniqueIDCounter;
    QMap<int, konfytFluidSynthData> synthDataMap;
    double samplerate;
    
signals:
    void userMessage(QString msg);
    
public slots:
    
};

#endif // KONFYT_FLUIDSYNTH_ENGINE_H
