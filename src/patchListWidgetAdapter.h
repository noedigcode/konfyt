/******************************************************************************
 *
 * Copyright 2024 Gideon van der Kolf
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

#include "GidListWidget.h"
#include "konfytUtils.h"
#include "konfytPatch.h"

#include <QListWidgetItem>
#include <QMap>
#include <QObject>

class PatchListWidgetAdapter : public QObject
{
    Q_OBJECT
public:
    explicit PatchListWidgetAdapter(QObject *parent = nullptr);

    void init(GidListWidget* listWidget);
    void addPatch(Patch* patch);
    void insertPatch(Patch* patch, int index);
    void addPatches(QList<Patch*> patches);
    void removePatch(Patch* patch);
    void patchModified(Patch* patch);
    void clear();
    void moveSelectedPatchUp();
    void moveSelectedPatchDown();
    void setPatchNumbersVisible(bool visible);
    void setPatchNotesVisible(bool visible);
    void setPatchLoaded(Patch* patch, bool loaded);
    void setCurrentPatch(Patch* patch);

signals:
    /* Emitted when current patch selection changed. nullptr if no patch is selected. */
    void patchSelected(Patch* patch);
    void patchMoved(int indexFrom, int indexTo);

private:
    struct PatchData
    {
        bool loaded = false;
        QListWidgetItem* item = nullptr;
    };
    const QString notenames = "CDEFGAB";

    GidListWidget* mListWidget = nullptr;
    QMap<Patch*, PatchData> patchDataMap;
    QMap<QListWidgetItem*, Patch*> itemPatchMap;
    bool mNumbersVisible = true;
    bool mNotesVisible = false;
    Patch* mCurrentPatch = nullptr;

private slots:
    void onListWidgetCurrentChanged(QListWidgetItem* item);
    void onListWidgetItemMoved(QListWidgetItem* item, int from, int to);
    void updatePatchItem(Patch* patch);
    void updatePatchIcon(Patch* patch);
    void updateAll();
    void moveItem(int indexFrom, int indexTo);
};

#endif // PATCHLISTWIDGETADAPTER_H
