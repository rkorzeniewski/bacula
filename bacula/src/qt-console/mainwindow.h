#include <QtGui>
#include "ui_main.h"

class MainWindow : public QMainWindow, public Ui::MainForm    
{

public:
   MainWindow(QWidget *parent = 0);
};
