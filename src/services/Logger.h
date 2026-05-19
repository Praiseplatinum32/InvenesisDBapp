#ifndef LOGGER_H
#define LOGGER_H

#include <QString>

class Logger {
public:
    static void init(const QString& logFilePath);
    static void cleanup();
};

#endif // LOGGER_H
