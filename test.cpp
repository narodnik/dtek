#include "ui_darkwallet.h"

int main( int argc, char* argv[] )
{
    QApplication app(argc, argv);
    QMainWindow *window = new QMainWindow;
    Ui::darkwindow ui;
    ui.setupUi(window);

    window->show();
    return app.exec();
}

