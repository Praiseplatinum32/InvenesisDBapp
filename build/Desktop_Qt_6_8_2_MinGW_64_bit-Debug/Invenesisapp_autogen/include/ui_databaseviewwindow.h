/********************************************************************************
** Form generated from reading UI file 'databaseviewwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DATABASEVIEWWINDOW_H
#define UI_DATABASEVIEWWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionAdd;
    QAction *refreshTableButton;
    QAction *actionexportCsvButton;
    QAction *actionTecan;
    QWidget *centralwidget;
    QSplitter *splitter;
    QTreeView *tableTreeView;
    QWidget *widget;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLineEdit *searchLineEdit;
    QComboBox *columnComboBox;
    QLineEdit *searchLineEdit_2;
    QComboBox *columnComboBox_2;
    QTableView *dataTableView;
    QStatusBar *statusbar;
    QToolBar *toolBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(982, 600);
        actionAdd = new QAction(MainWindow);
        actionAdd->setObjectName("actionAdd");
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icon/icon/ajouter.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        actionAdd->setIcon(icon);
        actionAdd->setMenuRole(QAction::MenuRole::NoRole);
        refreshTableButton = new QAction(MainWindow);
        refreshTableButton->setObjectName("refreshTableButton");
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icon/icon/rafraichir.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        refreshTableButton->setIcon(icon1);
        refreshTableButton->setMenuRole(QAction::MenuRole::NoRole);
        actionexportCsvButton = new QAction(MainWindow);
        actionexportCsvButton->setObjectName("actionexportCsvButton");
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icon/icon/telecharger.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        actionexportCsvButton->setIcon(icon2);
        actionexportCsvButton->setMenuRole(QAction::MenuRole::NoRole);
        actionTecan = new QAction(MainWindow);
        actionTecan->setObjectName("actionTecan");
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icon/icon/tecan.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        actionTecan->setIcon(icon3);
        actionTecan->setMenuRole(QAction::MenuRole::NoRole);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        splitter = new QSplitter(centralwidget);
        splitter->setObjectName("splitter");
        splitter->setGeometry(QRect(170, 130, 680, 226));
        splitter->setOrientation(Qt::Orientation::Horizontal);
        tableTreeView = new QTreeView(splitter);
        tableTreeView->setObjectName("tableTreeView");
        splitter->addWidget(tableTreeView);
        widget = new QWidget(splitter);
        widget->setObjectName("widget");
        verticalLayout = new QVBoxLayout(widget);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        searchLineEdit = new QLineEdit(widget);
        searchLineEdit->setObjectName("searchLineEdit");

        horizontalLayout->addWidget(searchLineEdit);

        columnComboBox = new QComboBox(widget);
        columnComboBox->setObjectName("columnComboBox");

        horizontalLayout->addWidget(columnComboBox);

        searchLineEdit_2 = new QLineEdit(widget);
        searchLineEdit_2->setObjectName("searchLineEdit_2");

        horizontalLayout->addWidget(searchLineEdit_2);

        columnComboBox_2 = new QComboBox(widget);
        columnComboBox_2->setObjectName("columnComboBox_2");

        horizontalLayout->addWidget(columnComboBox_2);


        verticalLayout->addLayout(horizontalLayout);

        dataTableView = new QTableView(widget);
        dataTableView->setObjectName("dataTableView");

        verticalLayout->addWidget(dataTableView);

        splitter->addWidget(widget);
        MainWindow->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);
        toolBar = new QToolBar(MainWindow);
        toolBar->setObjectName("toolBar");
        MainWindow->addToolBar(Qt::ToolBarArea::TopToolBarArea, toolBar);

        toolBar->addAction(actionAdd);
        toolBar->addAction(refreshTableButton);
        toolBar->addAction(actionexportCsvButton);
        toolBar->addAction(actionTecan);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        actionAdd->setText(QCoreApplication::translate("MainWindow", "Add", nullptr));
#if QT_CONFIG(tooltip)
        actionAdd->setToolTip(QCoreApplication::translate("MainWindow", "Add data to table", nullptr));
#endif // QT_CONFIG(tooltip)
        refreshTableButton->setText(QCoreApplication::translate("MainWindow", "refreshTableButton", nullptr));
#if QT_CONFIG(tooltip)
        refreshTableButton->setToolTip(QCoreApplication::translate("MainWindow", "Refresh Table", nullptr));
#endif // QT_CONFIG(tooltip)
        actionexportCsvButton->setText(QCoreApplication::translate("MainWindow", "exportCsvButton", nullptr));
#if QT_CONFIG(tooltip)
        actionexportCsvButton->setToolTip(QCoreApplication::translate("MainWindow", "Export to CSV", nullptr));
#endif // QT_CONFIG(tooltip)
        actionTecan->setText(QCoreApplication::translate("MainWindow", "Tecan", nullptr));
#if QT_CONFIG(tooltip)
        actionTecan->setToolTip(QCoreApplication::translate("MainWindow", "Open tecan view", nullptr));
#endif // QT_CONFIG(tooltip)
        searchLineEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "Search...", nullptr));
        columnComboBox->setCurrentText(QString());
        columnComboBox->setPlaceholderText(QCoreApplication::translate("MainWindow", "All Columns", nullptr));
        searchLineEdit_2->setPlaceholderText(QCoreApplication::translate("MainWindow", "Search...", nullptr));
        columnComboBox_2->setCurrentText(QString());
        columnComboBox_2->setPlaceholderText(QCoreApplication::translate("MainWindow", "All Columns", nullptr));
        toolBar->setWindowTitle(QCoreApplication::translate("MainWindow", "toolBar", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DATABASEVIEWWINDOW_H
