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
