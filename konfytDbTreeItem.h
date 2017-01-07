/******************************************************************************
 *
 * Copyright 2017 Gideon van der Kolf
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

#ifndef KONFYT_DB_TREE_ITEM_H
#define KONFYT_DB_TREE_ITEM_H

#include <QString>
#include <QList>

class konfytDbTreeItem
{
public:
    konfytDbTreeItem();
    konfytDbTreeItem(QString newName);
    konfytDbTreeItem(konfytDbTreeItem* newParent, QString newName);

    konfytDbTreeItem* parent;
    QList<konfytDbTreeItem*> children;

    bool hasChildren();
    bool hasParent();

    /* Additional data that can be assigned */
    QString name;
    QString path;
    void* data;

};

#endif // KONFYT_DB_TREE_ITEM_H
