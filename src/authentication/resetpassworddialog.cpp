#include "resetpassworddialog.h"
#include "ui_resetpassworddialog.h"
#include <QMessageBox>
#include "../data_access/AuthenticationDao.h"
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
    connect(showNewPasswordAction, &QAction::triggered, this, [=](){
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
    connect(showOldPasswordAction, &QAction::triggered, this, [=](){
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

    AuthenticationDao dao;
    QString err;
    if (dao.updatePassword(username, oldPassword, newPassword, &err)) {
        QMessageBox::information(this, "Success", "Password has been changed successfully.");
        QDialog::accept();  // clearly close dialog with success
    } else {
        if (err == "Old password is incorrect.") {
            QMessageBox::warning(this, "Error", err);
        } else if (err == "User not found.") {
            QMessageBox::warning(this, "Error", "Username not found.");
        } else {
            QMessageBox::critical(this, "Error", "Failed to update password in database:\n" + err);
        }
    }
}
