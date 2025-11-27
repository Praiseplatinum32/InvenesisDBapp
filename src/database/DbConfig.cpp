#include "DbConfig.h"
#include <QProcessEnvironment>

bool loadDbConfigFromEnv(DbConfig &cfg, QString &error)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    error.clear();

    auto getRequired = [&](const QString &key) -> QString {
        QString value = env.value(key);
        if (value.isEmpty()) {
            if (!error.isEmpty())
                error += '\n';
            error += QString("Missing environment variable: %1").arg(key);
        }
        return value;
    };

    // Required values
    cfg.host     = getRequired("INV_DB_HOST");
    cfg.dbName   = getRequired("INV_DB_NAME");
    cfg.user     = getRequired("INV_DB_USER");
    cfg.password = getRequired("INV_DB_PASSWORD");

    // Port: optional, default 5432 if not set
    QString portStr = env.value("INV_DB_PORT");
    if (portStr.isEmpty()) {
        cfg.port = 5432;  // default PostgreSQL port
    } else {
        bool ok = false;
        int p = portStr.toInt(&ok);
        if (!ok) {
            if (!error.isEmpty())
                error += '\n';
            error += "Environment variable INV_DB_PORT is not a valid integer.";
            // fall back to default
            cfg.port = 5432;
        } else {
            cfg.port = p;
        }
    }

    // If 'error' is still empty, everything is fine
    return error.isEmpty();
}
