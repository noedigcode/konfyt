#ifndef HOVERFORM_H
#define HOVERFORM_H

#include <QWidget>

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
