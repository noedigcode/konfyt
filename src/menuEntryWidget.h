#ifndef MENUITEM_H
#define MENUITEM_H

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QWidget>

class MenuEntryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MenuEntryWidget(QString text = "", bool hasRemoveButton = false,
                      QWidget *parent = nullptr);

    void setLayoutSpacing(int spacing);
    void setText(QString text);
    QString text();

signals:
    void removeButtonClicked();

private:
    QHBoxLayout* layout;
    QLabel* label;

    void highlight(bool hilite);

    // QWidget interface
protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
};

#endif // MENUITEM_H
