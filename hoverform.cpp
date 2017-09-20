#include "hoverform.h"
#include "ui_hoverform.h"

HoverForm::HoverForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HoverForm)
{
    ui->setupUi(this);

    QString txt = ui->textBrowser->document()->toHtml();
    txt.replace(REPLACE_TXT_APP_VERSION, APP_VERSION);
    ui->textBrowser->document()->setHtml(txt);
}

HoverForm::~HoverForm()
{
    delete ui;
}

void HoverForm::on_pushButton_clicked()
{
    this->hide();
}

void HoverForm::on_groupBox_clicked()
{
    this->hide();
}
