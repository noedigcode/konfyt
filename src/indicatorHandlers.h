#ifndef INDICATORHANDLERS_H
#define INDICATORHANDLERS_H

#include "konfytJackStructs.h"
#include "konfytLayerWidget.h"

#include <QMap>

class LayerIndicatorHandler
{
public:
    void layerWidgetAdded(KonfytLayerWidget* w, KfJackMidiRoute* route);
    void jackEventReceived(KfJackMidiRxEvent ev);
    void layerWidgetRemoved(KonfytLayerWidget* w);

private:
    struct RouteState {
        bool sustain = false;
        bool pitchbend = false;
        KonfytLayerWidget* widget;
    };

    QMap<KfJackMidiRoute*, RouteState> routeStateMap;
    QMap<KonfytLayerWidget*, KfJackMidiRoute*> widgetRouteMap;

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
