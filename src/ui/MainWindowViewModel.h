#ifndef MAINWINDOWVIEWMODEL_H
#define MAINWINDOWVIEWMODEL_H

#include <QObject>
#include <QSqlTableModel>
#include <QStringList>
#include <memory>
#include "customproxymodel.h"

class MainWindowViewModel : public QObject {
    Q_OBJECT
public:
    explicit MainWindowViewModel(QObject* parent = nullptr);
    ~MainWindowViewModel() override;

    void setUserRole(const QString& role);
    QString getUserRole() const;
    bool isAdmin() const;

    QStringList getVisibleTables() const;
    
    void loadTable(const QString& tableName);
    QSqlTableModel* getTableModel() const;
    CustomProxyModel* getProxyModel() const;
    
    void refreshTable();
    void autoRefreshIfNeeded(bool hasSelection);
    
    bool exportCsv(const QString& filePath, const QList<int>& selectedRows, QString* errOut);

signals:
    void roleChanged();

private:
    QString m_userRole;
    std::unique_ptr<QSqlTableModel> m_tableModel;
    CustomProxyModel* m_proxyModel;
};

#endif // MAINWINDOWVIEWMODEL_H
