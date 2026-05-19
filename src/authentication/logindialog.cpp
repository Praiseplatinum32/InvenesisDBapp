#include "logindialog.h"
#include "ui_logindialog.h"

#include <QMessageBox>
#include <QPushButton>
#include <QDebug>
#include <QSettings>


#include "common/ClickableLabel.h"
#include "resetpassworddialog.h"
#include "../data_access/AuthenticationDao.h"
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
        QIcon(":/icons/resources/icons/cacher.png"),
        QLineEdit::TrailingPosition
        );

    // Toggle both echo mode AND icon
    connect(showPasswordAction, &QAction::triggered, this, [=](){
        if(ui->passwordLineEdit->echoMode() == QLineEdit::Password) {
            ui->passwordLineEdit->setEchoMode(QLineEdit::Normal);
            showPasswordAction->setIcon(QIcon(":/icons/resources/icons/visible.png")); // open eye clearly shown
        } else {
            ui->passwordLineEdit->setEchoMode(QLineEdit::Password);
            showPasswordAction->setIcon(QIcon(":/icons/resources/icons/cacher.png")); // closed eye clearly shown
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
    connect(resetPasswordLabel, &ClickableLabel::clicked, this, [=](){
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



    AuthenticationDao dao;
    QString err;
    if (dao.authenticateUser(username, password, &userRole, &err)) {
        emit loginSuccessful(userRole);  // Emit the role upon success
        // ✅ Save username for next login
        saveLastUsername(username);
        accept();  // Closes the dialog successfully
    } else {
        if (err == "Incorrect password." || err == "User not found.") {
            QMessageBox::warning(this, "Login Error", "Incorrect username or password.");
        } else {
            QMessageBox::critical(this, "Database Error", QString("Query failed: %1").arg(err));
        }
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
