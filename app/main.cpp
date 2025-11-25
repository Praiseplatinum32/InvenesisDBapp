#include "authentication/logindialog.h"
#include "ui/databaseviewwindow.h"
#include "database/Database.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QIcon appIcon(":/icons/Sphere.png");
    a.setWindowIcon(appIcon);

    QCoreApplication::setApplicationName("Invenesis screening");
    QCoreApplication::setOrganizationName("Invenesis");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    a.setWindowIcon(QIcon(":/icons/resources/icons/Sphere.png"));

    if (!Database::connect("10.0.0.19", 5432, "invenesisdb", "invenesis_app", "JoKRNegUsUFeaLwH5i")) {
        return -1;
    }

    MainWindow w;
    w.show();
    return a.exec();

    return 0;
}
