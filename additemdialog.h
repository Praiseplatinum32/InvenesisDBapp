#ifndef ADDITEMDIALOG_H
#define ADDITEMDIALOG_H

#include <QDialog>
#include <QSqlTableModel>
#include <QTableWidget>

namespace Ui {
class AddItemDialog;
}

class AddItemDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddItemDialog(const QString &tableName, QWidget *parent = nullptr);
    ~AddItemDialog();

signals:
    void dataInserted(); // Signal to refresh main window table

private slots:
    void nextPage();
    void prevPage();
    void submitData();
    void pasteFromClipboard();

private:
    Ui::AddItemDialog *ui;
    QString currentTable;
    QStringList columnNames;
    QList<QTableWidget *> columnTables;

    void setupPages();
    void insertIntoDatabase();
};

#endif // ADDITEMDIALOG_H
