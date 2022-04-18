/******************************************************************************
 *
 * Copyright 2021 Gideon van der Kolf
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

#include "indicatorHandlers.h"


void LayerIndicatorHandler::layerWidgetAdded(KonfytLayerWidget *w, KfJackMidiRoute *route)
{
    MidiState& state = midiRouteStateMap[route]; // Created if it does not exist

    state.widget = w;
    w->indicateSustain(state.sustain);
    w->indicatePitchbend(state.pitchbend);
    midiWidgetRouteMap.insert(w, route);
}

void LayerIndicatorHandler::layerWidgetAdded(KonfytLayerWidget *w,
                                             KfJackAudioRoute *route1,
                                             KfJackAudioRoute *route2)
{
    audioRouteMap.insert(route1, { .left = true, .widget = w });
    audioRouteMap.insert(route2, { .left = false, .widget = w });
}

void LayerIndicatorHandler::jackEventReceived(KfJackMidiRxEvent ev)
{
    if (!midiRouteStateMap.contains(ev.midiRoute)) { return; }

    MidiState& state = midiRouteStateMap[ev.midiRoute];

    if ((ev.midiEvent.type() == MIDI_EVENT_TYPE_CC)
        && (ev.midiEvent.data1() == 64)) {
        state.sustain = (ev.midiEvent.data2() > MIDI_SUSTAIN_THRESH);
    }
    if (ev.midiEvent.type() == MIDI_EVENT_TYPE_PITCHBEND) {
        state.pitchbend = (ev.midiEvent.pitchbendValueSigned() != 0);
    }

    if (state.widget) {
        state.widget->indicateMidi();
        state.widget->indicateSustain(state.sustain);
        state.widget->indicatePitchbend(state.pitchbend);
    }
}

void LayerIndicatorHandler::jackEventReceived(KfJackAudioRxEvent ev)
{
    if (!audioRouteMap.contains(ev.audioRoute)) { return; }

    AudioInfo& a = audioRouteMap[ev.audioRoute];

    if (a.widget) {
        if (a.left) {
            a.widget->indicateAudioLeft(ev.data);
        } else {
            a.widget->indicateAudioRight(ev.data);
        }
    }
}

void LayerIndicatorHandler::layerWidgetRemoved(KonfytLayerWidget *w)
{
    KfJackMidiRoute* route = midiWidgetRouteMap.take(w);

    if (route) {
        MidiState& state = midiRouteStateMap[route];
        if (state.widget == w) {
            state.widget = nullptr;
        }
    }

    foreach (KfJackAudioRoute* route, audioRouteMap.keys()) {
        AudioInfo& a = audioRouteMap[route];
        if (a.widget == w) {
            a.widget = nullptr;
        }
    }
}

void PortIndicatorHandler::jackEventReceived(KfJackMidiRxEvent ev)
{
    if (ev.sourcePort == nullptr) { return; }

    PortState& state = portStateMap[ev.sourcePort]; // Created if it does not exist

    if ((ev.midiEvent.type() == MIDI_EVENT_TYPE_CC)
        && (ev.midiEvent.data1() == 64)) {
        setPortSustain(state, ev.midiEvent.data2() > MIDI_SUSTAIN_THRESH);
    }
    if ((ev.midiEvent.type() == MIDI_EVENT_TYPE_PITCHBEND)) {
        setPortPitchbend(state, ev.midiEvent.pitchbendValueSigned() != 0);
    }
}

bool PortIndicatorHandler::isSustainDown()
{
    return (totalSustainCount != 0);
}

bool PortIndicatorHandler::isPitchbendNonzero()
{
    return (totalPitchbendCount != 0);
}

void PortIndicatorHandler::portRemoved(KfJackMidiPort *port)
{
    if (!portStateMap.contains(port)) { return; }

    PortState& state = portStateMap[port];

    setPortSustain(state, false);
    setPortPitchbend(state, false);

    portStateMap.remove(port);
}

void PortIndicatorHandler::clearSustain()
{
    foreach (KfJackMidiPort* port, portStateMap.keys()) {
        portStateMap[port].sustain = false;
    }
    totalSustainCount = 0;
}

void PortIndicatorHandler::clearPitchbend()
{
    foreach (KfJackMidiPort* port, portStateMap.keys()) {
        portStateMap[port].pitchbend = false;
    }
    totalPitchbendCount = 0;
}

void PortIndicatorHandler::setPortSustain(PortIndicatorHandler::PortState &state, bool sustain)
{
    if (state.sustain != sustain) {
        state.sustain = sustain;
        totalSustainCount += (sustain ? 1 : -1);
        if (totalSustainCount < 0) { totalSustainCount = 0; }
    }
}

void PortIndicatorHandler::setPortPitchbend(PortIndicatorHandler::PortState &state, bool pitchbend)
{
    if (state.pitchbend != pitchbend) {
        state.pitchbend = pitchbend;
        totalPitchbendCount += (pitchbend ? 1 : -1);
        if (totalPitchbendCount < 0) { totalPitchbendCount = 0; }
    }
}
