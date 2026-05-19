#include "customproxymodel.h"
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

CustomProxyModel::CustomProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent), filterColumn1(-1), filterColumn2(-1)
{}

void CustomProxyModel::setFilter1(const QString &text, int column)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    beginFilterChange();
#endif
    filterText1 = text;
    filterColumn1 = column;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif
}

void CustomProxyModel::setFilter2(const QString &text, int column)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    beginFilterChange();
#endif
    filterText2 = text;
    filterColumn2 = column;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif
}

void CustomProxyModel::setHideDone(bool hide)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    beginFilterChange();
#endif
    hideDone = hide;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif
}

void CustomProxyModel::setDoneColumn(int column)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    beginFilterChange();
#endif
    doneColumn = column;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    endFilterChange();
#else
    invalidateFilter();
#endif
}

bool CustomProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // Debug logging to Documents folder
    static bool firstLog = true;
    if (firstLog && sourceRow == 0) {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/temp_qt_debug.txt";
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << "\n--- New Filter Run ---\n";
            out << "Filter1: '" << filterText1 << "', Col1: " << filterColumn1 << "\n";
            out << "Filter2: '" << filterText2 << "', Col2: " << filterColumn2 << "\n";
            out << "ColumnCount: " << sourceModel()->columnCount(sourceParent) << "\n";
        }
    }
    if (sourceRow == sourceModel()->rowCount(sourceParent) - 1) {
        firstLog = true; // reset at the end of the batch
    } else {
        firstLog = false;
    }

    // ✅ Check Filter 1
    if (!filterText1.isEmpty()) {
        bool match1 = false;
        if (filterColumn1 >= 0) {
            QModelIndex index1 = sourceModel()->index(sourceRow, filterColumn1, sourceParent);
            if (sourceModel()->data(index1).toString().contains(filterText1, Qt::CaseInsensitive)) {
                match1 = true;
            }
        } else {
            int colCount = sourceModel()->columnCount(sourceParent);
            for (int col = 0; col < colCount; ++col) {
                QModelIndex index1 = sourceModel()->index(sourceRow, col, sourceParent);
                if (sourceModel()->data(index1).toString().contains(filterText1, Qt::CaseInsensitive)) {
                    match1 = true;
                    break;
                }
            }
        }
        if (!match1) return false;
    }

    // ✅ Check Filter 2
    if (!filterText2.isEmpty()) {
        bool match2 = false;
        if (filterColumn2 >= 0) {
            QModelIndex index2 = sourceModel()->index(sourceRow, filterColumn2, sourceParent);
            if (sourceModel()->data(index2).toString().contains(filterText2, Qt::CaseInsensitive)) {
                match2 = true;
            }
        } else {
            int colCount = sourceModel()->columnCount(sourceParent);
            for (int col = 0; col < colCount; ++col) {
                QModelIndex index2 = sourceModel()->index(sourceRow, col, sourceParent);
                if (sourceModel()->data(index2).toString().contains(filterText2, Qt::CaseInsensitive)) {
                    match2 = true;
                    break;
                }
            }
        }
        if (!match2) return false;
    }

    // ✅ Hide rows where done == true
    if (hideDone && doneColumn >= 0) {
        QModelIndex doneIndex = sourceModel()->index(sourceRow, doneColumn, sourceParent);
        QVariant v = sourceModel()->data(doneIndex);

        // Works whether DB returns bool or "t"/"true"/"1"
        bool isDone = false;
        if (v.metaType().id() == QMetaType::Bool) {
            isDone = v.toBool();
        } else {
            const QString s = v.toString().trimmed().toLower();
            isDone = (s == "true" || s == "t" || s == "1" || s == "yes");
        }

        if (isDone)
            return false;
    }

    return true;
}
