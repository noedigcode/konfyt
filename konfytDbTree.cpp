#include "konfytDbTree.h"

konfytDbTree::konfytDbTree()
{
    root = new konfytDbTreeItem();
}

void konfytDbTree::clearTree()
{
    removeAllChildren(this->root);
}

/* Recursive function to remove all children items
 * of a treeItem, recursively repeating. */
void konfytDbTree::removeAllChildren(konfytDbTreeItem *item)
{
    while (item->children.count()) {
        konfytDbTreeItem* child = item->children.at(0);
        removeAllChildren(child);
        item->children.removeAt(0);
        delete child;
    }
}
