#ifndef ADMINRESETPASSWORDDIALOG_H
#define ADMINRESETPASSWORDDIALOG_H

#include <QDialog>

namespace Ui {
class AdminResetPasswordDialog;  // matches class name in .ui
}

class AdminResetPasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdminResetPasswordDialog(QWidget *parent = nullptr);
    ~AdminResetPasswordDialog() override;

public slots:
    void accept() override;   // handle OK button

private:
    Ui::AdminResetPasswordDialog *ui;

    void setupPasswordToggleIcons();
};

#endif // ADMINRESETPASSWORDDIALOG_H
