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

#include "patchListWidgetAdapter.h"

PatchListWidgetAdapter::PatchListWidgetAdapter(QObject *parent) : QObject(parent)
{

}

void PatchListWidgetAdapter::init(QListWidget *listWidget)
{
    w = listWidget;
    connect(w, &QListWidget::currentItemChanged,
            this, &PatchListWidgetAdapter::onListWidgetCurrentChanged);
}

void PatchListWidgetAdapter::addPatch(KonfytPatch *patch)
{
    KONFYT_ASSERT(!patchDataMap.contains(patch));

    PatchData data;
    data.item = new QListWidgetItem();
    patchDataMap.insert(patch, data);
    itemPatchMap.insert(data.item, patch);
    w->addItem(data.item);
    updatePatchItem(patch);
}

void PatchListWidgetAdapter::addPatches(QList<KonfytPatch *> patches)
{
    foreach (KonfytPatch* p, patches) {
        addPatch(p);
    }
}

void PatchListWidgetAdapter::removePatch(KonfytPatch *patch)
{
    KONFYT_ASSERT(patchDataMap.contains(patch));
    KONFYT_ASSERT(itemPatchMap.values().contains(patch));
    QListWidgetItem* item = patchDataMap.take(patch).item;
    itemPatchMap.remove(item);
    KONFYT_ASSERT(item);
    delete item;
    updateAll();
}

void PatchListWidgetAdapter::patchModified(KonfytPatch *patch)
{
    updatePatchItem(patch);
}

void PatchListWidgetAdapter::clear()
{
    patchDataMap.clear();
    itemPatchMap.clear();
    w->clear();
}

void PatchListWidgetAdapter::moveSelectedPatchUp()
{
    int from = w->currentRow();
    if (from < 0) { return; }
    int to = from - 1;
    if (to < 0) { to = w->count()-1; }
    moveItem(from, to);
    emit patchMoved(from, to);
}

void PatchListWidgetAdapter::moveSelectedPatchDown()
{
    int from = w->currentRow();
    if (from < 0) { return; }
    int to = from + 1;
    if (to >= w->count()) { to = 0; }
    moveItem(from, to);
    emit patchMoved(from, to);
}

void PatchListWidgetAdapter::setPatchNumbersVisible(bool visible)
{
    mNumbersVisible = visible;
    updateAll();
}

void PatchListWidgetAdapter::setPatchNotesVisible(bool visible)
{
    mNotesVisible = visible;
    updateAll();
}

void PatchListWidgetAdapter::setPatchLoaded(KonfytPatch *patch, bool loaded)
{
    if (patch == nullptr) { return; }
    KONFYT_ASSERT_RETURN(patchDataMap.contains(patch));

    PatchData& data = patchDataMap[patch];
    data.loaded = loaded;
    updatePatchItem(patch);
}

void PatchListWidgetAdapter::setCurrentPatch(KonfytPatch *patch)
{
    KonfytPatch* lastPatch = mCurrentPatch;
    mCurrentPatch = patch;

    if (lastPatch) { updatePatchIcon(lastPatch); }
    if (patch) { updatePatchIcon(patch); }

    QListWidgetItem* item = nullptr;
    if (patchDataMap.contains(patch)) { item = patchDataMap.value(patch).item; }
    w->setCurrentItem(item);
}

void PatchListWidgetAdapter::onListWidgetCurrentChanged(QListWidgetItem *item)
{
    KonfytPatch* patch = itemPatchMap.value(item);
    emit patchSelected(patch);
}

void PatchListWidgetAdapter::updatePatchItem(KonfytPatch *patch)
{
    KONFYT_ASSERT_RETURN(patchDataMap.contains(patch));
    PatchData data = patchDataMap.value(patch);

    int index = w->row(data.item);
    QString txt;
    if (mNumbersVisible) {
        txt.append(QString("%1 ").arg(index+1));
    }
    if (mNotesVisible) {
        txt.append(QString(notenames.at(index%notenames.length())) + " ");
    }
    txt.append(patch->name());
    data.item->setText(txt);

    if (data.loaded) {
        data.item->setForeground(QBrush(Qt::white));
    } else {
        data.item->setForeground(QBrush(Qt::gray));
    }

    updatePatchIcon(patch);
}

void PatchListWidgetAdapter::updatePatchIcon(KonfytPatch *patch)
{
    if (!patchDataMap.contains(patch)) {
        // This could be the case if a patch was removed
        return;
    }

    QListWidgetItem* item = patchDataMap.value(patch).item;

    if (patch == mCurrentPatch) {
        item->setIcon(QIcon(":/icons/play.png"));
    } else {
        if (patch->alwaysActive) {
            item->setIcon(QIcon(":/icons/alwaysactive.png"));
        } else {
            item->setIcon(QIcon(":/icons/empty.png"));
        }
    }
}

void PatchListWidgetAdapter::updateAll()
{
    foreach (KonfytPatch* patch, patchDataMap.keys()) {
        updatePatchItem(patch);
    }
}

void PatchListWidgetAdapter::moveItem(int indexFrom, int indexTo)
{
    KONFYT_ASSERT_RETURN( (indexTo >= 0) && (indexTo <= w->count()) );

    w->blockSignals(true);
    QListWidgetItem* item = w->takeItem(indexFrom);
    KONFYT_ASSERT(item);
    w->insertItem(indexTo, item);
    w->setCurrentItem(item);
    w->blockSignals(false);
    updateAll();
}
