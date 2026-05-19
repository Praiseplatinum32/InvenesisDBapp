#include "databaseviewwindow.h"
#include "./ui_databaseviewwindow.h"
#include "additemdialog.h"

#include <QSqlDatabase>
#include <QSqlQueryModel>
#include <QSqlTableModel>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QAction>
#include <QFileDialog>
#include <QTextStream>
#include <QStandardPaths>
#include <QProcess>
#include <QDesktopServices>
#include <QCheckBox>

#include "logindialog.h"
#include "adminresetpassworddialog.h"
#include "tecanwindow.h"
#include "UpdateChecker.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , viewModel(std::make_unique<MainWindowViewModel>(this))
{
    ui->setupUi(this);

    //Admin-only action: hidden/disabled by default
    if (ui->actionAdminResetPassword) {
        ui->actionAdminResetPassword->setVisible(false);
        ui->actionAdminResetPassword->setEnabled(false);
    }

    showMaximized();
    
    mainStackedWidget = new QStackedWidget(this);
    
    QWidget* databasePage = new QWidget(this);
    QVBoxLayout* dbLayout = new QVBoxLayout(databasePage);
    dbLayout->setContentsMargins(0, 0, 0, 0);
    dbLayout->addWidget(ui->splitter);
    
    tecanView = new TecanWindow(this);
    connect(tecanView, &TecanWindow::backRequested, this, [this](){
        mainStackedWidget->setCurrentIndex(0);
        ui->toolBar->show();
    });
    
    mainStackedWidget->addWidget(databasePage);
    mainStackedWidget->addWidget(tecanView);
    
    setCentralWidget(mainStackedWidget);
    setWindowTitle("Invenesis Database Manager");

    // Set initial splitter proportions
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 10);

    // Set explicit initial size for splitter
    QList<int> sizes;
    sizes << 150 << width() - 150;
    ui->splitter->setSizes(sizes);

    // Set fixed minimum width for QTreeView
    ui->tableTreeView->setMinimumWidth(150);

    // ✅ Create a QAction for the search icon
    QAction *searchIcon = new QAction(this);
    searchIcon->setIcon(QIcon(":/icons/resources/icons/chercher.png"));  // ✅ Set the icon

    // ✅ Add the icon inside searchLineEdit
    ui->searchLineEdit->addAction(searchIcon, QLineEdit::LeadingPosition);

    // ✅ Create a Clear Button (❌) inside searchLineEdit
    QAction *clearAction = new QAction(this);
    clearAction->setIcon(QIcon(":/icons/resources/icons/effacer.png"));

    // ✅ Add clear button inside search bar
    ui->searchLineEdit->addAction(clearAction, QLineEdit::TrailingPosition);
    connect(clearAction, &QAction::triggered, ui->searchLineEdit, &QLineEdit::clear);

    // ✅ Create search icons for second filter
    QAction *searchIcon2 = new QAction(this);
    searchIcon2->setIcon(QIcon(":/icons/resources/icons/chercher.png"));
    ui->searchLineEdit_2->addAction(searchIcon2, QLineEdit::LeadingPosition);

    // ✅ Create a Clear Button (❌) inside searchLineEdit_2
    QAction *clearAction2 = new QAction(this);
    clearAction2->setIcon(QIcon(":/icons/resources/icons/effacer.png"));
    ui->searchLineEdit_2->addAction(clearAction2, QLineEdit::TrailingPosition);
    connect(clearAction2, &QAction::triggered, ui->searchLineEdit_2, &QLineEdit::clear);

    // ✅  setup the hide done checkbox
    hideDoneCheckBox = new QCheckBox("Hide done", this);
    hideDoneCheckBox->setVisible(false);                  // only for test_requests
    ui->statusbar->addPermanentWidget(hideDoneCheckBox);  // right side of status bar
    connect(hideDoneCheckBox, &QCheckBox::toggled, this, [this](bool checked){
        viewModel->getProxyModel()->setHideDone(checked);
    });

    ui->dataTableView->setModel(viewModel->getProxyModel());

    // Connect searchLineEdit with corresponding combo box:
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, [=](const QString &text){
        int column = (ui->columnComboBox->currentIndex() <= 0) ? -1 : ui->columnComboBox->currentData().toInt();
        viewModel->getProxyModel()->setFilter1(text, column);
    });

    // Connect columnComboBox to update filtering immediately:
    connect(ui->columnComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index) {
                QString currentText = ui->searchLineEdit->text();
                int column = (index <= 0) ? -1 : ui->columnComboBox->itemData(index).toInt();
                viewModel->getProxyModel()->setFilter1(currentText, column);
            });

    // Connect searchLineEdit_2 with corresponding combo box_2:
    connect(ui->searchLineEdit_2, &QLineEdit::textChanged, this, [=](const QString &text) {
        int column = (ui->columnComboBox_2->currentIndex() <= 0) ? -1 : ui->columnComboBox_2->currentData().toInt();
        viewModel->getProxyModel()->setFilter2(text, column);
    });

    // Connect columnComboBox_2 with corresponding search line edit:
    connect(ui->columnComboBox_2, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int index) {
                QString currentText = ui->searchLineEdit_2->text();
                int column = (index <= 0) ? -1 : ui->columnComboBox_2->itemData(index).toInt();
                viewModel->getProxyModel()->setFilter2(currentText, column);
            });


    // Initialize status bar labels
    rowCountLabel = new QLabel(this);
    columnCountLabel = new QLabel(this);
    selectedRowCountLabel = new QLabel(this);

    ui->statusbar->addWidget(rowCountLabel);
    ui->statusbar->addWidget(columnCountLabel);
    ui->statusbar->addWidget(selectedRowCountLabel);

    // Open Login Dialog
    LoginDialog loginDialog;
    connect(&loginDialog, &LoginDialog::loginSuccessful, this, &MainWindow::setUserRole);
    connect(viewModel.get(), &MainWindowViewModel::roleChanged, this, &MainWindow::setupTreeView);

    if (loginDialog.exec() != QDialog::Accepted) {
        qApp->quit();
        return;
    }

    // Set up tree view based on user role

    // Timer for automatic data refresh
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::autoRefreshTableView);
    constexpr int kAutoRefreshMs = 60000 * 5; // 1 min * 5
    refreshTimer->start(kAutoRefreshMs);

    // Ensure statistics update on row selection
    connect(ui->dataTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateTableStatistics);


    auto* checker = new UpdateChecker(this);
    connect(checker, &UpdateChecker::updateAvailable, this,
            [this](const QString& ver, const QString& notes, const QUrl& url){
                if (ver.isEmpty()) {
                    QMessageBox::information(this, "Updates", "You're up to date.");
                    return;
                }
                const auto ret = QMessageBox::information(
                    this, tr("Update Available"),
                    tr("Version %1 is available.\n\n%2\n\nUpdate now?").arg(ver, notes),
                    QMessageBox::Yes | QMessageBox::No);
                if (ret == QMessageBox::Yes) {
                    // Preferred (IFW): launch MaintenanceTool in updater mode if present
                    QProcess::startDetached(QCoreApplication::applicationDirPath()+"/MaintenanceTool.exe", {"--updater"});
                }
            });
    // Silent check on startup
    checker->checkNow(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onTableSelected(const QItemSelection &selected, const QItemSelection &)
{
    if (selected.indexes().isEmpty()) return;

    QModelIndex index = selected.indexes().first();
    QString tableName = index.data().toString();

    qDebug() << "Switching to table:" << tableName;


    viewModel->loadTable(tableName);

    // Setup proxy model
    ui->dataTableView->setModel(viewModel->getProxyModel());
    ui->dataTableView->setSelectionModel(new QItemSelectionModel(viewModel->getProxyModel()));

    if (tableName == "test_requests") {
        hideDoneCheckBox->setVisible(true);
        viewModel->getProxyModel()->setHideDone(hideDoneCheckBox->isChecked());
    } else {
        hideDoneCheckBox->setVisible(false);
    }

    // Populate first combo box
    ui->columnComboBox->clear();
    ui->columnComboBox->addItem("All Columns", -1);
    ui->columnComboBox_2->clear();
    ui->columnComboBox_2->addItem("All Columns", -1);

    for (int i = 0; i < viewModel->getTableModel()->columnCount(); ++i) {
        QString columnName = viewModel->getTableModel()->headerData(i, Qt::Horizontal).toString();
        ui->columnComboBox->addItem(columnName, i);
        ui->columnComboBox_2->addItem(columnName, i);
    }

    ui->dataTableView->resizeColumnsToContents();

    // Scroll to last row
    int lastRow = viewModel->getTableModel()->rowCount() - 1;
    if (lastRow >= 0) {
        QModelIndex lastIndex = viewModel->getProxyModel()->index(lastRow, 0);
        ui->dataTableView->scrollTo(lastIndex, QAbstractItemView::PositionAtBottom);
    }

    connect(ui->dataTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateTableStatistics);

    updateTableStatistics();
}



void MainWindow::on_actionAdd_triggered()
{
    QModelIndex selectedIndex = ui->tableTreeView->currentIndex();
    if (!selectedIndex.isValid()) {
        QMessageBox::warning(this, "Error", "No table selected!");
        return;
    }

    QString selectedTable = selectedIndex.data().toString();
    AddItemDialog addItemDialog(selectedTable, this);
    connect(&addItemDialog, &AddItemDialog::dataInserted, this, &MainWindow::refreshTableView);
    addItemDialog.exec();
}

void MainWindow::refreshTableView()
{
    if (!viewModel->getTableModel()) return;

    viewModel->refreshTable();
    ui->dataTableView->setModel(viewModel->getProxyModel());

    ui->dataTableView->resizeColumnsToContents();

    //Ensure the last column stretches to fill available space
    ui->dataTableView->horizontalHeader()->setStretchLastSection(true);

    //Scroll to the last row
    int lastRow = viewModel->getTableModel()->rowCount() - 1;
    if (lastRow >= 0) {
        QModelIndex lastIndex = viewModel->getTableModel()->index(lastRow, 0);
        ui->dataTableView->scrollTo(lastIndex, QAbstractItemView::PositionAtBottom);
    }
    updateTableStatistics();
}

void MainWindow::autoRefreshTableView()
{
    if (!viewModel->getTableModel()) return;

    QItemSelectionModel *selectionModel = ui->dataTableView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedRows();  // ✅ Get selected rows

    if (!selectedIndexes.isEmpty()) {
        return;  // ✅ Skip refresh if there are selected rows
    }

    int previousRowCount = viewModel->getTableModel()->rowCount();
    viewModel->refreshTable();
    int newRowCount = viewModel->getTableModel()->rowCount();

    if (previousRowCount != newRowCount) {
        ui->dataTableView->setModel(viewModel->getProxyModel());

        ui->dataTableView->resizeColumnsToContents();
        ui->dataTableView->horizontalHeader()->setStretchLastSection(true);

        if (newRowCount > 0) {
            QModelIndex lastIndex = viewModel->getTableModel()->index(newRowCount - 1, 0);
            ui->dataTableView->scrollTo(lastIndex, QAbstractItemView::PositionAtBottom);
        }
    }
}


void MainWindow::on_refreshTableButton_triggered()
{
    refreshTableView();
}

void MainWindow::setUserRole(const QString &role)
{
    viewModel->setUserRole(role);
    qDebug() << "User logged in as:" << viewModel->getUserRole();
    if (ui->actionAdminResetPassword) {
        ui->actionAdminResetPassword->setVisible(viewModel->isAdmin());
        ui->actionAdminResetPassword->setEnabled(viewModel->isAdmin());
    }
}


void MainWindow::on_actionAdminResetPassword_triggered()
{
    // Extra safety check (in case someone changes visibility logic later)
    if (!viewModel->isAdmin()) {
        QMessageBox::warning(this, "Permission denied",
                             "Only administrators can reset user passwords.");
        return;
    }

    AdminResetPasswordDialog dlg(this);

    dlg.exec();   // The dialog will handle the DB update & messages
}


void MainWindow::setupTreeView()
{
    QStringList filteredTables = viewModel->getVisibleTables();

    // ✅ Create a new model for the tree
    QStandardItemModel* tableModel = new QStandardItemModel(this);
    tableModel->setHorizontalHeaderLabels(QStringList() << "Tables");

    QStandardItem* rootItem = tableModel->invisibleRootItem();
    //tableModel->appendRow(rootItem);

    foreach (const QString& tableName, filteredTables) {
        QStandardItem* item = new QStandardItem(QIcon(":/icons/resources/icons/table.png"), tableName);
        rootItem->appendRow(item);
    }

    ui->tableTreeView->setModel(tableModel);
    ui->tableTreeView->expandAll();

    // ✅ Ensure "onTableSelected()" is properly connected to the new model
    connect(ui->tableTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onTableSelected);
}

void MainWindow::updateTableStatistics()
{
    if (!viewModel->getTableModel()) {
        rowCountLabel->setText("Rows: 0");
        columnCountLabel->setText("Columns: 0");
        selectedRowCountLabel->setText("Selected Rows: 0");
        return;
    }

    int rowCount = viewModel->getTableModel()->rowCount();
    int columnCount = viewModel->getTableModel()->columnCount();
    int selectedRows = ui->dataTableView->selectionModel()->selectedRows().count();

    rowCountLabel->setText(QString("Rows: %1").arg(rowCount));
    columnCountLabel->setText(QString("Columns: %1").arg(columnCount));
    selectedRowCountLabel->setText(QString("Selected Rows: %1").arg(selectedRows));
}

void MainWindow::on_actionexportCsvButton_triggered()
{
    // ✅ Open file dialog with "Documents" folder as default
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getSaveFileName(this, "Save CSV", defaultPath + "/export.csv", "CSV Files (*.csv)");

    if (filePath.isEmpty()) return;  // User canceled

    // Get selected rows and map from proxy model to source model
    QItemSelectionModel *selectionModel = ui->dataTableView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedRows();

    QList<int> selectedRows;
    foreach(const QModelIndex &proxyIndex, selectedIndexes) {
        int sourceRow = viewModel->getProxyModel()->mapToSource(proxyIndex).row();
        selectedRows.append(sourceRow);
    }

    QString errOut;
    if (!viewModel->exportCsv(filePath, selectedRows, &errOut)) {
        QMessageBox::critical(this, "Export Error", errOut);
        return;
    }

    QMessageBox::information(this, "Export Successful", "Data exported successfully to:\n" + filePath);
}

void MainWindow::updateFilterCriteria()
{
    // Fetch the filter texts and selected columns
    QString filterText1 = ui->searchLineEdit->text();
    int column1 = (ui->columnComboBox->currentIndex() <= 0) ? -1 : ui->columnComboBox->currentData().toInt();

    QString filterText2 = ui->searchLineEdit_2->text();
    int column2 = (ui->columnComboBox_2->currentIndex() <= 0) ? -1 : ui->columnComboBox_2->currentData().toInt();

    // Set filters on the proxy model
    viewModel->getProxyModel()->setFilter1(filterText1, column1);
    viewModel->getProxyModel()->setFilter2(filterText2, column2);
}


void MainWindow::on_actionTecan_triggered()
{
    QModelIndexList selectedRows = ui->dataTableView->selectionModel()->selectedRows();

    if (!selectedRows.isEmpty()) {
        QStringList selectedRequestIDs;
        foreach(const QModelIndex &index, selectedRows) {
            QString requestId = viewModel->getProxyModel()->data(viewModel->getProxyModel()->index(index.row(), 0)).toString();
            selectedRequestIDs.append(requestId);
        }
        tecanView->loadTestRequests(selectedRequestIDs);
    }

    ui->toolBar->hide();
    mainStackedWidget->setCurrentIndex(1);
}



void MainWindow::on_actionUpdate_triggered()
{
    qInfo() << "Button pressed";
    UpdateChecker checker;
    checker.checkNow(true);
}

