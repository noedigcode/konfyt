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

#ifndef XML_H
#define XML_H

#include <QString>
#include <QDomElement>
#include <QMap>
#include <QVariant>

class Xml
{
public:
    Xml();
    Xml(QDomElement element);
    Xml(QString name, QString text = "");

    struct Result {
        bool ok = false;
        QString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;
        QString toString();
    };

    Result loadFromData(QByteArray data);

    bool hasChild(QString name);
    Xml child(QString name);

    /* Get value of the first child with specified name, or returns the
     * specified default value if no such child exists. */
    QString childText(QString name, QString defaultValue = "");
    int childInt(QString name, int defaultValue = 0);
    float childFloat(QString name, float defaultValue = 0);
    bool childBool(QString name, bool defaultValue = false);

    /* Set *value to that of the first child with the specified name, only if
     * the child exists and the value can successfully be converted. */
    void setIntFromChild(QString name, int* value);
    void setFloatFromChild(QString name, float* value);
    void setBoolFromChild(QString name, bool* value);

    QList<Xml> children();
    QList<Xml> childrenNamed(QStringList names);
    QList<Xml> childrenNamed(QString name);

    QString attribute(QString key);
    bool hasAttribute(QString key);
    QMap<QString, QString> allAttributes();
    void setAttribute(QString key, QString value);

    QString name();
    QString text();
    bool isValid();

    void addChild(Xml child);
    void addTextChild(QString name, QString text);

    QDomElement toDomElement(QDomDocument* doc);
    QByteArray toByteArray();

private:
    bool mValid = false;
    QString mName;
    QMap<QString, QString> mAttributes;
    QList<Xml> mChildren;
    QString mText;

    void fromElement(QDomElement element);
};


#endif // XML_H
