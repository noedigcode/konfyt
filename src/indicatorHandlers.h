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

#ifndef INDICATORHANDLERS_H
#define INDICATORHANDLERS_H

#include "konfytJackStructs.h"
#include "konfytLayerWidget.h"

#include <QMap>

class LayerIndicatorHandler
{
public:
    void layerWidgetAdded(KonfytLayerWidget* w, KfJackMidiRoute* route);
    void layerWidgetAdded(KonfytLayerWidget* w, KfJackAudioRoute* route1,
                          KfJackAudioRoute* route2);
    void jackEventReceived(KfJackMidiRxEvent ev);
    void jackEventReceived(KfJackAudioRxEvent ev);
    void layerWidgetRemoved(KonfytLayerWidget* w);

private:
    struct MidiState {
        bool sustain = false;
        bool pitchbend = false;
        KonfytLayerWidget* widget = nullptr;
    };

    QMap<KfJackMidiRoute*, MidiState> midiRouteStateMap;
    QMap<KonfytLayerWidget*, KfJackMidiRoute*> midiWidgetRouteMap;

    struct AudioInfo {
        bool left = true;
        KonfytLayerWidget* widget = nullptr;
    };

    QMap<KfJackAudioRoute*, AudioInfo> audioRouteMap;

};

// -----------------------------------------------------------------------------

class PortIndicatorHandler
{
public:
    void jackEventReceived(KfJackMidiRxEvent ev);
    bool isSustainDown();
    bool isPitchbendNonzero();
    void portRemoved(KfJackMidiPort* port);
    void clearSustain();
    void clearPitchbend();

private:
    struct PortState {
        bool sustain = false;
        bool pitchbend = false;
    };
    QMap<KfJackMidiPort*, PortState> portStateMap;
    int totalSustainCount = 0;
    int totalPitchbendCount = 0;

    void setPortSustain(PortState& state, bool sustain);
    void setPortPitchbend(PortState& state, bool pitchbend);
};

#endif // INDICATORHANDLERS_H
