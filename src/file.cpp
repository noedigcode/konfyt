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

#include "file.h"

#include <QFile>


File::ReadResult File::readAll(QString filepath)
{
    ReadResult result;
    result.filepath = filepath;

    QFile file(filepath);
    if (file.open(QIODevice::ReadOnly)) {
        result.data = file.readAll();
        if (file.error() != QFile::NoError) {
            result.ok = false;
            result.errorString = "Error while reading: " + file.errorString();
        } else {
            result.ok = true;
        }
    } else {
        result.ok = false;
        result.errorString = "Error opening file for reading: " + file.errorString();
    }

    return result;
}

File::WriteResult File::write(QString filepath, QByteArray data)
{
    WriteResult result;
    result.filepath = filepath;

    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly)) {
        qint64 nwritten = file.write(data);
        if (nwritten != data.count()) {
            result.ok = false;
            result.errorString = QString("Only %1 of %2 bytes written. %3")
                    .arg(nwritten).arg(data.count()).arg(file.errorString());
        } else if (file.error() != QFile::NoError) {
            result.ok = false;
            result.errorString = "Error while writing: " + file.errorString();
        } else {
            result.ok = true;
        }
    } else {
        result.ok = false;
        result.errorString = "Error opening file for writing: " + file.errorString();
    }

    return result;
}

QString File::WriteResult::toString()
{
    QString ret;
    if (!ok) {
        ret = QString("Error writing file: %1: %2")
                .arg(filepath, errorString);
    }
    return ret;
}

QString File::ReadResult::toString()
{
    QString ret;
    if (!ok) {
        ret = QString("Error reading file: %1: %2")
                .arg(filepath, errorString);
    }
    return ret;
}
