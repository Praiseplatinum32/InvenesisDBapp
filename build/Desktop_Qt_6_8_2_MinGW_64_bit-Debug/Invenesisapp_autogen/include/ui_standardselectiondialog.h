/********************************************************************************
** Form generated from reading UI file 'standardselectiondialog.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_STANDARDSELECTIONDIALOG_H
#define UI_STANDARDSELECTIONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_StandardSelectionDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFormLayout *formLayout;
    QComboBox *comboBox;
    QLabel *label;
    QTextEdit *textEdit;
    QSpacerItem *verticalSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *StandardSelectionDialog)
    {
        if (StandardSelectionDialog->objectName().isEmpty())
            StandardSelectionDialog->setObjectName("StandardSelectionDialog");
        StandardSelectionDialog->resize(400, 300);
        verticalLayout = new QVBoxLayout(StandardSelectionDialog);
        verticalLayout->setObjectName("verticalLayout");
        formLayout = new QFormLayout();
        formLayout->setObjectName("formLayout");
        comboBox = new QComboBox(StandardSelectionDialog);
        comboBox->setObjectName("comboBox");

        formLayout->setWidget(1, QFormLayout::SpanningRole, comboBox);

        label = new QLabel(StandardSelectionDialog);
        label->setObjectName("label");

        formLayout->setWidget(0, QFormLayout::SpanningRole, label);

        textEdit = new QTextEdit(StandardSelectionDialog);
        textEdit->setObjectName("textEdit");

        formLayout->setWidget(2, QFormLayout::SpanningRole, textEdit);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        formLayout->setItem(3, QFormLayout::SpanningRole, verticalSpacer);


        verticalLayout->addLayout(formLayout);

        buttonBox = new QDialogButtonBox(StandardSelectionDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(StandardSelectionDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, StandardSelectionDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, StandardSelectionDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(StandardSelectionDialog);
    } // setupUi

    void retranslateUi(QDialog *StandardSelectionDialog)
    {
        StandardSelectionDialog->setWindowTitle(QCoreApplication::translate("StandardSelectionDialog", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("StandardSelectionDialog", "Pleas select a standard for your experiment", nullptr));
    } // retranslateUi

};

namespace Ui {
    class StandardSelectionDialog: public Ui_StandardSelectionDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_STANDARDSELECTIONDIALOG_H
