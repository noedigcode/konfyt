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

#include "indicatorHandlers.h"


void LayerIndicatorHandler::layerWidgetAdded(KonfytLayerWidget *w, KfJackMidiRoute *route)
{
    RouteState& state = routeStateMap[route]; // Created if it does not exist
    state.widget = w;
    w->indicateSustain(state.sustain);
    w->indicatePitchbend(state.pitchbend);
    widgetRouteMap.insert(w, route);
}

void LayerIndicatorHandler::jackEventReceived(KfJackMidiRxEvent ev)
{
    if (!routeStateMap.contains(ev.midiRoute)) { return; }

    RouteState& state = routeStateMap[ev.midiRoute];

    if ((ev.midiEvent.type() == MIDI_EVENT_TYPE_CC)
        && (ev.midiEvent.data1() == 64)) {
        state.sustain = (ev.midiEvent.data2() > 0);
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

void LayerIndicatorHandler::layerWidgetRemoved(KonfytLayerWidget *w)
{
    KfJackMidiRoute* route = widgetRouteMap.take(w);
    routeStateMap[route].widget = nullptr;
}

void PortIndicatorHandler::jackEventReceived(KfJackMidiRxEvent ev)
{
    if (ev.sourcePort == nullptr) { return; }

    PortState& state = portStateMap[ev.sourcePort]; // Created if it does not exist

    if ((ev.midiEvent.type() == MIDI_EVENT_TYPE_CC)
        && (ev.midiEvent.data1() == 64)) {
        setPortSustain(state, ev.midiEvent.data2() > 0);
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
