/******************************************************************************
 *
 * Copyright 2020 Gideon van der Kolf
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

#include "konfytFluidsynthEngine.h"

#include <iostream>

#define MIDI_CHANNEL_0 0 // Used to easily see where channel 0 is forced.

KonfytFluidsynthEngine::KonfytFluidsynthEngine(QObject *parent) :
    QObject(parent)
{
}

KonfytFluidsynthEngine::~KonfytFluidsynthEngine()
{
    while (!synths.isEmpty()) {
        removeSoundfontProgram(synths[0]);
    }
}

/* Generate Fluidsynth MIDI events based on buffer from JACK MIDI input. */
void KonfytFluidsynthEngine::processJackMidi(KfFluidSynth *synth, const KonfytMidiEvent *ev)
{
    // TODO NB: handle case where we are in panic mode, and all-note-off etc. messages are
    // received, but the mutex is already locked. We have to queue it somehow for until the
    // mutex is unlocked, othewise the events are thrown away and panic mode doesn't do what
    // it's supposed to.

    // If we don't get the mutex immediately, don't block and wait for it.
    if ( !mutex.tryLock() ) {
        return;
    }

    if ( (ev->type() != MIDI_EVENT_TYPE_PROGRAM) || (ev->type() != MIDI_EVENT_TYPE_SYSTEM) ) {

        // All MIDI events are sent to Fluidsynth on channel 0

        if (ev->type() == MIDI_EVENT_TYPE_NOTEON) {
            fluid_synth_noteon( synth->synth, MIDI_CHANNEL_0, ev->note(), ev->velocity() );
        } else if (ev->type() == MIDI_EVENT_TYPE_NOTEOFF) {
            fluid_synth_noteoff( synth->synth, MIDI_CHANNEL_0, ev->note() );
        } else if (ev->type() == MIDI_EVENT_TYPE_CC) {
            fluid_synth_cc( synth->synth, MIDI_CHANNEL_0, ev->data1(), ev->data2() );
            // If we have received an all notes off, sommer kill all the sound also. This is probably a panic.
            if (ev->data1() == MIDI_CC_ALL_NOTES_OFF) {
                fluid_synth_all_sounds_off( synth->synth, MIDI_CHANNEL_0 );
            }
        } else if (ev->type() == MIDI_EVENT_TYPE_PITCHBEND) {
            // Fluidsynth expects a positive pitchbend value, i.e. centered around 8192, not zero.
            fluid_synth_pitch_bend( synth->synth, MIDI_CHANNEL_0, ev->pitchbendValueSigned()+8192 );
        }
    }

    mutex.unlock();
}

/* Calls fluid_synth_write_float for specified synth, fills specified buffers
 * and returns Fluidsynth's return value. */
int KonfytFluidsynthEngine::fluidsynthWriteFloat(KfFluidSynth *synth, void *leftBuffer, void *rightBuffer, int len)
{
    // If we don't get the mutex immediately, don't block and wait for it.
    if ( !mutex.tryLock() ) {
        return 0;
    }

    int ret =  fluid_synth_write_float( synth->synth, len,
                                    leftBuffer, 0, 1,
                                    rightBuffer, 0, 1);

    mutex.unlock();
    return ret;
}

/* Adds a new soundfont engine and returns a pointer to the synth. Returns nullptr on error. */
KfFluidSynth* KonfytFluidsynthEngine::addSoundfontProgram(KonfytSoundfontProgram p)
{
    KfFluidSynth* s = new KfFluidSynth();

    // Create settings object
    s->settings = new_fluid_settings();
    if (s->settings == NULL) {
        emit print("Failed to create Fluidsynth settings.");
        delete s;
        return nullptr;
    }

    // Set settings
    fluid_settings_setnum(s->settings, "synth.sample-rate", mSampleRate);

    // Create the synthesizer
    s->synth = new_fluid_synth(s->settings);
    if (s->synth == NULL) {
        emit print("Failed to create Fluidsynth synthesizer.");
        delete s;
        return nullptr;
    }

    // Load soundfont file
    int sfID = fluid_synth_sfload(s->synth, p.parent_soundfont.toLocal8Bit().data(), 0);
    if (sfID == -1) {
        emit print("Failed to load soundfont " + p.parent_soundfont);
        delete s;
        return nullptr;
    }

    // Set the program
    fluid_synth_program_select(s->synth, 0, sfID, p.bank, p.program);

    s->program = p;
    s->soundfontIDinSynth = sfID;

    synths.append(s);

    return s;
}

void KonfytFluidsynthEngine::removeSoundfontProgram(KfFluidSynth *synth)
{
    mutex.lock();

    synths.removeAll(synth);
    delete synth;

    mutex.unlock();
}

void KonfytFluidsynthEngine::initFluidsynth(double sampleRate)
{
    print("Fluidsynth version " + QString(fluid_version_str()));
    mSampleRate = sampleRate;
    print("Fluidsynth sample rate: " + n2s(mSampleRate));
}

float KonfytFluidsynthEngine::getGain(KfFluidSynth *synth)
{
    return fluid_synth_get_gain( synth->synth );
}

void KonfytFluidsynthEngine::setGain(KfFluidSynth *synth, float newGain)
{
    fluid_synth_set_gain( synth->synth, newGain );
}

/* Print error message to stdout, and abort app. */
void KonfytFluidsynthEngine::error_abort(QString msg)
{
    std::cout << "\n" << "Konfyt ERROR, ABORTING: sfengine:" << msg.toLocal8Bit().constData();
    abort();
}
