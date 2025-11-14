#ifndef DATABASEVIEWWINDOW_H
#define DATABASEVIEWWINDOW_H

#include <QMainWindow>
#include <QItemSelection>
#include <QStandardItemModel>
#include <QSqlTableModel>
#include <QLineEdit>
#include <QTimer>
#include <QLabel>

#include "customproxymodel.h"



QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    std::unique_ptr<QSqlTableModel> currentTableModel;
    CustomProxyModel *proxyModel;  // ✅ Corrected proxy model type
    QString currentUserRole;  // Store the logged-in user's role
    QTimer* refreshTimer;     // Timer to refresh data periodically
    QLabel *rowCountLabel;    // Displays total row count
    QLabel *columnCountLabel; // Displays total column count
    QLabel *selectedRowCountLabel; // Displays selected row count

    void updateFilters(); // ✅ Helper function for dual filtering

private slots:
    void onTableSelected(const QItemSelection &selected, const QItemSelection &deselected);
    void on_actionAdd_triggered();
    void refreshTableView();
    void on_refreshTableButton_triggered();
    void setUserRole(const QString &role);
    void setupTreeView();
    void autoRefreshTableView();
    void updateTableStatistics();
    void on_actionexportCsvButton_triggered();
    void on_actionTecan_triggered();
    void updateFilterCriteria(); // ✅ Slot for handling dual-column filtering changes
    void on_actionUpdate_triggered();
};

#endif // DATABASEVIEWWINDOW_H
