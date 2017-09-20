#ifndef HOVERFORM_H
#define HOVERFORM_H

#include <QWidget>
#include "src/konfytDefines.h"

#define REPLACE_TXT_APP_VERSION "%APP_VERSION%"

namespace Ui {
class HoverForm;
}

class HoverForm : public QWidget
{
    Q_OBJECT

public:
    explicit HoverForm(QWidget *parent = 0);
    ~HoverForm();

private slots:
    void on_pushButton_clicked();

    void on_groupBox_clicked();

private:
    Ui::HoverForm *ui;
};

#endif // HOVERFORM_H
