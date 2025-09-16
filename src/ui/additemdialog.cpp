#include "additemdialog.h"
#include "ui_additemdialog.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QClipboard>
#include <QMessageBox>
#include <QHeaderView>
#include <QShortcut>
#include <qsqlerror>

AddItemDialog::AddItemDialog(const QString &tableName, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddItemDialog),
    currentTable(tableName)
{
    ui->setupUi(this);
    setupPages();

    connect(ui->nextButton, &QPushButton::clicked, this, &AddItemDialog::nextPage);
    connect(ui->prevButton, &QPushButton::clicked, this, &AddItemDialog::prevPage);
    connect(ui->submitButton, &QPushButton::clicked, this, &AddItemDialog::submitData);
    connect(ui->pasteButton, &QPushButton::clicked, this, &AddItemDialog::pasteFromClipboard);

}

AddItemDialog::~AddItemDialog()
{
    delete ui;
}

void AddItemDialog::setupPages()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);

    // Fetch column names AND check if they are auto-incremented
    query.prepare(R"(
        SELECT column_name, column_default, data_type
        FROM information_schema.columns
        WHERE table_name = :table
        ORDER BY ordinal_position
    )");
    query.bindValue(":table", currentTable);

    if (!query.exec()) {
        QMessageBox::critical(this, "Database Error", "Failed to retrieve table columns.");
        return;
    }

    qDebug() << "Fetching columns for table:" << currentTable;

    while (query.next()) {
        QString columnName = query.value(0).toString();
        QString columnDefault = query.value(1).toString();
        QString dataType = query.value(2).toString();

        qDebug() << "Column:" << columnName << " | Default:" << columnDefault << " | Type:" << dataType;

        // Skip auto-incremented columns (they use "nextval()" in PostgreSQL)
        if (columnDefault.contains("nextval(")) {
            qDebug() << "Skipping auto-incremented column:" << columnName;
            continue;
        }

        columnNames.append(columnName);

        // Create a new page for the column
        QWidget *page = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *columnLabel = new QLabel("Enter values for '" + columnName + "':", this);

        // Create Dynamic QLabel for Expected Data Type
        QLabel *infoLabel = new QLabel(this);
        infoLabel->setStyleSheet("font-style: italic; color: gray;"); // Styling for visibility
        infoLabel->setText("Expected type: " + dataType);

        // Create the input table for bulk entry
        QTableWidget *tableWidget = new QTableWidget(60, 1);
        tableWidget->setEditTriggers(QAbstractItemView::AllEditTriggers);
        tableWidget->setHorizontalHeaderLabels(QStringList() << columnName);
        tableWidget->setSizeAdjustPolicy(QTableWidget::AdjustToContents);
        tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

        // Enable "Next" button when data is entered
        connect(tableWidget, &QTableWidget::cellChanged, this, [=, this]() {
            bool hasData = false;
            for (int row = 0; row < tableWidget->rowCount(); ++row) {
                QTableWidgetItem *item = tableWidget->item(row, 0);
                if (item && !item->text().trimmed().isEmpty()) {
                    hasData = true;
                    break;
                }
            }
            ui->nextButton->setEnabled(hasData);
        });

        layout->addWidget(columnLabel);
        layout->addWidget(infoLabel);
        layout->addWidget(tableWidget);
        if (page->layout()) {
            delete page->layout();
        }
        page->setLayout(layout);

        ui->stackedWidget->addWidget(page);
        columnTables.append(tableWidget);

        // Clear button functionality
        connect(ui->clearButton, &QPushButton::clicked, this, [=]() {
            tableWidget->clearContents();  // Clears all data in the table but keeps headers
        });

        qDebug() << "Added page for column:" << columnName;
    }

    qDebug() << "Total pages created:" << ui->stackedWidget->count();

    if (columnTables.isEmpty()) {
        QMessageBox::critical(this, "Error", "No columns found for this table.");
        reject();
    }

    // Start the wizard on page 3
    if (ui->stackedWidget->count() > 2) {
        ui->stackedWidget->setCurrentIndex(2);
    }

    // Initialize navigation buttons
    ui->prevButton->setEnabled(false);
    ui->nextButton->setEnabled(false);  // Next is disabled until data is entered
    ui->submitButton->setEnabled(false);

    // Ensure Ctrl+V works across ALL pages
    QShortcut *pasteShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_V), this);
    connect(pasteShortcut, &QShortcut::activated, this, &AddItemDialog::pasteFromClipboard);

    qDebug() << "SetupPages execution complete!";

    // qDebug() << "Total pages in stackedWidget:" << ui->stackedWidget->count();
    // for (int i = 0; i < ui->stackedWidget->count(); ++i) {
    //     qDebug() << "Page " << i << " : " << ui->stackedWidget->widget(i);
    // }

    // ui->stackedWidget->setCurrentIndex(ui->stackedWidget->count() - 1);

}



void AddItemDialog::nextPage()
{
    int currentIndex = ui->stackedWidget->currentIndex();
    ui->nextButton->setEnabled(true);
    if (currentIndex < columnTables.size() - 1) {
        ui->stackedWidget->setCurrentIndex(currentIndex + 1);
        ui->prevButton->setEnabled(true);
    }

    // Disable Next on last page & Enable Submit
    if (currentIndex + 1 == columnTables.size() - 1) {
        ui->nextButton->setEnabled(true);
        ui->submitButton->setEnabled(true);
    }


}

void AddItemDialog::prevPage()
{
    int currentIndex = ui->stackedWidget->currentIndex();
    if (currentIndex > 0) {
        ui->stackedWidget->setCurrentIndex(currentIndex - 1);
        ui->nextButton->setEnabled(true);
    }
    if (currentIndex - 1 == 0) {
        ui->prevButton->setEnabled(false);
    }
}

void AddItemDialog::pasteFromClipboard()
{
    QString clipboardText = QApplication::clipboard()->text();
    QStringList rows = clipboardText.split("\n", Qt::SkipEmptyParts);

    //Fetch the correct table widget based on the CURRENT page
    QWidget *currentPage = ui->stackedWidget->currentWidget(); // Get active page
    QTableWidget *currentTableWidget = nullptr;

    foreach (QTableWidget *table, columnTables) {
        if (table->parentWidget() == currentPage) { //Ensure we're on the correct page
            currentTableWidget = table;
            break;
        }
    }

    if (!currentTableWidget) return; //Prevent crashes if something goes wrong

    //Find the selected row to start pasting
    int startRow = currentTableWidget->currentRow();
    if (startRow < 0) startRow = 0;

    for (int i = 0; i < rows.size() && (startRow + i) < currentTableWidget->rowCount(); ++i) {
        QStringList columns = rows[i].split("\t");

        //Ensure we only paste in the first column
        if (!columns.isEmpty()) {
            QTableWidgetItem *newItem = new QTableWidgetItem(columns[0].trimmed());
            currentTableWidget->setItem(startRow + i, 0, newItem);
        }
    }
}



void AddItemDialog::submitData()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);

    int rowCount = columnTables.first()->rowCount();

    // ✅ Retrieve default values for each column
    QMap<QString, QString> defaultValuesMap;
    QSqlQuery defaultQuery(db);
    defaultQuery.prepare(QString("SELECT column_name, column_default FROM information_schema.columns WHERE table_name = :table"));
    defaultQuery.bindValue(":table", currentTable);

    if (defaultQuery.exec()) {
        while (defaultQuery.next()) {
            QString columnName = defaultQuery.value(0).toString();
            QString columnDefault = defaultQuery.value(1).toString();
            defaultValuesMap[columnName] = columnDefault;
        }
    } else {
        qDebug() << "Failed to retrieve default values:" << defaultQuery.lastError().text();
    }

    for (int row = 0; row < rowCount; ++row) {
        QStringList values;
        for (int col = 0; col < columnTables.size(); ++col) {
            QTableWidgetItem *item = columnTables[col]->item(row, 0);
            values.append(item ? item->text().trimmed() : "NULL");
        }

        // ✅ Skip empty rows
        bool isRowEmpty = true;
        for (const QString &val : values) {
            if (val != "NULL" && !val.isEmpty()) {
                isRowEmpty = false;
                break;
            }
        }
        if (isRowEmpty) {
            qDebug() << "Skipping empty row:" << row;
            continue;
        }

        // ✅ Skip auto-incremented columns
        QStringList filteredColumns;
        QStringList filteredValues;
        QStringList finalValues;

        for (int i = 0; i < columnNames.size(); ++i) {
            if (defaultValuesMap[columnNames[i]].startsWith("nextval")) {
                continue;  // Skip auto-incremented column
            }
            filteredColumns.append(columnNames[i]);
            filteredValues.append("?");

            // ✅ Convert numbers from "3,16" to "3.16"
            QString sanitizedValue = values[i].replace(",", ".");
            finalValues.append(sanitizedValue == "NULL" ? QVariant().toString() : sanitizedValue);
        }

        // ✅ Debugging output
        qDebug() << "Inserting into table:" << currentTable;
        qDebug() << "Columns:" << filteredColumns;
        qDebug() << "Values for row" << row << ":" << finalValues;

        // ✅ Generate correct SQL statement
        QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                          .arg(currentTable)
                          .arg(filteredColumns.join(", "))
                          .arg(filteredValues.join(", "));

        qDebug() << "Generated SQL:" << sql;

        query.prepare(sql);

        for (int i = 0; i < finalValues.size(); ++i) {
            query.bindValue(i, finalValues[i] == "NULL" ? QVariant() : finalValues[i]);
        }

        if (!query.exec()) {
            qDebug() << "SQL Error:" << query.lastError().text();
            QMessageBox::critical(this, "Database Error", "Failed to insert row.");
        }
    }

    emit dataInserted();  // ✅ Notify MainWindow to update the table
    QMessageBox::information(this, "Success", "Items added successfully!");
    accept();  // ✅ Close dialog after successful insertion
}