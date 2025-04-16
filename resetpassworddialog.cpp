#include "resetpassworddialog.h"
#include "ui_resetpassworddialog.h"
#include "qtbcrypt.h"
#include <QMessageBox>
#include <QSqlQuery>

ResetPasswordDialog::ResetPasswordDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ResetPasswordDialog)
{
    ui->setupUi(this);
    QAction* showNewPasswordAction = ui->newPasswordLineEdit->addAction(
        QIcon(":/icon/icon/cacher.png"),
        QLineEdit::TrailingPosition
        );

    // Toggle both echo mode AND icon
    connect(showNewPasswordAction, &QAction::triggered, this, [=, this](){
        if(ui->newPasswordLineEdit->echoMode() == QLineEdit::Password) {
            ui->newPasswordLineEdit->setEchoMode(QLineEdit::Normal);
            showNewPasswordAction->setIcon(QIcon(":/icon/icon/visible.png")); // open eye clearly shown
        } else {
            ui->newPasswordLineEdit->setEchoMode(QLineEdit::Password);
            showNewPasswordAction->setIcon(QIcon(":/icon/icon/cacher.png")); // closed eye clearly shown
        }
    });

    QAction* showOldPasswordAction = ui->oldPasswordLineEdit->addAction(
        QIcon(":/icon/icon/cacher.png"),
        QLineEdit::TrailingPosition
        );

    // Toggle both echo mode AND icon
    connect(showOldPasswordAction, &QAction::triggered, this, [=, this](){
        if(ui->oldPasswordLineEdit->echoMode() == QLineEdit::Password) {
            ui->oldPasswordLineEdit->setEchoMode(QLineEdit::Normal);
            showOldPasswordAction->setIcon(QIcon(":/icon/icon/visible.png")); // open eye clearly shown
        } else {
            ui->oldPasswordLineEdit->setEchoMode(QLineEdit::Password);
            showOldPasswordAction->setIcon(QIcon(":/icon/icon/cacher.png")); // closed eye clearly shown
        }
    });
}

ResetPasswordDialog::~ResetPasswordDialog()
{
    delete ui;
}

void ResetPasswordDialog::accept()
{
    QString username = ui->usernameLineEdit->text();
    QString oldPassword = ui->oldPasswordLineEdit->text();
    QString newPassword = ui->newPasswordLineEdit->text();

    // Check  if user exists and old password is correct
    QSqlQuery query;
    query.prepare("SELECT password_hash FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if(query.exec() && query.next()) {
        QString stored_hash = query.value(0).toString();

        if(QtBCrypt::hashPassword(oldPassword, stored_hash) == stored_hash) {
            // Old password correct, hash new password
            QString newSalt = QtBCrypt::generateSalt();
            QString newHashedPassword = QtBCrypt::hashPassword(newPassword, newSalt);

            // Update database clearly with new hashed password
            QSqlQuery updateQuery;
            updateQuery.prepare("UPDATE users SET password_hash = :newHash WHERE username = :username");
            updateQuery.bindValue(":newHash", newHashedPassword);
            updateQuery.bindValue(":username", username);

            if(updateQuery.exec()) {
                QMessageBox::information(this, "Success", "Password has been changed successfully.");
                QDialog::accept();  // clearly close dialog with success
            } else {
                QMessageBox::critical(this, "Error", "Failed to update password in database.");
            }
        } else {
            QMessageBox::warning(this, "Error", "Old password is incorrect.");
        }
    } else {
        QMessageBox::warning(this, "Error", "Username not found.");
    }
}
