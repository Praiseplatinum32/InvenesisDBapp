#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    QString getUserRole() const;

signals:
    void loginSuccessful(const QString &role);

private slots:
    void loginButton_clicked();


private:
    Ui::LoginDialog *ui;
    QString userRole;
    void loadLastUsername();  // ✅ Loads the last username from settings
    void saveLastUsername(const QString &username);  // ✅ Saves the last username
};

#endif // LOGINDIALOG_H
