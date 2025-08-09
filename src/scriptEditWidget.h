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

class LineNumberArea;

class ScriptEditWidget : public QPlainTextEdit
{
    Q_OBJECT
public:
    ScriptEditWidget(QWidget* parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    const QString INDENT = "    ";
    const int LINE_NUMBER_AREA_EXTRA_PADDING = 4;
    const int LINE_NUMBER_AREA_RIGHT_PADDING = 4;

    LineNumberArea* lineNumberArea;

    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void insertIndent();
    void deIndent();
    void toggleBlockComment();
    void insertEnterKeepIndentation();
    void gotoLineStartBeforeOrAfterIndent(bool shift);

    int lineStartPos(const QTextCursor& c);
    int lineEndPos(const QTextCursor& c);
    QList<QTextCursor> getLineStartsOfSelection();

    void select(int startPos, int endPos);

private slots:
    void updateLineNumberAreaWidth(int newBlockCount = 0);
    void updateLineNumberArea(const QRect& rect, int dy);
};


class LineNumberArea : public QWidget
{
public:
    LineNumberArea(ScriptEditWidget* editWidget);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ScriptEditWidget* editWidget;
};

#endif // SCRIPTEDITWIDGET_H
