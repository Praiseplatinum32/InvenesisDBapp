#include "customproxymodel.h"

CustomProxyModel::CustomProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent), filterColumn1(-1), filterColumn2(-1)
{}

void CustomProxyModel::setFilter1(const QString &text, int column)
{
    filterText1 = text;
    filterColumn1 = column;
    invalidateFilter();
}

void CustomProxyModel::setFilter2(const QString &text, int column)
{
    filterText2 = text;
    filterColumn2 = column;
    invalidateFilter();
}

// ✅ NEW
void CustomProxyModel::setHideDone(bool hide)
{
    hideDone = hide;
    invalidateFilter();
}

// ✅ NEW
void CustomProxyModel::setDoneColumn(int column)
{
    doneColumn = column;
    invalidateFilter();
}

bool CustomProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // ✅ Check Filter 1
    if (!filterText1.isEmpty() && filterColumn1 >= 0) {
        QModelIndex index1 = sourceModel()->index(sourceRow, filterColumn1, sourceParent);
        if (!sourceModel()->data(index1).toString().contains(filterText1, Qt::CaseInsensitive))
            return false;
    }

    // ✅ Check Filter 2
    if (!filterText2.isEmpty() && filterColumn2 >= 0) {
        QModelIndex index2 = sourceModel()->index(sourceRow, filterColumn2, sourceParent);
        if (!sourceModel()->data(index2).toString().contains(filterText2, Qt::CaseInsensitive))
            return false;
    }

    // ✅ NEW: Hide rows where done == true
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
