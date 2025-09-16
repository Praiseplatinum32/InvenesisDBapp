#include "logindialog.h"
#include "ui_logindialog.h"
#include <QtSql/QSqlQuery>
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>
#include <QSettings>
#include <qsqlerror.h>

#include "common/ClickableLabel.h"
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
        QIcon(":/icons/resources/icons/cacher.png"),
        QLineEdit::TrailingPosition
        );

    // Toggle both echo mode AND icon
    connect(showPasswordAction, &QAction::triggered, this, [=, this](){
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

    // Debug: Check database connection
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        QMessageBox::critical(this, "Database Error", "Database connection is not open!");
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT password_hash, role FROM users WHERE username = :username");
    query.bindValue(":username", username);

    qDebug() << "Executing query for username:" << username;
    
    if(query.exec()) {
        qDebug() << "Query executed successfully";
        if(query.next()) {
            qDebug() << "User found in database";
            QString stored_hash = query.value(0).toString();

            if(QtBCrypt::hashPassword(password, stored_hash) == stored_hash) {
                userRole = query.value(1).toString();
                emit loginSuccessful(userRole);  // Emit the role upon success
                // ✅ Save username for next login
                saveLastUsername(username);
                accept();  // Closes the dialog successfully
            } else {
                qDebug() << "Password verification failed";
                QMessageBox::warning(this, "Login Error", "Incorrect username or password.");
            }
        } else {
            qDebug() << "No user found with username:" << username;
            QMessageBox::warning(this, "Login Error", "User not found.");
        }
    } else {
        qDebug() << "Query execution failed:" << query.lastError().text();
        QMessageBox::critical(this, "Database Error",
            QString("Query failed: %1").arg(query.lastError().text()));
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
