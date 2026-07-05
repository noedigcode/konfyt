#ifndef WARNINGLISTADAPTER_H
#define WARNINGLISTADAPTER_H

#include "biqmap.h"

#include <QListWidget>
#include <QObject>

#include <functional>


template<typename T>
class WarningListAdapter
{
public:
    struct Data {
        T ref;
        int channel = 0;
    };

    void setListWidget(QObject* context, QListWidget* w)
    {
        mListWidget = w;
        context->connect(w, &QListWidget::itemDoubleClicked,
                context, [=](QListWidgetItem* item)
        {
            if (!mDoubleClickCallback) { return; }

            if (!itemDataMap.contains(item)) { return; }
            const Data& data = itemDataMap[item];

            mDoubleClickCallback(data.ref, data.channel);
        });
    }

    void setDoubleClickCallback(std::function<void(T, int)> callback)
    {
        mDoubleClickCallback = callback;
    }
    QListWidgetItem* getItem(T ref, int channel)
    {
        // Get list widget item from maps using ref and channel
        QListWidgetItem* item = nullptr;
        if (refChanItemMap.contains(ref)) {
            const QMap<int, QListWidgetItem*>& map = refChanItemMap[ref];
            if (map.contains(channel)) {
                item = map[channel];
            }
        }
        return item;
    }
    void updateSubject(T ref, int channel, QString text)
    {
        QListWidgetItem* item = getItem(ref, channel);

        if (text.isEmpty()) {
            // If text is empty, remove the list widget item (if any)
            if (item) {
                itemDataMap.remove(item);
                if (refChanItemMap.contains(ref)) {
                    QMap<int, QListWidgetItem*>& map = refChanItemMap[ref];
                    map.remove(channel);
                }
                delete item; // Removes item from list widget
            }
        } else {
            // Text is not empty. Set item text, adding a new one if one does
            // not exist yet.
            if (!item) {
                item = new QListWidgetItem();

                // Add to ref-->channel-->item map
                if (refChanItemMap.contains(ref)) {
                    QMap<int, QListWidgetItem*>& map = refChanItemMap[ref];
                    map.insert(channel, item);
                } else {
                    QMap<int, QListWidgetItem*> map;
                    map.insert(channel, item);
                    refChanItemMap.insert(ref, map);
                }

                // Add to item-->ref/channel map
                Data data;
                data.ref = ref;
                data.channel = channel;
                itemDataMap.insert(item, data);

                if (mListWidget) {
                    mListWidget->addItem(item);
                }
            }
            item->setText(text);
            item->setToolTip(text);
        }
    }
    void removeItem(QListWidgetItem* item)
    {
        if (itemDataMap.contains(item)) {
            Data data = itemDataMap[item];
            if (refChanItemMap.contains(data.ref)) {
                QMap<int, QListWidgetItem*>& map = refChanItemMap[data.ref];
                map.remove(data.channel);
            }
            itemDataMap.remove(item);
        }
    }
    void removeSubject(T ref)
    {
        if (refChanItemMap.contains(ref)) {
            const QMap<int, QListWidgetItem*>& map = refChanItemMap[ref];
            foreach (QListWidgetItem* item, map.values()) {
                itemDataMap.remove(item);
                delete item; // Removes from list widget
            }
            refChanItemMap.remove(ref);
        }
    }

private:
    QListWidget* mListWidget = nullptr;
    std::function<void(T, int)> mDoubleClickCallback;
    //BiQMap<DataPtr, QListWidgetItem*> map;

    QMap<T, QMap<int, QListWidgetItem*>> refChanItemMap;
    QMap<QListWidgetItem*, Data> itemDataMap;
};

#endif // WARNINGLISTADAPTER_H
