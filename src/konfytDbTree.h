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

#ifndef KONFYT_DB_TREE_H
#define KONFYT_DB_TREE_H

#include <QString>
#include <QList>

class KonfytDbTreeItem
{
public:
    KonfytDbTreeItem();
    KonfytDbTreeItem(QString newName);
    KonfytDbTreeItem(KonfytDbTreeItem* newParent, QString newName);

    KonfytDbTreeItem* parent;
    QList<KonfytDbTreeItem*> children;

    bool hasChildren();
    bool hasParent();

    /* Additional data that can be assigned */
    QString name;
    QString path;
    void* data;
};



class KonfytDbTree
{
public:
    KonfytDbTree();
    ~KonfytDbTree();

    void clearTree();
    void removeAllChildren(KonfytDbTreeItem* item);
    KonfytDbTreeItem* root;
};

#endif // KONFYT_DB_TREE_H
