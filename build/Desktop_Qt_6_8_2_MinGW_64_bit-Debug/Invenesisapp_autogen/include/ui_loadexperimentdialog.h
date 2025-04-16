/********************************************************************************
** Form generated from reading UI file 'loadexperimentdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOADEXPERIMENTDIALOG_H
#define UI_LOADEXPERIMENTDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_LoadExperimentDialog
{
public:
    QVBoxLayout *verticalLayout;
    QTableView *experimentTableView;
    QCheckBox *readOnlyCheckBox;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *LoadExperimentDialog)
    {
        if (LoadExperimentDialog->objectName().isEmpty())
            LoadExperimentDialog->setObjectName("LoadExperimentDialog");
        LoadExperimentDialog->resize(364, 464);
        verticalLayout = new QVBoxLayout(LoadExperimentDialog);
        verticalLayout->setObjectName("verticalLayout");
        experimentTableView = new QTableView(LoadExperimentDialog);
        experimentTableView->setObjectName("experimentTableView");

        verticalLayout->addWidget(experimentTableView);

        readOnlyCheckBox = new QCheckBox(LoadExperimentDialog);
        readOnlyCheckBox->setObjectName("readOnlyCheckBox");
        readOnlyCheckBox->setTristate(false);

        verticalLayout->addWidget(readOnlyCheckBox);

        buttonBox = new QDialogButtonBox(LoadExperimentDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(LoadExperimentDialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, LoadExperimentDialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, LoadExperimentDialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(LoadExperimentDialog);
    } // setupUi

    void retranslateUi(QDialog *LoadExperimentDialog)
    {
        LoadExperimentDialog->setWindowTitle(QCoreApplication::translate("LoadExperimentDialog", "Dialog", nullptr));
        readOnlyCheckBox->setText(QCoreApplication::translate("LoadExperimentDialog", "Open in read-only mode", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoadExperimentDialog: public Ui_LoadExperimentDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOADEXPERIMENTDIALOG_H
