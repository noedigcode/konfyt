#include "scriptEditWidget.h"

#include <QTextBlock>

ScriptEditWidget::ScriptEditWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    setLineWrapMode(NoWrap);
}

void ScriptEditWidget::keyPressEvent(QKeyEvent *event)
{
    bool noMod = event->modifiers() == Qt::NoModifier;
    bool shiftOnly = event->modifiers() == Qt::ShiftModifier;
    bool ctrlOnly = event->modifiers() == Qt::ControlModifier;

    if (event->key() == Qt::Key_Tab) {

        if (shiftOnly) {

            // TODO Shift+Tab decrease indentation

        } else {
            // Insert spaces instead of tab
            insertPlainText("    ");
        }

    } else if (event->key() == Qt::Key_Slash) {

        if (ctrlOnly) {
            // TODO Ctrl+/ toggles line comment
        }

    } else if ( (event->key() == Qt::Key_Return)
                || (event->key() == Qt::Key_Enter) ) {

        // Entering keeps the indentation of the preceding line.

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

        // Insert newline and indentation
        QString text = "\n";
        for (int i=0; i < indent; i++) {
            text += " ";
        }

        textCursor().insertText(text);

    } else if (event->key() == Qt::Key_Home) {

        if (noMod || shiftOnly) {
            // Home or Shift+Home sets cursor to start of line, toggling between
            // before and after the initial indentation.

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
                               shiftOnly ? QTextCursor::KeepAnchor :
                                       QTextCursor::MoveAnchor);
            setTextCursor(cursor);

        } else {
            // For other modifiers, e.g. Ctrl+Home, use default implementation
            QPlainTextEdit::keyPressEvent(event);
        }

    } else {
        QPlainTextEdit::keyPressEvent(event);
    }
}
