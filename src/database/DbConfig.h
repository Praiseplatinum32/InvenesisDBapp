#ifndef DBCONFIG_H
#define DBCONFIG_H
#pragma once

#include <QString>

// Simple struct to hold DB settings
struct DbConfig {
    QString host;
    int     port;
    QString dbName;
    QString user;
    QString password;
};

/**
 * Reads DB config from environment variables:
 *   INV_DB_HOST, INV_DB_PORT, INV_DB_NAME, INV_DB_USER, INV_DB_PASSWORD
 *
 * Returns true on success.
 * On failure, returns false and fills 'error' with a human-readable message.
 */
bool loadDbConfigFromEnv(DbConfig &cfg, QString &error);
#endif // DBCONFIG_H
