#include "MainWindowViewModel.h"
#include <QSqlDatabase>
#include <QFile>
#include <QTextStream>
#include <QDebug>

MainWindowViewModel::MainWindowViewModel(QObject* parent)
    : QObject(parent),
      m_proxyModel(new CustomProxyModel(this))
{
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

MainWindowViewModel::~MainWindowViewModel() = default;

void MainWindowViewModel::setUserRole(const QString& role) {
    m_userRole = role;
    emit roleChanged();
}

QString MainWindowViewModel::getUserRole() const {
    return m_userRole;
}

bool MainWindowViewModel::isAdmin() const {
    return m_userRole == "admin";
}

QStringList MainWindowViewModel::getVisibleTables() const {
    QStringList allTables = QSqlDatabase::database().tables();
    QStringList filteredTables;

    if (m_userRole == "admin") {
        filteredTables = allTables;
    } else if (m_userRole == "userplus") {
        filteredTables = allTables;
        filteredTables.removeOne("users");
    } else if (m_userRole == "user") {
        filteredTables = {"test_requests", "bottles", "solutions"};
    }

    return filteredTables;
}

void MainWindowViewModel::loadTable(const QString& tableName) {
    m_tableModel = std::make_unique<QSqlTableModel>(this);
    m_tableModel->setTable(tableName);
    m_tableModel->select();

    m_proxyModel->setSourceModel(m_tableModel.get());
    
    if (tableName == "test_requests") {
        int doneCol = m_tableModel->fieldIndex("done");
        m_proxyModel->setDoneColumn(doneCol);
    } else {
        m_proxyModel->setDoneColumn(-1);
        m_proxyModel->setHideDone(false);
    }
}

QSqlTableModel* MainWindowViewModel::getTableModel() const {
    return m_tableModel.get();
}

CustomProxyModel* MainWindowViewModel::getProxyModel() const {
    return m_proxyModel;
}

void MainWindowViewModel::refreshTable() {
    if (m_tableModel) {
        m_tableModel->select();
    }
}

void MainWindowViewModel::autoRefreshIfNeeded(bool hasSelection) {
    if (!m_tableModel) return;
    if (hasSelection) return;

    int previousRowCount = m_tableModel->rowCount();
    m_tableModel->select();
    // Re-applying proxy is handled externally if row count changes
}

bool MainWindowViewModel::exportCsv(const QString& filePath, const QList<int>& selectedRows, QString* errOut) {
    if (!m_tableModel) {
        if (errOut) *errOut = "No table loaded to export.";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errOut) *errOut = "Failed to open file for writing.";
        return false;
    }

    QTextStream stream(&file);

    QStringList headers;
    for (int col = 0; col < m_tableModel->columnCount(); ++col) {
        headers << m_tableModel->headerData(col, Qt::Horizontal).toString();
    }
    stream << headers.join(",") << "\n";

    int rowCount = m_tableModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        if (!selectedRows.isEmpty() && !selectedRows.contains(row)) continue;

        QStringList rowValues;
        for (int col = 0; col < m_tableModel->columnCount(); ++col) {
            rowValues << m_tableModel->index(row, col).data().toString();
        }
        stream << rowValues.join(",") << "\n";
    }

    file.close();
    return true;
}
