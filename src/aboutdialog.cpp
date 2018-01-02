#include "aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    QString txt = ui->textBrowser->document()->toHtml();
    txt.replace(REPLACE_TXT_APP_VERSION, APP_VERSION);
    ui->textBrowser->document()->setHtml(txt);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::on_pushButton_clicked()
{
    this->hide();
}

