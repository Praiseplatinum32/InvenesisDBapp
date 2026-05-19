#pragma once

#include <QString>
#include <QVector>
#include <QVariant>
#include <QList>

struct ColumnMeta {
    QString name;
    QString dataType;
    QString columnDefault;   // empty if none
    bool nullable = true;
    bool autoIncrement = false;
    bool hasDefault() const { return !columnDefault.trimmed().isEmpty(); }
};

class ItemDao {
public:
    ItemDao() = default;
    ~ItemDao() = default;

    // Fetch column metadata for the table, excluding auto-incrementing fields
    QVector<ColumnMeta> getTableMetadata(const QString& tableName, QString* errOut = nullptr) const;

    // Insert rows of raw string data. The row data size must match the column size.
    bool insertRows(const QString& tableName, const QVector<ColumnMeta>& columns, const QList<QVector<QString>>& rowsData, QString* errOut = nullptr) const;

private:
    static QString quoteIdent(const QString& ident);
    static bool isNullToken(const QString& s);
};
