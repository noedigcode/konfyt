#ifndef TOETSDIALOG_H
#define TOETSDIALOG_H

#include <QDialog>

namespace Ui {
class ToetsDialog;
}

class ToetsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ToetsDialog(QWidget *parent = 0);
    ~ToetsDialog();
    
private:
    Ui::ToetsDialog *ui;
};

#endif // TOETSDIALOG_H
