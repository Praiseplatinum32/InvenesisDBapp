#ifndef RESETPASSWORDDIALOG_H
#define RESETPASSWORDDIALOG_H

#include <QDialog>

namespace Ui {
class ResetPasswordDialog;
}

class ResetPasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResetPasswordDialog(QWidget *parent = nullptr);
    ~ResetPasswordDialog();

private slots:
    void accept() override;  // clearly override accept()

private:
    Ui::ResetPasswordDialog *ui;
};

#endif // RESETPASSWORDDIALOG_H
