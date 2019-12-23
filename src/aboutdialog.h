#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QWidget>
#include "konfytDefines.h"

#define REPLACE_TXT_APP_VERSION "%APP_VERSION%"
#define REPLACE_TXT_MORE_VERSION "%MORE_VERSION_TEXT%"

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QWidget
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = 0);
    ~AboutDialog();
    void setExtraVersionText(QString txt);

private slots:
    void on_pushButton_clicked();

private:
    Ui::AboutDialog *ui;
};

#endif // ABOUTDIALOG_H
