#ifndef DATABASE_H
#define DATABASE_H

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlTableModel>
#include <QMessageBox>

class Database {
public:
    static bool connect(const QString &host, int port, const QString &dbName, const QString &user, const QString &password) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL");
        db.setHostName(host);
        db.setPort(port);
        db.setDatabaseName(dbName);
        db.setUserName(user);
        db.setPassword(password);

        if (!db.open()) {
            QMessageBox::critical(nullptr, "Database Error", db.lastError().text());
            return false;
        }
        return true;
    }


    static QSqlTableModel* getTableModel(const QString &tableName, QObject *parent = nullptr) {
        QSqlTableModel *model = new QSqlTableModel(parent);
        model->setTable(tableName);
        model->select();
        return model;
    }
};

#endif // DATABASE_H