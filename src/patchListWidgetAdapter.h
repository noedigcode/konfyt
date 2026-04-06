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
    void addPatch(PatchPtr patch);
    void insertPatch(PatchPtr patch, int index);
    void addPatches(QList<PatchPtr> patches);
    void removePatch(PatchPtr patch);
    void patchModified(PatchPtr patch);
    void clear();
    void moveSelectedPatchUp();
    void moveSelectedPatchDown();
    void setPatchNumbersVisible(bool visible);
    void setPatchNotesVisible(bool visible);
    void setPatchLoaded(PatchPtr patch, bool loaded);
    void setCurrentPatch(PatchPtr patch);

signals:
    /* Emitted when current patch selection changed. nullptr if no patch is selected. */
    void patchSelected(PatchPtr patch);
    void patchMoved(int indexFrom, int indexTo);

private:
    struct PatchData
    {
        bool loaded = false;
        QListWidgetItem* item = nullptr;
    };
    const QString notenames = "CDEFGAB";

    GidListWidget* mListWidget = nullptr;
    QMap<PatchPtr, PatchData> patchDataMap;
    QMap<QListWidgetItem*, PatchPtr> itemPatchMap;
    bool mNumbersVisible = true;
    bool mNotesVisible = false;
    PatchPtr mCurrentPatch;

private slots:
    void onListWidgetCurrentChanged(QListWidgetItem* item);
    void onListWidgetItemMoved(QListWidgetItem* item, int from, int to);
    void updatePatchItem(PatchPtr patch);
    void updatePatchIcon(PatchPtr patch);
    void updateAll();
    void moveItem(int indexFrom, int indexTo);
};

#endif // PATCHLISTWIDGETADAPTER_H
