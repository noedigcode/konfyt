#ifndef KONFYT_DB_TREE_H
#define KONFYT_DB_TREE_H

#include "konfytDbTreeItem.h"

class konfytDbTree
{
public:
    konfytDbTree();

    void clearTree();
    void removeAllChildren(konfytDbTreeItem* item);
    konfytDbTreeItem* root;


};

#endif // KONFYT_DB_TREE_H
