#include "authentication/logindialog.h"
#include "ui/databaseviewwindow.h"
#include "src/database/Database.h"
#include "src/services/Logger.h"
#include "database/DbConfig.h"
#include <QApplication>
#include <QMessageBox>
#include <QFile>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Initialize Logger
    Logger::init("invenesis_app.log");
    qInfo() << "========================================";
    qInfo() << "Application started";

    QIcon appIcon(":/icons/Sphere.png");
    a.setWindowIcon(appIcon);

    QCoreApplication::setApplicationName("Invenesis screening");
    QCoreApplication::setOrganizationName("Invenesis");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    a.setWindowIcon(QIcon(":/icons/resources/icons/Sphere.png"));

    // Load stylesheet
    QFile styleFile(":/styles/resources/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        a.setStyleSheet(styleSheet);
        styleFile.close();
    } else {
        qWarning() << "Failed to load stylesheet";
    }

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
    QString dbErr;
    if (!Database::connect(cfg.host,
                           cfg.port,
                           cfg.dbName,
                           cfg.user,
                           cfg.password,
                           &dbErr)) {
        QMessageBox::critical(nullptr,
                              QObject::tr("Database error"),
                              QObject::tr("Could not connect to the database:\n%1").arg(dbErr));
        return -1;
    }

    // 3. Start the UI
    MainWindow w;
    w.show();
    int ret = a.exec();

    qInfo() << "Application shutting down";
    Logger::cleanup();
    return ret;
}
