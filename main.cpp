#include "logindialog.h"
#include "databaseviewwindow.h"
#include "database.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QIcon appIcon(":/icon/icon/Sphere.png");
    a.setWindowIcon(appIcon);

    if (!Database::connect("localhost", 5432, "invenesis_db", "postgres", "DmEr2861995!!!!")) {
        return -1;
    }
    MainWindow w;
    w.show();
    return a.exec();

    return 0;
}
