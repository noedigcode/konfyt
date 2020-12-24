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

#include "konfytDbTree.h"


KfDbTreeItemPtr KonfytDbTreeItem::addChild(QString newName, QString newPath, KfSoundPtr data)
{
    KfDbTreeItemPtr child = KfDbTreeItemPtr(new KonfytDbTreeItem());
    child->parent = this;
    child->name = newName;
    child->path = newPath;
    child->data = data;
    children.append(child);
    return child;
}

bool KonfytDbTreeItem::hasChildren()
{
    return children.count() > 0;
}

bool KonfytDbTreeItem::hasParent()
{
    return (parent != nullptr);
}

KonfytDbTree::~KonfytDbTree()
{
    clearTree();
}

void KonfytDbTree::clearTree()
{
    removeAllChildren(root);
}

/* Recursively remove and delete all children items of a tree item. */
void KonfytDbTree::removeAllChildren(KfDbTreeItemPtr item)
{
    while (item->children.count()) {
        KfDbTreeItemPtr child = item->children.takeFirst();
        removeAllChildren(child);
    }
}
