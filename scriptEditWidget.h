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
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // SCRIPTEDITWIDGET_H
