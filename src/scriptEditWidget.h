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

#ifndef SCRIPTEDITWIDGET_H
#define SCRIPTEDITWIDGET_H

#include <QPlainTextEdit>
#include <QWidget>

class ScriptEditWidget : public QPlainTextEdit
{
    Q_OBJECT
public:
    ScriptEditWidget(QWidget* parent = nullptr);

protected:
    const QString INDENT = "    ";

    void keyPressEvent(QKeyEvent *event) override;

    void insertIndent();
    void deIndent();
    void toggleBlockComment();
    void insertEnterKeepIndentation();
    void gotoLineStartBeforeOrAfterIndent(bool shift);

    int lineStartPos(const QTextCursor& c);
    int lineEndPos(const QTextCursor& c);
    QList<QTextCursor> getLineStartsOfSelection();

    void select(int startPos, int endPos);
};

#endif // SCRIPTEDITWIDGET_H
