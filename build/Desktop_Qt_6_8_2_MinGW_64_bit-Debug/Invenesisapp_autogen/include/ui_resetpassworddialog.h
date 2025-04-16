/********************************************************************************
** Form generated from reading UI file 'resetpassworddialog.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RESETPASSWORDDIALOG_H
#define UI_RESETPASSWORDDIALOG_H

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

class Ui_ResetPasswordDialog
{
public:
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *usernameLineEdit;
    QLabel *label_2;
    QLineEdit *oldPasswordLineEdit;
    QLabel *label_3;
    QLineEdit *newPasswordLineEdit;
    QDialogButtonBox *buttonBox;
    QSpacerItem *verticalSpacer;

    void setupUi(QDialog *ResetPasswordDialog)
    {
        if (ResetPasswordDialog->objectName().isEmpty())
            ResetPasswordDialog->setObjectName("ResetPasswordDialog");
        ResetPasswordDialog->resize(400, 183);
        formLayout = new QFormLayout(ResetPasswordDialog);
        formLayout->setObjectName("formLayout");
        label = new QLabel(ResetPasswordDialog);
        label->setObjectName("label");

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        usernameLineEdit = new QLineEdit(ResetPasswordDialog);
        usernameLineEdit->setObjectName("usernameLineEdit");

        formLayout->setWidget(0, QFormLayout::FieldRole, usernameLineEdit);

        label_2 = new QLabel(ResetPasswordDialog);
        label_2->setObjectName("label_2");

        formLayout->setWidget(1, QFormLayout::LabelRole, label_2);

        oldPasswordLineEdit = new QLineEdit(ResetPasswordDialog);
        oldPasswordLineEdit->setObjectName("oldPasswordLineEdit");
        oldPasswordLineEdit->setEchoMode(QLineEdit::EchoMode::Password);

        formLayout->setWidget(1, QFormLayout::FieldRole, oldPasswordLineEdit);

        label_3 = new QLabel(ResetPasswordDialog);
        label_3->setObjectName("label_3");

        formLayout->setWidget(2, QFormLayout::LabelRole, label_3);

        newPasswordLineEdit = new QLineEdit(ResetPasswordDialog);
        newPasswordLineEdit->setObjectName("newPasswordLineEdit");
        newPasswordLineEdit->setEchoMode(QLineEdit::EchoMode::Password);

        formLayout->setWidget(2, QFormLayout::FieldRole, newPasswordLineEdit);

        buttonBox = new QDialogButtonBox(ResetPasswordDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        formLayout->setWidget(4, QFormLayout::SpanningRole, buttonBox);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(3, QFormLayout::LabelRole, verticalSpacer);


        retranslateUi(ResetPasswordDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, ResetPasswordDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, ResetPasswordDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(ResetPasswordDialog);
    } // setupUi

    void retranslateUi(QDialog *ResetPasswordDialog)
    {
        ResetPasswordDialog->setWindowTitle(QCoreApplication::translate("ResetPasswordDialog", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("ResetPasswordDialog", "Username:", nullptr));
        label_2->setText(QCoreApplication::translate("ResetPasswordDialog", "Old Password:", nullptr));
        label_3->setText(QCoreApplication::translate("ResetPasswordDialog", "New Password", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ResetPasswordDialog: public Ui_ResetPasswordDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RESETPASSWORDDIALOG_H
