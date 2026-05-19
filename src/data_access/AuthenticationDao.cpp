#include "AuthenticationDao.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include "../authentication/qtbcrypt.h"

bool AuthenticationDao::authenticateUser(const QString& username, const QString& password, QString* roleOut, QString* errOut) const {
    QSqlQuery query;
    query.prepare("SELECT password_hash, role FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (!query.exec()) {
        if (errOut) *errOut = query.lastError().text();
        return false;
    }

    if (query.next()) {
        QString stored_hash = query.value(0).toString();
        if (QtBCrypt::hashPassword(password, stored_hash) == stored_hash) {
            if (roleOut) *roleOut = query.value(1).toString();
            return true;
        } else {
            if (errOut) *errOut = "Incorrect password.";
            return false;
        }
    } else {
        if (errOut) *errOut = "User not found.";
        return false;
    }
}

bool AuthenticationDao::updatePassword(const QString& username, const QString& oldPassword, const QString& newPassword, QString* errOut) const {
    QSqlQuery query;
    query.prepare("SELECT password_hash FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (!query.exec()) {
        if (errOut) *errOut = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        if (errOut) *errOut = "User not found.";
        return false;
    }

    QString stored_hash = query.value(0).toString();
    if (QtBCrypt::hashPassword(oldPassword, stored_hash) != stored_hash) {
        if (errOut) *errOut = "Old password is incorrect.";
        return false;
    }

    QString newSalt = QtBCrypt::generateSalt();
    QString newHashedPassword = QtBCrypt::hashPassword(newPassword, newSalt);

    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE users SET password_hash = :newHash WHERE username = :username");
    updateQuery.bindValue(":newHash", newHashedPassword);
    updateQuery.bindValue(":username", username);

    if (!updateQuery.exec()) {
        if (errOut) *errOut = updateQuery.lastError().text();
        return false;
    }

    return true;
}

bool AuthenticationDao::adminResetPassword(const QString& username, const QString& newPassword, QString* errOut) const {
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT 1 FROM users WHERE username = :username");
    checkQuery.bindValue(":username", username);

    if (!checkQuery.exec()) {
        if (errOut) *errOut = checkQuery.lastError().text();
        return false;
    }

    if (!checkQuery.next()) {
        if (errOut) *errOut = "User not found.";
        return false;
    }

    QString salt = QtBCrypt::generateSalt();
    QString newHash = QtBCrypt::hashPassword(newPassword, salt);

    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE users SET password_hash = :hash WHERE username = :username");
    updateQuery.bindValue(":hash", newHash);
    updateQuery.bindValue(":username", username);

    if (!updateQuery.exec()) {
        if (errOut) *errOut = updateQuery.lastError().text();
        return false;
    }

    if (updateQuery.numRowsAffected() == 0) {
        if (errOut) *errOut = "Update failed (no rows affected).";
        return false;
    }

    return true;
}
