#ifndef SCRIPTEDITWIDGET_H
#define SCRIPTEDITWIDGET_H

#include <QPlainTextEdit>
#include <QWidget>

class ScriptEditWidget : public QPlainTextEdit
{
    Q_OBJECT
public:
    ScriptEditWidget(QWidget* parent = nullptr);

    // QWidget interface
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
