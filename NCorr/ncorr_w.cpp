#include "ncorr_w.h"
#include "ui_ncorr_w.h"

Ncorr_W::Ncorr_W(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Ncorr_W)
{
    ui->setupUi(this);
}

Ncorr_W::~Ncorr_W()
{
    delete ui;
}
