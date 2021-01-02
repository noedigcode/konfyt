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

#include "midiEventListWidgetAdapter.h"

MidiEventListWidgetAdapter::MidiEventListWidgetAdapter(QObject *parent) : QObject(parent)
{

}

void MidiEventListWidgetAdapter::init(QListWidget *listWidget)
{
    w = listWidget;
    connect(w, &QListWidget::itemClicked,
            this, &MidiEventListWidgetAdapter::itemClicked);
    connect(w, &QListWidget::itemDoubleClicked,
            this, &MidiEventListWidgetAdapter::itemDoubleClicked);
}

void MidiEventListWidgetAdapter::addMidiEvent(KonfytMidiEvent ev)
{
    if (!w) { return; }

    w->insertItem(0, ev.toString());
    mMidiEvents.insert(0, ev);

    // The list shouldn't get too crowded
    while (mMidiEvents.count() > 100) {
        mMidiEvents.removeLast();
        delete w->item(w->count() - 1);
    }

    // Select last received event
    w->setCurrentRow(0);
}

KonfytMidiEvent MidiEventListWidgetAdapter::selectedMidiEvent()
{
    return mMidiEvents.value(w->currentRow());
}
