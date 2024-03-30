/******************************************************************************
 *
 * Copyright 2024 Gideon van der Kolf
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

#ifndef KONFYT_DEFINES_H
#define KONFYT_DEFINES_H

#include <QString>


#define APP_NAME "Konfyt"
#define APP_VERSION "1.6.1-dev2"
#define APP_YEAR "2024"

#define n2s(x) QString::number(x)
#define bool2int(x) (x ? 1 : 0)
#define bool2str(x) (x ? "1" : "0")
#define int2bool(x) (x!=0)
#define Qstr2bool(x) (x!="0")

#define STRING_PROJECT_DIR "$PROJ_DIR$"
#define KONFYT_PATCH_SUFFIX "konfytpatch"

// ============================================================================
// Assert
// ============================================================================

/* Q_ASSERT if in debug mode. If not in debug mode and the condition is false,
 * print file, line, function name and condition to stdout. */
#define KONFYT_ASSERT(cond) Q_ASSERT(cond); if (!(cond)) { konfytAssertMsg(__FILE__, __LINE__, __func__, #cond); }

/* Calls a failing KONFYT_ASSERT. text must be a non-null string literal. */
#define KONFYT_ASSERT_FAIL(text) KONFYT_ASSERT(!text)

/* Like KONFYT_ASSERT but contains a return statement to return out of the
 * current function if the condition is false. */
#define KONFYT_ASSERT_RETURN(cond) Q_ASSERT(cond); if (!(cond)) { konfytAssertMsg(__FILE__, __LINE__, __func__, #cond); return; }

/* Like KONFYT_ASSERT_RETURN with a return value. */
#define KONFYT_ASSERT_RETURN_VAL(cond, ret) Q_ASSERT(cond); if (!(cond)) { konfytAssertMsg(__FILE__, __LINE__, __func__, #cond); return ret; }

/* Like KONFYT_ASSERT but contains a continue statement for loops. */
#define KONFYT_ASSERT_CONTINUE(cond) Q_ASSERT(cond); if (!(cond)) { konfytAssertMsg(__FILE__, __LINE__, __func__, #cond); continue; }

void konfytAssertMsg(const char* file, int line, const char* func,
                     const char* text);

// ============================================================================

int wrapIndex(int index, int listLength);
QString sanitiseFilename(QString path);
QString getUniquePath(QString dirname, QString name, QString extension);
QString getCompileVersionText();


#endif // KONFYT_DEFINES_H
