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

#include "logindialog.h"
#include "tecanwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    showMaximized();
    setCentralWidget(ui->splitter);
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
    searchIcon->setIcon(QIcon(":/icon/icon/chercher.png"));  // ✅ Set the icon

    // ✅ Add the icon inside searchLineEdit
    ui->searchLineEdit->addAction(searchIcon, QLineEdit::LeadingPosition);

    // ✅ Create a Clear Button (❌) inside searchLineEdit
    QAction *clearAction = new QAction(this);
    clearAction->setIcon(QIcon(":/icon/icon/effacer.png"));

    // ✅ Add clear button inside search bar
    ui->searchLineEdit->addAction(clearAction, QLineEdit::TrailingPosition);
    connect(clearAction, &QAction::triggered, ui->searchLineEdit, &QLineEdit::clear);

    // ✅ Create search icons for second filter
    QAction *searchIcon2 = new QAction(this);
    searchIcon2->setIcon(QIcon(":/icon/icon/chercher.png"));
    ui->searchLineEdit_2->addAction(searchIcon2, QLineEdit::LeadingPosition);

    // ✅ Create a Clear Button (❌) inside searchLineEdit_2
    QAction *clearAction2 = new QAction(this);
    clearAction2->setIcon(QIcon(":/icon/icon/effacer.png"));
    ui->searchLineEdit_2->addAction(clearAction2, QLineEdit::TrailingPosition);
    connect(clearAction2, &QAction::triggered, ui->searchLineEdit_2, &QLineEdit::clear);

    // Initialize CustomProxyModel for dual-column filtering
    proxyModel = new CustomProxyModel(this);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    ui->dataTableView->setModel(proxyModel);

    // Connect searchLineEdit with corresponding combo box:
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, [=, this](const QString &text){
        int column = ui->columnComboBox->currentData().toInt();
        proxyModel->setFilter1(text, column);
    });

    // Connect columnComboBox to update filtering immediately:
    connect(ui->columnComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=, this](int index) {
                QString currentText = ui->searchLineEdit->text();
                int column = ui->columnComboBox->itemData(index).toInt();
                proxyModel->setFilter1(currentText, column);
            });

    // Connect searchLineEdit_2 with corresponding combo box_2:
    connect(ui->searchLineEdit_2, &QLineEdit::textChanged, this, [=, this](const QString &text) {
        int column = ui->columnComboBox_2->currentData().toInt();
        proxyModel->setFilter2(text, column);
    });

    // Connect columnComboBox_2 with corresponding search line edit:
    connect(ui->columnComboBox_2, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=, this](int index) {
                QString currentText = ui->searchLineEdit_2->text();
                int column = ui->columnComboBox_2->itemData(index).toInt();
                proxyModel->setFilter2(currentText, column);
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

    if (loginDialog.exec() != QDialog::Accepted) {
        qApp->quit();
        return;
    }

    // Set up tree view based on user role
    setupTreeView();

    // Timer for automatic data refresh
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::autoRefreshTableView);
    refreshTimer->start(5000);

    // Ensure statistics update on row selection
    connect(ui->dataTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateTableStatistics);
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

    currentTableModel = std::make_unique<QSqlTableModel>(this);
    currentTableModel->setTable(tableName);
    currentTableModel->select();

    // Setup proxy model
    proxyModel->setSourceModel(currentTableModel.get());
    ui->dataTableView->setModel(proxyModel);
    ui->dataTableView->setSelectionModel(new QItemSelectionModel(proxyModel));

    // Populate first combo box
    ui->columnComboBox->clear();
    ui->columnComboBox->addItem("All Columns", -1);
    ui->columnComboBox_2->clear();
    ui->columnComboBox_2->addItem("All Columns", -1);

    for (int i = 0; i < currentTableModel->columnCount(); ++i) {
        QString columnName = currentTableModel->headerData(i, Qt::Horizontal).toString();
        ui->columnComboBox->addItem(columnName, i);
        ui->columnComboBox_2->addItem(columnName, i);
    }

    ui->dataTableView->resizeColumnsToContents();

    // Scroll to last row
    int lastRow = currentTableModel->rowCount() - 1;
    if (lastRow >= 0) {
        QModelIndex lastIndex = proxyModel->index(lastRow, 0);
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
    if (!currentTableModel) return;

    currentTableModel->select();  //Refresh the table model
    ui->dataTableView->setModel(currentTableModel.get());
    ui->dataTableView->resizeColumnsToContents();

    //Ensure the last column stretches to fill available space
    ui->dataTableView->horizontalHeader()->setStretchLastSection(true);


    //Scroll to the last row
    int lastRow = currentTableModel->rowCount() - 1;
    if (lastRow >= 0) {
        QModelIndex lastIndex = currentTableModel->index(lastRow, 0);
        ui->dataTableView->scrollTo(lastIndex, QAbstractItemView::PositionAtBottom);
    }
    updateTableStatistics();
}

void MainWindow::autoRefreshTableView()
{
    if (!currentTableModel) return;

    QItemSelectionModel *selectionModel = ui->dataTableView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedRows();  // ✅ Get selected rows

    if (!selectedIndexes.isEmpty()) {
        //qDebug() << "[Auto-Refresh] Skipped: Rows are selected.";
        return;  // ✅ Skip refresh if there are selected rows
    }

    int previousRowCount = currentTableModel->rowCount();
    currentTableModel->select();  // ✅ Refresh table model
    int newRowCount = currentTableModel->rowCount();

    if (previousRowCount != newRowCount) {
        ui->dataTableView->setModel(currentTableModel.get());
        ui->dataTableView->resizeColumnsToContents();
        ui->dataTableView->horizontalHeader()->setStretchLastSection(true);

        if (newRowCount > 0) {
            QModelIndex lastIndex = currentTableModel->index(newRowCount - 1, 0);
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
    currentUserRole = role;
    qDebug() << "User logged in as:" << currentUserRole;

    // ✅ Rebuild the tree view to apply role-based restrictions
    setupTreeView();
}

void MainWindow::setupTreeView()
{
    QStringList allTables = QSqlDatabase::database().tables();
    QStringList filteredTables;

    // ✅ Apply Role-Based Table Visibility
    if (currentUserRole == "admin") {
        filteredTables = allTables;
    }
    else if (currentUserRole == "userplus") {
        filteredTables = allTables;
        filteredTables.removeOne("users");  //Hide "users" table
    }
    else if (currentUserRole == "user") {
        filteredTables = {"test_requests", "bottles", "solutions"};  // ✅ Show only test_requests, bottles & solutions
    }

    // ✅ Create a new model for the tree
    QStandardItemModel* tableModel = new QStandardItemModel(this);
    tableModel->setHorizontalHeaderLabels(QStringList() << "Tables");

    QStandardItem* rootItem = tableModel->invisibleRootItem();
    //tableModel->appendRow(rootItem);

    foreach (const QString& tableName, filteredTables) {
        QStandardItem* item = new QStandardItem(QIcon(":/icon/icon/table.png"), tableName);
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
    if (!currentTableModel) {
        rowCountLabel->setText("Rows: 0");
        columnCountLabel->setText("Columns: 0");
        selectedRowCountLabel->setText("Selected Rows: 0");
        return;
    }

    int rowCount = currentTableModel->rowCount();
    int columnCount = currentTableModel->columnCount();
    int selectedRows = ui->dataTableView->selectionModel()->selectedRows().count();

    rowCountLabel->setText(QString("Rows: %1").arg(rowCount));
    columnCountLabel->setText(QString("Columns: %1").arg(columnCount));
    selectedRowCountLabel->setText(QString("Selected Rows: %1").arg(selectedRows));
}

void MainWindow::on_actionexportCsvButton_triggered()
{
    if (!currentTableModel) {
        QMessageBox::warning(this, "Export Error", "No table loaded to export.");
        return;
    }

    // ✅ Open file dialog with "Documents" folder as default
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getSaveFileName(this, "Save CSV", defaultPath + "/export.csv", "CSV Files (*.csv)");

    if (filePath.isEmpty()) return;  // User canceled

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export Error", "Failed to open file for writing.");
        return;
    }

    QTextStream stream(&file);

    // ✅ Get column headers
    QStringList headers;
    for (int col = 0; col < currentTableModel->columnCount(); ++col) {
        headers << currentTableModel->headerData(col, Qt::Horizontal).toString();
    }
    stream << headers.join(",") << "\n";  // ✅ Write headers to CSV

    // ✅ Get selected rows and map from proxy model to source model
    QItemSelectionModel *selectionModel = ui->dataTableView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedRows();

    qDebug() << "[DEBUG] Selected rows in proxy model:" << selectedIndexes;

    QList<int> selectedRows;
    foreach(const QModelIndex &proxyIndex, selectedIndexes) {
        int sourceRow = proxyModel->mapToSource(proxyIndex).row();  // ✅ Convert to source model row
        selectedRows.append(sourceRow);
    }

    qDebug() << "Exporting selected rows:" << selectedRows;  // ✅ Debugging

    // ✅ Export rows (either selected or full table)
    int rowCount = currentTableModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        // ✅ If selection exists, only export selected rows
        if (!selectedRows.isEmpty() && !selectedRows.contains(row)) continue;

        QStringList rowValues;
        for (int col = 0; col < currentTableModel->columnCount(); ++col) {
            rowValues << currentTableModel->index(row, col).data().toString();
        }
        stream << rowValues.join(",") << "\n";  // ✅ Write row to CSV
    }

    file.close();
    QMessageBox::information(this, "Export Successful", "Data exported successfully to:\n" + filePath);
}

void MainWindow::updateFilterCriteria()
{
    // Fetch the filter texts and selected columns
    QString filterText1 = ui->searchLineEdit->text();
    int column1 = ui->columnComboBox->currentData().toInt();

    QString filterText2 = ui->searchLineEdit_2->text();
    int column2 = ui->columnComboBox_2->currentData().toInt();

    // Set filters on the proxy model
    proxyModel->setFilter1(filterText1, column1);
    proxyModel->setFilter2(filterText2, column2);
}


void MainWindow::on_actionTecan_triggered()
{
    QModelIndexList selectedRows = ui->dataTableView->selectionModel()->selectedRows();

    // Create Tecan Window instance
    TecanWindow *tecanWindow = new TecanWindow(this);

    // Pass selected test request IDs (assuming first column contains request_id)
    if (!selectedRows.isEmpty()) {
        QStringList selectedRequestIDs;
        foreach(const QModelIndex &index, selectedRows) {
            QString requestId = proxyModel->data(proxyModel->index(index.row(), 0)).toString();
            selectedRequestIDs.append(requestId);
        }
        tecanWindow->loadTestRequests(selectedRequestIDs);
    }

    // Show Tecan window
    tecanWindow->show();
}


