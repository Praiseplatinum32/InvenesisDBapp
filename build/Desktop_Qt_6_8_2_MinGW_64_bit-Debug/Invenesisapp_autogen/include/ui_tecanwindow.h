/********************************************************************************
** Form generated from reading UI file 'tecanwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TECANWINDOW_H
#define UI_TECANWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <draggabletableview.h>

QT_BEGIN_NAMESPACE

class Ui_TecanWindow
{
public:
    QAction *actionSave;
    QAction *actionLoad;
    QAction *actionGenerate_GWL;
    QAction *actionCreate_Plate_Map;
    QWidget *centralwidget;
    QSplitter *splitter_3;
    QSplitter *splitter;
    QTableView *testRequestTableView;
    DraggableTableView *compoundQueryTableView;
    QSplitter *splitter_2;
    QScrollArea *plateDisplayScrollArea;
    QWidget *scrollAreaWidgetContents;
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout;
    QPushButton *clearPlatesButton;
    QScrollArea *daughterPlateScrollArea;
    QWidget *scrollAreaWidgetContents_2;
    QMenuBar *menubar;
    QMenu *menuActions;
    QStatusBar *statusbar;
    QToolBar *toolBar;

    void setupUi(QMainWindow *TecanWindow)
    {
        if (TecanWindow->objectName().isEmpty())
            TecanWindow->setObjectName("TecanWindow");
        TecanWindow->resize(928, 500);
        actionSave = new QAction(TecanWindow);
        actionSave->setObjectName("actionSave");
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icon/icon/sauvegarder.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        actionSave->setIcon(icon);
        actionLoad = new QAction(TecanWindow);
        actionLoad->setObjectName("actionLoad");
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icon/icon/dossier.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        actionLoad->setIcon(icon1);
        actionGenerate_GWL = new QAction(TecanWindow);
        actionGenerate_GWL->setObjectName("actionGenerate_GWL");
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icon/icon/telechargement-de-fichiers.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        actionGenerate_GWL->setIcon(icon2);
        actionCreate_Plate_Map = new QAction(TecanWindow);
        actionCreate_Plate_Map->setObjectName("actionCreate_Plate_Map");
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icon/icon/platemap.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        actionCreate_Plate_Map->setIcon(icon3);
        actionCreate_Plate_Map->setMenuRole(QAction::MenuRole::NoRole);
        centralwidget = new QWidget(TecanWindow);
        centralwidget->setObjectName("centralwidget");
        splitter_3 = new QSplitter(centralwidget);
        splitter_3->setObjectName("splitter_3");
        splitter_3->setGeometry(QRect(10, 10, 851, 401));
        splitter_3->setOrientation(Qt::Orientation::Vertical);
        splitter = new QSplitter(splitter_3);
        splitter->setObjectName("splitter");
        splitter->setOrientation(Qt::Orientation::Horizontal);
        testRequestTableView = new QTableView(splitter);
        testRequestTableView->setObjectName("testRequestTableView");
        splitter->addWidget(testRequestTableView);
        compoundQueryTableView = new DraggableTableView(splitter);
        compoundQueryTableView->setObjectName("compoundQueryTableView");
        splitter->addWidget(compoundQueryTableView);
        splitter_3->addWidget(splitter);
        splitter_2 = new QSplitter(splitter_3);
        splitter_2->setObjectName("splitter_2");
        splitter_2->setOrientation(Qt::Orientation::Horizontal);
        plateDisplayScrollArea = new QScrollArea(splitter_2);
        plateDisplayScrollArea->setObjectName("plateDisplayScrollArea");
        plateDisplayScrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 68, 136));
        plateDisplayScrollArea->setWidget(scrollAreaWidgetContents);
        splitter_2->addWidget(plateDisplayScrollArea);
        layoutWidget = new QWidget(splitter_2);
        layoutWidget->setObjectName("layoutWidget");
        verticalLayout = new QVBoxLayout(layoutWidget);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        clearPlatesButton = new QPushButton(layoutWidget);
        clearPlatesButton->setObjectName("clearPlatesButton");
        QIcon icon4;
        icon4.addFile(QString::fromUtf8("icon/effacer.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        clearPlatesButton->setIcon(icon4);

        verticalLayout->addWidget(clearPlatesButton);

        daughterPlateScrollArea = new QScrollArea(layoutWidget);
        daughterPlateScrollArea->setObjectName("daughterPlateScrollArea");
        daughterPlateScrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents_2 = new QWidget();
        scrollAreaWidgetContents_2->setObjectName("scrollAreaWidgetContents_2");
        scrollAreaWidgetContents_2->setGeometry(QRect(0, 0, 773, 104));
        daughterPlateScrollArea->setWidget(scrollAreaWidgetContents_2);

        verticalLayout->addWidget(daughterPlateScrollArea);

        splitter_2->addWidget(layoutWidget);
        splitter_3->addWidget(splitter_2);
        TecanWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(TecanWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 928, 21));
        menuActions = new QMenu(menubar);
        menuActions->setObjectName("menuActions");
        TecanWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(TecanWindow);
        statusbar->setObjectName("statusbar");
        TecanWindow->setStatusBar(statusbar);
        toolBar = new QToolBar(TecanWindow);
        toolBar->setObjectName("toolBar");
        TecanWindow->addToolBar(Qt::ToolBarArea::TopToolBarArea, toolBar);

        menubar->addAction(menuActions->menuAction());
        menuActions->addAction(actionSave);
        menuActions->addAction(actionLoad);
        menuActions->addAction(actionGenerate_GWL);
        toolBar->addAction(actionSave);
        toolBar->addAction(actionLoad);
        toolBar->addAction(actionGenerate_GWL);
        toolBar->addAction(actionCreate_Plate_Map);

        retranslateUi(TecanWindow);

        QMetaObject::connectSlotsByName(TecanWindow);
    } // setupUi

    void retranslateUi(QMainWindow *TecanWindow)
    {
        TecanWindow->setWindowTitle(QCoreApplication::translate("TecanWindow", "MainWindow", nullptr));
        actionSave->setText(QCoreApplication::translate("TecanWindow", "Save", nullptr));
        actionLoad->setText(QCoreApplication::translate("TecanWindow", "Load", nullptr));
        actionGenerate_GWL->setText(QCoreApplication::translate("TecanWindow", "Generate GWL", nullptr));
        actionCreate_Plate_Map->setText(QCoreApplication::translate("TecanWindow", "Create Plate Map", nullptr));
        clearPlatesButton->setText(QCoreApplication::translate("TecanWindow", "Clear Daughter Plates", nullptr));
        menuActions->setTitle(QCoreApplication::translate("TecanWindow", "Actions", nullptr));
        toolBar->setWindowTitle(QCoreApplication::translate("TecanWindow", "toolBar", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TecanWindow: public Ui_TecanWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TECANWINDOW_H
