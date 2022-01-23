/******************************************************************************
 *
 * Copyright 2022 Gideon van der Kolf
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

#ifndef PATCHLISTWIDGETADAPTER_H
#define PATCHLISTWIDGETADAPTER_H

#include "konfytDefines.h"
#include "konfytPatch.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QObject>

class PatchListWidgetAdapter : public QObject
{
    Q_OBJECT
public:
    explicit PatchListWidgetAdapter(QObject *parent = nullptr);

    void init(QListWidget* listWidget);
    void addPatch(KonfytPatch* patch);
    void addPatches(QList<KonfytPatch*> patches);
    void removePatch(KonfytPatch* patch);
    void patchModified(KonfytPatch* patch);
    void clear();
    void moveSelectedPatchUp();
    void moveSelectedPatchDown();
    void setPatchNumbersVisible(bool visible);
    void setPatchNotesVisible(bool visible);
    void setPatchLoaded(KonfytPatch* patch, bool loaded);
    void setCurrentPatch(KonfytPatch* patch);

signals:
    /* Emitted when current patch selection changed. nullptr if no patch is selected. */
    void patchSelected(KonfytPatch* patch);
    void patchMoved(int indexFrom, int indexTo);

private:
    struct PatchData
    {
        bool loaded = false;
        QListWidgetItem* item = nullptr;
    };
    const QString notenames = "CDEFGAB";

    QListWidget* w = nullptr;
    QMap<KonfytPatch*, PatchData> patchDataMap;
    QMap<QListWidgetItem*, KonfytPatch*> itemPatchMap;
    bool mNumbersVisible = true;
    bool mNotesVisible = false;
    KonfytPatch* mCurrentPatch = nullptr;

private slots:
    void onListWidgetCurrentChanged(QListWidgetItem* item);
    void onListWidgetModelRowsMoved(const QModelIndex &parent, int start,
                                    int end, const QModelIndex &destination,
                                    int row);
    void updatePatchItem(KonfytPatch* patch);
    void updatePatchIcon(KonfytPatch* patch);
    void updateAll();
    void moveItem(int indexFrom, int indexTo);
};

#endif // PATCHLISTWIDGETADAPTER_H
