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

#ifndef BIQMAP_H
#define BIQMAP_H

#include <QMap>

/* A bi-directional map using two instances of QMap. */

template <typename T, typename U>
class BiQMap {
public:
    QMap<T, U>& leftmap() { return maptu; }
    QMap<U, T>& rightmap() { return maput; }

    void insert(T a, U b)
    {
        maptu.insert(a, b);
        maput.insert(b, a);
    }

    U value(T key) { return maptu.value(key); }
    T value(U key) { return maput.value(key); }

    void remove(T key)
    {
        U val = maptu.take(key);
        maput.remove(val);
    }
    void remove(U key)
    {
        T val = maput.take(key);
        maput.remove(val);
    }
private:
    QMap<T, U> maptu;
    QMap<U, T> maput;
};

#endif // BIQMAP_H
