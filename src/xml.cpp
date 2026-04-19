/******************************************************************************
 *
 * Copyright 2026 Gideon van der Kolf
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

#include "xml.h"


Xml::Xml()
{

}

Xml::Xml(QDomElement element)
{
    fromElement(element);
}

Xml::Xml(QString name, QString text)
{
    mName = name;
    mText = text;
    mValid = true;
}

bool Xml::hasChild(QString name)
{
    return child(name).isValid();
}

Xml Xml::child(QString name)
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

QString Xml::childText(QString name, QString defaultValue)
{
    Xml c = child(name);
    if (c.isValid()) {
        return c.text();
    } else {
        return defaultValue;
    }
}

int Xml::childInt(QString name, int defaultValue)
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

void Xml::setIntFromChild(QString name, int *value)
{
    Xml c = child(name);
    if (c.isValid()) {
        bool ok;
        int v = c.text().toInt(&ok);
        if (ok) { *value = v; }
    }
}

float Xml::childFloat(QString name, float defaultValue)
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

void Xml::setFloatFromChild(QString name, float *value)
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

bool Xml::childBool(QString name, bool defaultValue)
{
    Xml c = child(name);
    if (c.isValid()) {
        return QVariant(c.text()).toBool();
    } else {
        return defaultValue;
    }
}

void Xml::setBoolFromChild(QString name, bool *value)
{
    Xml c = child(name);
    if (c.isValid()) {
        *value = QVariant(c.text()).toBool();
    }
}

QList<Xml> Xml::children()
{
    return mChildren;
}

QList<Xml> Xml::childrenNamed(QStringList names)
{
    QList<Xml> list;
    foreach (Xml child, mChildren) {
        if (names.contains(child.name())) {
            list.append(child);
        }
    }
    return list;
}

QList<Xml> Xml::childrenNamed(QString name)
{
    return childrenNamed(QStringList({name}));
}

QString Xml::attribute(QString key)
{
    return mAttributes.value(key);
}

bool Xml::hasAttribute(QString key)
{
    return mAttributes.contains(key);
}

QMap<QString, QString> Xml::allAttributes()
{
    return mAttributes;
}

void Xml::setAttribute(QString key, QString value)
{
    mAttributes.insert(key, value);
}

QString Xml::name()
{
    return mName;
}

QString Xml::text()
{
    return mText;
}

bool Xml::isValid()
{
    return mValid;
}

void Xml::addChild(Xml child)
{
    mChildren.append(child);
}

void Xml::addTextChild(QString name, QString text)
{
    mChildren.append(Xml(name, text));
}

QDomElement Xml::toDomElement(QDomDocument *doc)
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

QByteArray Xml::toByteArray()
{
    QDomDocument doc;

    doc.appendChild(toDomElement(&doc));
    return doc.toByteArray(4);
}

void Xml::fromElement(QDomElement element)
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

Xml::Result Xml::loadFromData(QByteArray data)
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

QString Xml::Result::toString()
{
    return QString("Line %1, column %2: %3")
            .arg(errorLine).arg(errorColumn).arg(errorMsg);
}
