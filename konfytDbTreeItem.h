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
