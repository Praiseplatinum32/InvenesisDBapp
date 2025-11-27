#include "authentication/logindialog.h"
#include "ui/databaseviewwindow.h"
#include "database/Database.h"
#include "database/DbConfig.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    QIcon appIcon(":/icons/Sphere.png");
    a.setWindowIcon(appIcon);

    QCoreApplication::setApplicationName("Invenesis screening");
    QCoreApplication::setOrganizationName("Invenesis");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    a.setWindowIcon(QIcon(":/icons/resources/icons/Sphere.png"));

    // 1. Load DB config from environment variables
    DbConfig cfg;
    QString error;
    if (!loadDbConfigFromEnv(cfg, error)) {
        QMessageBox::critical(nullptr,
                              QObject::tr("Configuration error"),
                              QObject::tr("Database configuration is invalid:\n%1").arg(error));
        return -1;
    }

    // 2. Connect to the database using the env-based config
    if (!Database::connect(cfg.host,
                           cfg.port,
                           cfg.dbName,
                           cfg.user,
                           cfg.password)) {
        QMessageBox::critical(nullptr,
                              QObject::tr("Database error"),
                              QObject::tr("Could not connect to the database."));
        return -1;
    }

    // 3. Start the UI
    MainWindow w;
    w.show();
    return a.exec();
}
