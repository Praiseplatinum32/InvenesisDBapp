/********************************************************************************
** Form generated from reading UI file 'additemdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADDITEMDIALOG_H
#define UI_ADDITEMDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AddItemDialog
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *titleLabel;
    QStackedWidget *stackedWidget;
    QWidget *page;
    QWidget *page_2;
    QPushButton *pasteButton;
    QPushButton *clearButton;
    QHBoxLayout *horizontalLayout;
    QPushButton *prevButton;
    QPushButton *nextButton;
    QPushButton *submitButton;

    void setupUi(QDialog *AddItemDialog)
    {
        if (AddItemDialog->objectName().isEmpty())
            AddItemDialog->setObjectName("AddItemDialog");
        AddItemDialog->resize(546, 761);
        verticalLayout = new QVBoxLayout(AddItemDialog);
        verticalLayout->setObjectName("verticalLayout");
        titleLabel = new QLabel(AddItemDialog);
        titleLabel->setObjectName("titleLabel");

        verticalLayout->addWidget(titleLabel);

        stackedWidget = new QStackedWidget(AddItemDialog);
        stackedWidget->setObjectName("stackedWidget");
        page = new QWidget();
        page->setObjectName("page");
        stackedWidget->addWidget(page);
        page_2 = new QWidget();
        page_2->setObjectName("page_2");
        stackedWidget->addWidget(page_2);

        verticalLayout->addWidget(stackedWidget);

        pasteButton = new QPushButton(AddItemDialog);
        pasteButton->setObjectName("pasteButton");

        verticalLayout->addWidget(pasteButton);

        clearButton = new QPushButton(AddItemDialog);
        clearButton->setObjectName("clearButton");

        verticalLayout->addWidget(clearButton);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        prevButton = new QPushButton(AddItemDialog);
        prevButton->setObjectName("prevButton");

        horizontalLayout->addWidget(prevButton);

        nextButton = new QPushButton(AddItemDialog);
        nextButton->setObjectName("nextButton");

        horizontalLayout->addWidget(nextButton);

        submitButton = new QPushButton(AddItemDialog);
        submitButton->setObjectName("submitButton");

        horizontalLayout->addWidget(submitButton);


        verticalLayout->addLayout(horizontalLayout);


        retranslateUi(AddItemDialog);

        QMetaObject::connectSlotsByName(AddItemDialog);
    } // setupUi

    void retranslateUi(QDialog *AddItemDialog)
    {
        AddItemDialog->setWindowTitle(QCoreApplication::translate("AddItemDialog", "Dialog", nullptr));
        titleLabel->setText(QCoreApplication::translate("AddItemDialog", "Adding data:", nullptr));
        pasteButton->setText(QCoreApplication::translate("AddItemDialog", "Paste from Clipboard", nullptr));
        clearButton->setText(QCoreApplication::translate("AddItemDialog", "Clear", nullptr));
        prevButton->setText(QCoreApplication::translate("AddItemDialog", "Previous", nullptr));
        nextButton->setText(QCoreApplication::translate("AddItemDialog", "Next", nullptr));
        submitButton->setText(QCoreApplication::translate("AddItemDialog", "Submit", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AddItemDialog: public Ui_AddItemDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADDITEMDIALOG_H
