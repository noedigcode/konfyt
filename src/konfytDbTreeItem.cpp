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

#include "konfytDbTreeItem.h"

konfytDbTreeItem::konfytDbTreeItem()
{
    this->name = "Item";
    this->parent = NULL;
}

konfytDbTreeItem::konfytDbTreeItem(QString newName)
{
    this->name = newName;
    this->parent = NULL;
}

konfytDbTreeItem::konfytDbTreeItem(konfytDbTreeItem *newParent, QString newName)
{
    this->parent = newParent;
    this->name = newName;
}

bool konfytDbTreeItem::hasChildren()
{
    return children.count() > 0;
}

bool konfytDbTreeItem::hasParent()
{
    return parent != NULL;
}
