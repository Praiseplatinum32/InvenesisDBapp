#include "authentication/logindialog.h"
#include "ui/databaseviewwindow.h"
#include "database/Database.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QIcon appIcon(":/icons/Sphere.png");
    a.setWindowIcon(appIcon);

    if (!Database::connect("localhost", 5432, "invenesis_db", "postgres", "DmEr2861995!!!!")) {
        return -1;
    }

    // if (!Database::connect("db.dwycqohuigokorflerwd.supabase.co", 5432, "postgres", "postgres", "PMN0VhUxfaThDDHB45")) {
    //     return -1;
    // }
    MainWindow w;
    w.show();
    return a.exec();

    return 0;
}
