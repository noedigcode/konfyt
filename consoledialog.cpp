#include "consoledialog.h"
#include "ui_consoledialog.h"

#include "mainwindow.h"

ConsoleDialog::ConsoleDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConsoleDialog)
{
    ui->setupUi(this);
}

ConsoleDialog::~ConsoleDialog()
{
    delete ui;
}

void ConsoleDialog::userMessage(QString message)
{
    ui->textBrowser->append(message);
}

void ConsoleDialog::on_pushButton_Clear_clicked()
{
    ui->textBrowser->clear();
}


void ConsoleDialog::on_checkBox_ShowMidiEvents_clicked()
{
    ((MainWindow*)(this->parent()))->setConsoleShowMidiMessages( ui->checkBox_ShowMidiEvents->isChecked() );
}

void ConsoleDialog::setShowMidiEvents(bool show)
{
    ui->checkBox_ShowMidiEvents->setChecked( show );
}
