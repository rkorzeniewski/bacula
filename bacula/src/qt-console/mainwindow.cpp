
#include "mainwindow.h"


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
   setupUi(this);
   stackedWidget->setCurrentIndex(0);
   textEdit->setPlainText("Hello Baculites\nThis is the main console window.");
}
