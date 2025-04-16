#include "logindialog.h"
#include "ui_logindialog.h"
#include <QtSql/QSqlQuery>
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>
#include <QSettings>

#include "ClickableLabel.h"
#include "resetpassworddialog.h"
#include "qtbcrypt.h"


LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle("Invenesis Database Login");

    // ✅ Load last logged-in username
    loadLastUsername();

    // ✅ Auto-focus password field
    ui->passwordLineEdit->setFocus();

    //Add "Login" button to the button box
    QPushButton* loginButton = ui->buttonBox->addButton("Login", QDialogButtonBox::AcceptRole);

    //Connect the buttonbox to the login slot
    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::loginButton_clicked);
    ui->passwordLineEdit->setEchoMode(QLineEdit::Password);

    QAction* showPasswordAction = ui->passwordLineEdit->addAction(
        QIcon(":/icon/icon/cacher.png"),
        QLineEdit::TrailingPosition
        );

    // Toggle both echo mode AND icon
    connect(showPasswordAction, &QAction::triggered, this, [=, this](){
        if(ui->passwordLineEdit->echoMode() == QLineEdit::Password) {
            ui->passwordLineEdit->setEchoMode(QLineEdit::Normal);
            showPasswordAction->setIcon(QIcon(":/icon/icon/visible.png")); // open eye clearly shown
        } else {
            ui->passwordLineEdit->setEchoMode(QLineEdit::Password);
            showPasswordAction->setIcon(QIcon(":/icon/icon/cacher.png")); // closed eye clearly shown
        }
    });

    //Create clickable label:
    ClickableLabel* resetPasswordLabel = new ClickableLabel(this);
    resetPasswordLabel->setText("Reset Password");
    resetPasswordLabel->setCursor(Qt::PointingHandCursor);
    resetPasswordLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);

    // Add it to existing form layout:
    ui->formLayout->setWidget(2, QFormLayout::FieldRole, resetPasswordLabel);

    // Connect the click event:
    connect(resetPasswordLabel, &ClickableLabel::clicked, this, [=, this](){
        ResetPasswordDialog dlg(this);
        dlg.exec();
    });
}

LoginDialog::~LoginDialog() {
    delete ui;
}

QString LoginDialog::getUserRole() const {
    return userRole;
}

void LoginDialog::loginButton_clicked() {

    QString username = ui->usernameLineEdit->text();
    QString password = ui->passwordLineEdit->text();

    QSqlQuery query;
    query.prepare("SELECT password_hash, role FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if(query.exec() && query.next()) {
        QString stored_hash = query.value(0).toString();

        if(QtBCrypt::hashPassword(password, stored_hash) == stored_hash) {
            userRole = query.value(1).toString();
            emit loginSuccessful(userRole);  // Emit the role upon success
            // ✅ Save username for next login
            saveLastUsername(username);
            accept();  // Closes the dialog successfully
        } else {
            QMessageBox::warning(this, "Login Error", "Incorrect username or password.");
        }
    } else {
        QMessageBox::warning(this, "Login Error", "User not found.");
    }
}

void LoginDialog::loadLastUsername()
{
    QSettings settings("Invenesis", "DatabaseApp");
    QString lastUser = settings.value("lastUsername", "").toString();  // ✅ Retrieve saved username

    if (!lastUser.isEmpty()) {
        ui->usernameLineEdit->setText(lastUser);
    }
}

void LoginDialog::saveLastUsername(const QString &username)
{
    QSettings settings("Invenesis", "DatabaseApp");
    settings.setValue("lastUsername", username);  // ✅ Store username in settings
}
