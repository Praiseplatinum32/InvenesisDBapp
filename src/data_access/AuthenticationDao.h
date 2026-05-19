#pragma once

#include <QString>

class AuthenticationDao {
public:
    AuthenticationDao() = default;
    ~AuthenticationDao() = default;

    // Authenticates a user using bcrypt. Returns true if successful and sets roleOut.
    bool authenticateUser(const QString& username, const QString& password, QString* roleOut, QString* errOut = nullptr) const;
    
    // Updates the password for a user by verifying the old password first.
    bool updatePassword(const QString& username, const QString& oldPassword, const QString& newPassword, QString* errOut = nullptr) const;

    // Admin password reset: bypasses old password check.
    bool adminResetPassword(const QString& username, const QString& newPassword, QString* errOut = nullptr) const;
};
