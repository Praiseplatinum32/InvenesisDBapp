#include "ItemDao.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>

QString ItemDao::quoteIdent(const QString& ident) {
    QString out = ident;
    out.replace("\"", "\"\"");          // escape quotes
    return "\"" + out + "\"";
}

bool ItemDao::isNullToken(const QString& s) {
    return s.trimmed().compare("NULL", Qt::CaseInsensitive) == 0;
}

QVector<ColumnMeta> ItemDao::getTableMetadata(const QString& tableName, QString* errOut) const {
    QVector<ColumnMeta> columns;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery query(db);

    query.prepare(R"(
        SELECT column_name, column_default, data_type, is_nullable
        FROM information_schema.columns
        WHERE table_name = :table
        ORDER BY ordinal_position
    )");
    query.bindValue(":table", tableName);

    if (!query.exec()) {
        if (errOut) *errOut = query.lastError().databaseText();
        return columns;
    }

    while (query.next()) {
        ColumnMeta meta;
        meta.name = query.value(0).toString();
        meta.columnDefault = query.value(1).toString();
        meta.dataType = query.value(2).toString();
        const QString isNullable = query.value(3).toString();
        meta.nullable = (isNullable.compare("YES", Qt::CaseInsensitive) == 0);
        meta.autoIncrement = meta.columnDefault.contains("nextval(", Qt::CaseInsensitive);

        if (meta.autoIncrement) {
            continue; // skip serial/identity columns
        }

        columns.push_back(meta);
    }
    return columns;
}

bool ItemDao::insertRows(const QString& tableName, const QVector<ColumnMeta>& columns, const QList<QVector<QString>>& rowsData, QString* errOut) const {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        if (errOut) *errOut = "Database connection is not open.";
        return false;
    }

    if (!db.transaction()) {
        if (errOut) *errOut = "Failed to start transaction:\n" + db.lastError().databaseText();
        return false;
    }

    for (int row = 0; row < rowsData.size(); ++row) {
        const auto& rawInputs = rowsData[row];
        
        bool anyValue = false;
        for (const QString& v : rawInputs) {
            if (!v.isEmpty()) anyValue = true;
        }
        if (!anyValue) continue;

        QStringList colSql;
        QStringList valSql;
        QVector<QVariant> binds;
        binds.reserve(columns.size());

        for (int i = 0; i < columns.size(); ++i) {
            const auto& meta = columns[i];
            QString v = rawInputs[i];

            colSql << quoteIdent(meta.name);

            // Treat explicit "NULL" token
            if (isNullToken(v)) {
                if (!meta.nullable) {
                    db.rollback();
                    if (errOut) *errOut = QString("Row %1: Column '%2' is NOT NULL but you entered NULL.").arg(row + 1).arg(meta.name);
                    return false;
                }
                valSql << "NULL";
                continue;
            }

            // Empty cell -> DEFAULT if available, else NULL if nullable, else error
            if (v.isEmpty()) {
                if (meta.hasDefault()) {
                    valSql << "DEFAULT";
                } else if (meta.nullable) {
                    valSql << "NULL";
                } else {
                    db.rollback();
                    if (errOut) *errOut = QString("Row %1: Column '%2' is required (NOT NULL) and has no default.").arg(row + 1).arg(meta.name);
                    return false;
                }
                continue;
            }

            // Sanitize numeric with comma decimal
            if (meta.dataType.contains("numeric", Qt::CaseInsensitive) ||
                meta.dataType.contains("double", Qt::CaseInsensitive) ||
                meta.dataType.contains("real", Qt::CaseInsensitive) ||
                meta.dataType.contains("integer", Qt::CaseInsensitive) ||
                meta.dataType.contains("bigint", Qt::CaseInsensitive) ||
                meta.dataType.contains("smallint", Qt::CaseInsensitive)) {
                v.replace(",", ".");
            }

            valSql << "?";
            binds.push_back(v);
        }

        const QString sql = QString("INSERT INTO %1 (%2) VALUES (%3)")
                                .arg(quoteIdent(tableName),
                                     colSql.join(", "),
                                     valSql.join(", "));

        QSqlQuery q(db);
        if (!q.prepare(sql)) {
            db.rollback();
            if (errOut) *errOut = "Prepare failed:\n" + q.lastError().databaseText() + "\n\nSQL:\n" + sql;
            return false;
        }

        for (int b = 0; b < binds.size(); ++b) {
            q.bindValue(b, binds[b]);
        }

        if (!q.exec()) {
            db.rollback();
            if (errOut) *errOut = QString("Row %1 insert failed:\n%2\n\nSQL:\n%3").arg(row + 1).arg(q.lastError().databaseText()).arg(sql);
            return false;
        }
    }

    if (!db.commit()) {
        db.rollback();
        if (errOut) *errOut = "Commit failed:\n" + db.lastError().databaseText();
        return false;
    }

    return true;
}
