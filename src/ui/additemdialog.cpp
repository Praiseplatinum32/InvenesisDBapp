#include "additemdialog.h"
#include "ui_additemdialog.h"


#include <QClipboard>
#include <QMessageBox>
#include <QHeaderView>
#include <QShortcut>
#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>

#include "../data_access/ItemDao.h"

AddItemDialog::AddItemDialog(const QString &tableName, QWidget *parent)
    : QDialog(parent),
    ui(new Ui::AddItemDialog),
    currentTable(tableName)
{
    ui->setupUi(this);
    setupPages();

    connect(ui->nextButton, &QPushButton::clicked, this, &AddItemDialog::nextPage);
    connect(ui->prevButton, &QPushButton::clicked, this, &AddItemDialog::prevPage);
    connect(ui->submitButton, &QPushButton::clicked, this, &AddItemDialog::submitData);
    connect(ui->pasteButton, &QPushButton::clicked, this, &AddItemDialog::pasteFromClipboard);

    // Clear only current page (avoid N connections in loop)
    connect(ui->clearButton, &QPushButton::clicked, this, [this]() {
        if (auto* tw = currentPageTableWidget()) {
            tw->clearContents();
        }
    });

    // Ctrl+V works everywhere
    auto *pasteShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_V), this);
    connect(pasteShortcut, &QShortcut::activated, this, &AddItemDialog::pasteFromClipboard);
}

AddItemDialog::~AddItemDialog()
{
    delete ui;
}

void AddItemDialog::setupPages()
{
    ItemDao dao;
    QString err;
    columns = dao.getTableMetadata(currentTable, &err);
    if (!err.isEmpty()) {
        QMessageBox::critical(this, "Database Error", "Failed to retrieve table columns:\n" + err);
        reject();
        return;
    }

    for (const auto& meta : columns) {

        QWidget *page = new QWidget(this);
        auto *layout = new QVBoxLayout(page);

        auto *columnLabel = new QLabel("Enter values for '" + meta.name + "':", page);

        auto *infoLabel = new QLabel(page);
        infoLabel->setStyleSheet("font-style: italic; color: gray;");
        infoLabel->setText(QString("Expected type: %1 | Nullable: %2 | Default: %3")
                               .arg(meta.dataType,
                                    meta.nullable ? "YES" : "NO",
                                    meta.columnDefault.isEmpty() ? "-" : meta.columnDefault));

        auto *tableWidget = new QTableWidget(60, 1, page);
        tableWidget->setHorizontalHeaderLabels(QStringList() << meta.name);
        tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

        layout->addWidget(columnLabel);
        layout->addWidget(infoLabel);
        layout->addWidget(tableWidget);

        page->setLayout(layout);
        ui->stackedWidget->addWidget(page);
        columnTables.push_back(tableWidget);
    }

    if (columnTables.isEmpty()) {
        QMessageBox::critical(this, "Error", "No editable columns found for this table.");
        reject();
        return;
    }

    // Start on first dynamically created page
    ui->stackedWidget->setCurrentWidget(columnTables.first()->parentWidget());

    ui->prevButton->setEnabled(false);
    ui->nextButton->setEnabled(true);
    ui->submitButton->setEnabled(true);
}

QTableWidget* AddItemDialog::currentPageTableWidget() const
{
    QWidget *currentPage = ui->stackedWidget->currentWidget();
    for (auto* tw : columnTables) {
        if (tw->parentWidget() == currentPage)
            return tw;
    }
    return nullptr;
}

void AddItemDialog::pasteFromClipboard()
{
    QString clipboardText = QApplication::clipboard()->text();
    QStringList rows = clipboardText.split("\n", Qt::SkipEmptyParts);

    QTableWidget *tw = currentPageTableWidget();
    if (!tw) return;

    int startRow = tw->currentRow();
    if (startRow < 0) startRow = 0;

    for (int i = 0; i < rows.size() && (startRow + i) < tw->rowCount(); ++i) {
        QStringList cols = rows[i].split("\t");
        if (!cols.isEmpty()) {
            tw->setItem(startRow + i, 0, new QTableWidgetItem(cols[0].trimmed()));
        }
    }
}

void AddItemDialog::nextPage()
{
    int idx = ui->stackedWidget->currentIndex();
    if (idx < ui->stackedWidget->count() - 1) {
        ui->stackedWidget->setCurrentIndex(idx + 1);
        ui->prevButton->setEnabled(true);
    }
    if (ui->stackedWidget->currentIndex() == ui->stackedWidget->count() - 1) {
        ui->nextButton->setEnabled(false);
    }
}

void AddItemDialog::prevPage()
{
    int idx = ui->stackedWidget->currentIndex();
    if (idx > 0) {
        ui->stackedWidget->setCurrentIndex(idx - 1);
        ui->nextButton->setEnabled(true);
    }
    if (ui->stackedWidget->currentIndex() == 0) {
        ui->prevButton->setEnabled(false);
    }
}

void AddItemDialog::submitData()
{
    const int rowCount = columnTables.first()->rowCount();
    QList<QVector<QString>> rowsData;

    for (int row = 0; row < rowCount; ++row) {
        QVector<QString> rawInputs;
        rawInputs.reserve(columns.size());

        for (int c = 0; c < columnTables.size(); ++c) {
            auto *item = columnTables[c]->item(row, 0);
            rawInputs.push_back(item ? item->text().trimmed() : QString());
        }
        rowsData.push_back(rawInputs);
    }

    ItemDao dao;
    QString err;
    if (!dao.insertRows(currentTable, columns, rowsData, &err)) {
        QMessageBox::critical(this, "Database Error", err);
        return;
    }

    emit dataInserted();
    QMessageBox::information(this, "Success", "Items added successfully!");
    accept();
}
