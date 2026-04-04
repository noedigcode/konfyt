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

#include "konfytStructs.h"


KonfytReset konfytResetFromString(QString s, KonfytReset defaultValue)
{
    if (s.toLower() == "inherit") {
        return KonfytReset::Inherit;
    } else if (s.toLower() == "reset") {
        return KonfytReset::Reset;
    } else if (s.toLower() == "noreset") {
        return KonfytReset::NoReset;
    } else {
        return defaultValue;
    }
}

QString konfytResetToString(KonfytReset value)
{
    if (value == KonfytReset::Inherit) {
        return "Inherit";
    } else if (value == KonfytReset::Reset) {
        return "Reset";
    } else {
        return "NoReset";
    }
}

KonfytReset konfytResetFromInherits(QList<KonfytReset> inheritChain, KonfytReset defaultValue)
{
    KonfytReset ret = KonfytReset::Inherit;
    foreach (KonfytReset option, inheritChain) {
        if (ret == KonfytReset::Inherit) {
            ret = option;
        } else {
            break;
        }
    }
    if (ret == KonfytReset::Inherit) {
        ret = defaultValue;
    }
    return ret;
}
