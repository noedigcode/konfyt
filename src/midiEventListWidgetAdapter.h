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

#ifndef MIDIEVENTLISTWIDGETADAPTER_H
#define MIDIEVENTLISTWIDGETADAPTER_H

#include "konfytMidi.h"

#include <QListWidget>
#include <QObject>

/* This class facilitates displaying MIDI events in a list widget. */

class MidiEventListWidgetAdapter : public QObject
{
    Q_OBJECT
public:
    explicit MidiEventListWidgetAdapter(QObject *parent = nullptr);

    void init(QListWidget* listWidget);
    void addMidiEvent(KonfytMidiEvent ev);

    KonfytMidiEvent selectedMidiEvent();

signals:
    void itemClicked();
    void itemDoubleClicked();

private:
    QListWidget* w = nullptr;
    QList<KonfytMidiEvent> mMidiEvents;
};

#endif // MIDIEVENTLISTWIDGETADAPTER_H
