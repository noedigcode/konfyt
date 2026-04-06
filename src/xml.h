#ifndef XML_H
#define XML_H

#include <QString>
#include <QFile>
#include <QDomElement>
#include <QMap>
#include <QVariant>

class Xml
{
public:
    Xml() {}
    Xml(QDomElement element)
    {
        fromElement(element);
    }
    Xml(QString name, QString text = "")
    {
        mName = name;
        mText = text;
        mValid = true;
    }

    struct Result {
        bool ok = false;
        QString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;
        QString toString()
        {
            return QString("Line %1, column %2: %3")
                    .arg(errorLine).arg(errorColumn).arg(errorMsg);
        }
    };

    Result loadFromData(QByteArray data)
    {
        QDomDocument doc;
        Result result;
        result.ok = doc.setContent(data, false, &result.errorMsg,
                                          &result.errorLine,
                                          &result.errorColumn);
        if (!result.ok) {
            return result;
        }

        fromElement(doc.documentElement());
        return result;
    }

    bool hasChild(QString name)
    {
        return child(name).isValid();
    }
    Xml child(QString name)
    {
        Xml ret;
        foreach (Xml child, mChildren) {
            if (child.name() == name) {
                ret = child;
                break;
            }
        }
        return ret;
    }
    QString childText(QString name, QString defaultValue = "")
    {
        Xml c = child(name);
        if (c.isValid()) {
            return c.text();
        } else {
            return defaultValue;
        }
    }
    int childInt(QString name, int defaultValue = 0)
    {
        Xml c = child(name);
        if (c.isValid()) {
            bool ok;
            int value = c.text().toInt(&ok);
            if (!ok) { value = defaultValue; }
            return value;
        } else {
            return defaultValue;
        }
    }
    /* Set *value to int value of child with specified name, only if the child
     * exists and it can successfully be converted to an int. */
    void setIntFromChild(QString name, int* value)
    {
        Xml c = child(name);
        if (c.isValid()) {
            bool ok;
            int v = c.text().toInt(&ok);
            if (ok) { *value = v; }
        }
    }
    float childFloat(QString name, float defaultValue = 0)
    {
        Xml c = child(name);
        if (c.isValid()) {
            bool ok;
            float value = c.text().toFloat(&ok);
            if (!ok) { value = defaultValue; }
            return value;
        } else {
            return defaultValue;
        }
    }
    void setFloatFromChild(QString name, float* value)
    {
        Xml c = child(name);
        if (c.isValid()) {
            bool ok;
            float v = c.text().toFloat(&ok);
            if (ok) {
                *value = v;
            }
        }
    }
    bool childBool(QString name, bool defaultValue = false)
    {
        Xml c = child(name);
        if (c.isValid()) {
            return QVariant(c.text()).toBool();
        } else {
            return defaultValue;
        }
    }
    /* Set *value to bool value of child with specified name, only if the
     * child exists. */
    void setBoolFromChild(QString name, bool* value)
    {
        Xml c = child(name);
        if (c.isValid()) {
            *value = QVariant(c.text()).toBool();
        }
    }
    QList<Xml> children()
    {
        return mChildren;
    }
    QList<Xml> childrenNamed(QStringList names)
    {
        QList<Xml> list;
        foreach (Xml child, mChildren) {
            if (names.contains(child.name())) {
                list.append(child);
            }
        }
        return list;
    }
    QList<Xml> childrenNamed(QString name)
    {
        return childrenNamed(QStringList({name}));
    }

    QString attribute(QString key)
    {
        return mAttributes.value(key);
    }
    bool hasAttribute(QString key)
    {
        return mAttributes.contains(key);
    }
    QMap<QString, QString> allAttributes()
    {
        return mAttributes;
    }
    void setAttribute(QString key, QString value)
    {
        mAttributes.insert(key, value);
    }

    QString name()
    {
        return mName;
    }
    QString text()
    {
        return mText;
    }
    bool isValid()
    {
        return mValid;
    }
    QString toString()
    {
        return QString("%1: %2").arg(name(), text());
    }

    void addChild(Xml child)
    {
        mChildren.append(child);
    }
    void addTextChild(QString name, QString text)
    {
        mChildren.append(Xml(name, text));
    }

    QDomElement toDomElement(QDomDocument* doc)
    {
        QDomElement elem = doc->createElement(mName);

        foreach (QString key, mAttributes.keys()) {
            elem.setAttribute(key, mAttributes.value(key));
        }

        if (mChildren.isEmpty()) {
            elem.appendChild(doc->createTextNode(mText));
        } else {
            foreach (Xml child, mChildren) {
                elem.appendChild(child.toDomElement(doc));
            }
        }
        return elem;
    }
    QByteArray toByteArray()
    {
        QDomDocument doc;

        doc.appendChild(toDomElement(&doc));
        return doc.toByteArray(4);
    }

private:
    bool mValid = false;
    QString mName;
    QMap<QString, QString> mAttributes;
    QList<Xml> mChildren;
    QString mText;

    void fromElement(QDomElement element)
    {
        if (element.isNull()) {
            mValid = false;
            return;
        }

        mValid = true;
        mName = element.tagName();

        // Get all attributes
        QDomNamedNodeMap attrs = element.attributes();
        for (int i = 0; i < attrs.count(); ++i) {
            QDomNode attr = attrs.item(i);
            mAttributes.insert(attr.nodeName(), attr.nodeValue());
        }

        // Get all children
        QDomElement elem = element.firstChildElement();
        while (!elem.isNull()) {
            mChildren.append(Xml(elem));
            elem = elem.nextSiblingElement();
        }

        mText = element.text();
    }
};


#endif // XML_H
