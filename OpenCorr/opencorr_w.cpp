#include "opencorr_w.h"
#include "ui_opencorr_w.h"

OpenCorr_W::OpenCorr_W(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OpenCorr_W)
{
    ui->setupUi(this);
}

OpenCorr_W::~OpenCorr_W()
{
    delete ui;
}
