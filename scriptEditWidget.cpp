#include "scriptEditWidget.h"

#include <QTextBlock>
#include <QDebug>

ScriptEditWidget::ScriptEditWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    setLineWrapMode(NoWrap);
}

void ScriptEditWidget::keyPressEvent(QKeyEvent *event)
{
    bool noMod = event->modifiers() == Qt::NoModifier;
    bool shiftOnly = event->modifiers() == Qt::ShiftModifier;
    bool ctrlOnly = event->modifiers() == Qt::ControlModifier;

    switch (event->key()) {
    case Qt::Key_Tab:
        insertIndent();
        break;
    case Qt::Key_Backtab:
        // Shift+Tab removes tab before cursor
        deIndent();
        break;
    case Qt::Key_Slash:
        if (ctrlOnly) {
            // Ctrl+/ toggles line(s) comment. Only toggle at start of line.
            toggleBlockComment();
        } else {
            // Otherwise use default implementation
            QPlainTextEdit::keyPressEvent(event);
        }
        break;
    case Qt::Key_Return:
        /* no break */
    case Qt::Key_Enter:
        // Entering keeps the indentation of the preceding line.
        insertEnterKeepIndentation();
        break;
    case Qt::Key_Home:
        if (noMod || shiftOnly) {
            // Home or Shift+Home sets cursor to start of line, toggling between
            // before and after the initial indentation.
            gotoLineStartBeforeOrAfterIndent(shiftOnly);
        } else {
            // For other modifiers, e.g. Ctrl+Home, use default implementation
            QPlainTextEdit::keyPressEvent(event);
        }
        break;
    default:
        // Use default implementation
        QPlainTextEdit::keyPressEvent(event);
        break;
    }
}

void ScriptEditWidget::insertIndent()
{
    QTextCursor c = this->textCursor();

    if (c.position() == c.anchor()) {
        // No text selected. Simply insert indent.
        insertPlainText(INDENT);
    } else {
        // Indent each line
        QList<QTextCursor> cursors = getLineStartsOfSelection();
        foreach (QTextCursor c, cursors) {
            c.insertText(INDENT);
        }
        // Select all the affected lines
        // i.e. from 1st cursor's line start to last cursor's line end
        select(lineStartPos(cursors.first()), lineEndPos(cursors.last()));
    }
}

void ScriptEditWidget::deIndent()
{
    QTextCursor c = this->textCursor();

    if (c.position() == c.anchor()) {

        // No text selected.
        // Remove indent before cursor - at most 4 spaces or single tab character

        QTextCursor c = textCursor();
        QString block = c.block().text();
        int pos = c.positionInBlock();
        int deleted = 0;
        for (int i = pos-1; i >= 0; i--) {
            if (block.at(i) == ' ') { // TODO 2023-10-09: Use safer function e.g. .mid() everywhere to quietly fail and not crash on string errors.
                c.deletePreviousChar();
                deleted++;
                if (deleted >= 4) { break; }
            } else if (block.at(i) == '\t') {
                c.deletePreviousChar();
                break;
            } else {
                break;
            }
        }

    } else {

        // Dedent each line

        QList<QTextCursor> cursors = getLineStartsOfSelection();
        foreach (QTextCursor c, cursors) {

            int todelete = 0;
            int n = 0;
            QString blockText = c.block().text();
            // Note, block.length includes the newline character, but the
            // block.text does not, so block.length - 1 must be used below.
            for (int i=0; i < c.block().length() - 1; i++) {
                QChar ch = blockText.at(i);
                if (ch == ' ') {
                    todelete += 1;
                    n++;
                } else if (ch == '\t') {
                    todelete += 1;
                    n += 4;
                } else {
                    break;
                }
                if (n >= 4) { break; }
            }
            for (int i=0; i < todelete; i++) {
                c.deleteChar();
            }
        }

        // Select all the affected lines
        // i.e. from 1st cursor's line start to last cursor's line end
        select(lineStartPos(cursors.first()), lineEndPos(cursors.last()));
    }
}

void ScriptEditWidget::toggleBlockComment()
{
    // Toggle block (multi-line) comments by adding or removing // at start of lines

    QList<QTextCursor> cursors = getLineStartsOfSelection();

    QString text = toPlainText();

    // Determine commented state. False if at least one line is uncommented.
    bool commented = true;
    foreach (const QTextCursor& c, cursors) {
        if (!text.mid(c.position(), -1).startsWith("//")) {
            commented = false;
            break;
        }
    }

    // For each cursor (line start), add or remove comment characters
    foreach (QTextCursor c, cursors) {
        if (commented) {
            c.deleteChar();
            c.deleteChar();
        } else {
            c.insertText("//");
        }
    }

    // Select all the affected lines
    // i.e. from 1st cursor's line start to last cursor's line end
    select(lineStartPos(cursors.first()), lineEndPos(cursors.last()));
}

void ScriptEditWidget::insertEnterKeepIndentation()
{
    // Determine indentation of the current line
    QString block = textCursor().block().text();
    int indent = 0;
    for (int i=0; i < block.count(); i++) {
        if (block.at(i) == ' ') {
            indent += 1;
        } else if (block.at(i) == '\t') {
            indent += 4;
        } else {
            break;
        }
    }

    // Determine if at end of line preceded by a { and whether a } needs to be added.
    bool addClosingBrace = false;
    bool braceOpen = false;
    if (textCursor().atBlockEnd() && block.endsWith("{")) {
        braceOpen = true;

        // Count braces from start of text, + for { and - for }
        QString text = this->toPlainText();
        int braceCount = 0;

        for (int i=0; i < text.count(); i++) {
            QChar c = text.at(i);
            if (c == '{') {
                braceCount++;
            } else if (c == '}') {
                braceCount--;
            }
            if ((braceCount == 0) && (i > textCursor().position())) {
                break;
            }
        }

        // braceCount > 0 means there are more opening braces than matching
        // closing braces.
        addClosingBrace = (braceCount > 0);
    }

    // Create indentation text
    QString indentText = "";
    for (int i=0; i < indent; i++) {
        indentText += " ";
    }

    // Insert newline and indentation
    QTextCursor cur = textCursor();
    cur.insertText("\n" + indentText);
    // If braceOpen, insert additional indent, newline and brace close
    if (braceOpen) {
        cur.insertText(INDENT);
        if (addClosingBrace) {
            int pos = cur.position(); // Position where cursor should end up
            cur.insertText("\n" + indentText + "}");
            // Return cursor to in-between braces
            cur.setPosition(pos);
            setTextCursor(cur);
        }
    }
}

void ScriptEditWidget::gotoLineStartBeforeOrAfterIndent(bool shift)
{
    // Determine the end position of the current line's indentation
    QString block = textCursor().block().text();
    int pos = textCursor().positionInBlock();
    int indentPos = 0;
    for (int i=0; i < block.count(); i++) {
        if (block.at(i) == ' ') { indentPos++; }
        else if (block.at(i) == '\t') { indentPos ++; }
        else { break; }
    }
    // Determine where cursor should go to
    int gotoPos = 0;
    if ((pos == 0) || (pos > indentPos)) {
        gotoPos = indentPos;
    } else {
        gotoPos = 0;
    }
    int deltaPos = gotoPos - pos;

    // Move cursor
    QTextCursor cursor = textCursor();
    cursor.setPosition(textCursor().position() + deltaPos,
                       shift ? QTextCursor::KeepAnchor :
                               QTextCursor::MoveAnchor);
    setTextCursor(cursor);
}

int ScriptEditWidget::lineStartPos(const QTextCursor &c)
{
    return c.block().position();
}

int ScriptEditWidget::lineEndPos(const QTextCursor &c)
{
    return c.block().position() + c.block().length() - 1;
}

QList<QTextCursor> ScriptEditWidget::getLineStartsOfSelection()
{
    QTextCursor c = textCursor();

    int selStart = qMin(c.position(), c.anchor()); // Get left side of selection
    int selEnd = qMax(c.position(), c.anchor()); // Get right side of selection

    // Get start of first line by setting cursor to selection start position
    // and then getting the block position
    c.setPosition(selStart);
    int startOfFirstLine = c.block().position();

    int startPos = startOfFirstLine;
    int endPos = selEnd;

    // Create cursor for each line start
    QList<QTextCursor> cursors;
    for (int i=startPos; i <= endPos; i++) {
        // Set cursor at current position to determine if it is at a line (block) start
        c.setPosition(i);
        if (c.atBlockStart()) {
            // Create cursor for start of line
            cursors.append(c);
        }
    }

    return cursors;
}

void ScriptEditWidget::select(int startPos, int endPos)
{
    QTextCursor c = this->textCursor();
    c.setPosition(startPos);
    c.setPosition(endPos, QTextCursor::KeepAnchor);
    this->setTextCursor(c);
}
