#ifndef CONSOLEDIALOG_H
#define CONSOLEDIALOG_H

#include <QDialog>

namespace Ui {
class ConsoleDialog;
}

class ConsoleDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ConsoleDialog(QWidget *parent = 0);
    ~ConsoleDialog();

    void userMessage(QString message);
    void setShowMidiEvents(bool show);
    
private slots:
    void on_pushButton_Clear_clicked();

    void on_checkBox_ShowMidiEvents_clicked();

private:
    Ui::ConsoleDialog *ui;
};

#endif // CONSOLEDIALOG_H
