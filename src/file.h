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

#ifndef FILE_H
#define FILE_H

#include <QByteArray>
#include <QString>

class File
{
public:

    struct ReadResult {
        bool ok = false;
        QByteArray data;
        QString filepath;
        QString errorString;
        QString toString();
    };

    struct WriteResult {
        bool ok = false;
        QString filepath;
        QString errorString;
        QString toString();
    };

    static ReadResult readAll(QString filepath);
    static WriteResult write(QString filepath, QByteArray data);
};

#endif // FILE_H
