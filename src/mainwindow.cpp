#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "dsk_tools/dsk_tools.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    dsk_tools::hello();
}

MainWindow::~MainWindow()
{
    delete ui;
}
