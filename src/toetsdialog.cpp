#include "toetsdialog.h"
#include "ui_toetsdialog.h"

ToetsDialog::ToetsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ToetsDialog)
{
    ui->setupUi(this);
}

ToetsDialog::~ToetsDialog()
{
    delete ui;
}
