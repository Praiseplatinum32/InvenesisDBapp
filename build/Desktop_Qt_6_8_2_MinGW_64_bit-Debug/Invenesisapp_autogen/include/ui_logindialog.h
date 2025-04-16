/********************************************************************************
** Form generated from reading UI file 'logindialog.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGINDIALOG_H
#define UI_LOGINDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>

QT_BEGIN_NAMESPACE

class Ui_LoginDialog
{
public:
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *usernameLineEdit;
    QLabel *label_2;
    QLineEdit *passwordLineEdit;
    QDialogButtonBox *buttonBox;
    QSpacerItem *verticalSpacer;

    void setupUi(QDialog *LoginDialog)
    {
        if (LoginDialog->objectName().isEmpty())
            LoginDialog->setObjectName("LoginDialog");
        LoginDialog->resize(400, 161);
        LoginDialog->setCursor(QCursor(Qt::CursorShape::IBeamCursor));
        formLayout = new QFormLayout(LoginDialog);
        formLayout->setObjectName("formLayout");
        label = new QLabel(LoginDialog);
        label->setObjectName("label");

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        usernameLineEdit = new QLineEdit(LoginDialog);
        usernameLineEdit->setObjectName("usernameLineEdit");

        formLayout->setWidget(0, QFormLayout::FieldRole, usernameLineEdit);

        label_2 = new QLabel(LoginDialog);
        label_2->setObjectName("label_2");

        formLayout->setWidget(1, QFormLayout::LabelRole, label_2);

        passwordLineEdit = new QLineEdit(LoginDialog);
        passwordLineEdit->setObjectName("passwordLineEdit");
        passwordLineEdit->setEchoMode(QLineEdit::EchoMode::Password);

        formLayout->setWidget(1, QFormLayout::FieldRole, passwordLineEdit);

        buttonBox = new QDialogButtonBox(LoginDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel);

        formLayout->setWidget(3, QFormLayout::SpanningRole, buttonBox);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(2, QFormLayout::LabelRole, verticalSpacer);


        retranslateUi(LoginDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, LoginDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, LoginDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(LoginDialog);
    } // setupUi

    void retranslateUi(QDialog *LoginDialog)
    {
        LoginDialog->setWindowTitle(QCoreApplication::translate("LoginDialog", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("LoginDialog", "Username", nullptr));
        label_2->setText(QCoreApplication::translate("LoginDialog", "Password", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoginDialog: public Ui_LoginDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGINDIALOG_H
