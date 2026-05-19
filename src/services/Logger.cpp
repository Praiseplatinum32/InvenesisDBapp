#include "Logger.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

static QFile* logFile = nullptr;
static QMutex logMutex;

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    QMutexLocker locker(&logMutex);
    
    QString txt;
    switch (type) {
    case QtDebugMsg:    txt = QString("[Debug] "); break;
    case QtInfoMsg:     txt = QString("[Info] "); break;
    case QtWarningMsg:  txt = QString("[Warning] "); break;
    case QtCriticalMsg: txt = QString("[Critical] "); break;
    case QtFatalMsg:    txt = QString("[Fatal] "); break;
    }
    
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logMessage = QString("[%1] %2 %3\n").arg(currentDateTime, txt, msg);
    
    if (logFile && logFile->isOpen()) {
        QTextStream ts(logFile);
        ts << logMessage;
        ts.flush();
    }
    
    // Also print to console
    QTextStream console(stdout);
    console << logMessage;
    console.flush();
}

void Logger::init(const QString& logFilePath) {
    logFile = new QFile(logFilePath);
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(messageHandler);
    }
}

void Logger::cleanup() {
    if (logFile) {
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }
}
