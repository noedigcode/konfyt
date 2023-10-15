/******************************************************************************
 *
 * Copyright 2023 Gideon van der Kolf
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

#ifndef KONFYT_STRUCTS_H
#define KONFYT_STRUCTS_H

#include <QApplication>
#include <QSharedPointer>
#include <QString>

// =============================================================================

enum KonfytSoundType
{
    KfSoundTypeUndefined,
    KfSoundTypeSoundfont,
    KfSoundTypeSfz,
    KfSoundTypePatch
};

// =============================================================================

struct KonfytSoundPreset
{
    QString name;
    int bank = -1;       // Bank number
    int program = -1;    // Program/preset number
};

// =============================================================================

struct KonfytSound
{
    KonfytSound(KonfytSoundType type) : type(type) {}
    KonfytSoundType type = KfSoundTypeUndefined;
    QString filename;
    QString name;
    QList<KonfytSoundPreset> presets;
};

typedef QSharedPointer<KonfytSound> KfSoundPtr;

// =============================================================================

struct KonfytAppInfo
{
    QString exePath;
    bool bridge = false;
    bool headless = false;
    bool carla = false;
    QStringList filesToLoad;
    QString jackClientName;
};


#endif // KONFYT_STRUCTS_H
