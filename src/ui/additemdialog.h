#pragma once

#include <QDialog>
#include <QVector>
#include <QTableWidget>
#include "../data_access/ItemDao.h"

namespace Ui { class AddItemDialog; }

class AddItemDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddItemDialog(const QString &tableName, QWidget *parent = nullptr);
    ~AddItemDialog();

signals:
    void dataInserted();

private slots:
    void nextPage();
    void prevPage();
    void pasteFromClipboard();
    void submitData();

private:
    void setupPages();
    QTableWidget* currentPageTableWidget() const;

    Ui::AddItemDialog *ui;
    QString currentTable;

    QVector<ColumnMeta> columns;
    QVector<QTableWidget*> columnTables;
};
