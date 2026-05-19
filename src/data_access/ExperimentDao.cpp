#include "ExperimentDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QDebug>
#include <QSqlQueryModel>

QList<QVariantMap> ExperimentDao::fetchTestRequests(const QStringList& requestIds, QString* errOut) const {
    QList<QVariantMap> results;
    if (requestIds.isEmpty()) return results;

    const QString placeholders = '\'' + requestIds.join("','") + '\'';
    const QString queryStr = QStringLiteral("SELECT * FROM test_requests WHERE request_id IN (%1)").arg(placeholders);

    QSqlQuery query;
    if (!query.exec(queryStr)) {
        if (errOut) *errOut = query.lastError().text();
        return results;
    }

    while (query.next()) {
        QVariantMap row;
        QSqlRecord rec = query.record();
        for (int i = 0; i < rec.count(); ++i) {
            row[rec.fieldName(i)] = rec.value(i);
        }
        results.append(row);
    }
    return results;
}

QList<QVariantMap> ExperimentDao::fetchSolutionsForCompound(const QString& compoundName, QString* errOut) const {
    QList<QVariantMap> results;
    QSqlQuery query;
    query.prepare(R"(
            SELECT solution_id, product_name, invenesis_solution_id, weight, weight_unit,
                   concentration, concentration_unit, container_id, well_id, matrix_tube_id
            FROM   solutions
            WHERE  product_name = :compound)");
    query.bindValue(":compound", compoundName);

    if (!query.exec()) {
        if (errOut) *errOut = query.lastError().text();
        return results;
    }

    while (query.next()) {
        QVariantMap sol;
        QSqlRecord rec = query.record();
        for (int i = 0; i < rec.count(); ++i) {
            sol[rec.fieldName(i)] = rec.value(i);
        }
        results.append(sol);
    }
    return results;
}

QList<QVariantMap> ExperimentDao::fetchSolutionsByIds(const QList<int>& solutionIds, QString* errOut) const {
    QList<QVariantMap> results;
    if (solutionIds.isEmpty()) return results;

    QStringList idPlaceholders;
    for (int id : solutionIds) {
        idPlaceholders << QString::number(id);
    }

    const QString queryStr = QStringLiteral(R"(
        SELECT product_name, invenesis_solution_id, weight, weight_unit,
               concentration, concentration_unit, container_id, well_id,
               matrix_tube_id
        FROM   solutions
        WHERE  solution_id IN (%1))").arg(idPlaceholders.join(','));

    QSqlQuery query;
    if (!query.exec(queryStr)) {
        if (errOut) *errOut = query.lastError().text();
        return results;
    }

    while (query.next()) {
        QVariantMap sol;
        QSqlRecord rec = query.record();
        for (int i = 0; i < rec.count(); ++i) {
            sol[rec.fieldName(i)] = rec.value(i);
        }
        results.append(sol);
    }
    return results;
}

bool ExperimentDao::saveExperiment(const QString& expCode, const QString& projectCode, const QJsonObject& data, const QStringList& requestIds, QString* errOut) const {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid() || !db.isOpen()) {
        if (errOut) *errOut = "Database is not open.";
        return false;
    }

    if (!db.transaction()) {
        if (errOut) *errOut = "Failed to start DB transaction.";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO experiments (experiment_code, project_code, date_created, data)
        VALUES (:code, :project, NOW(), :data)
        ON CONFLICT (experiment_code) DO UPDATE
          SET date_created = NOW(), data = EXCLUDED.data
        RETURNING experiment_id)");
    q.bindValue(":code", expCode);
    q.bindValue(":project", projectCode);
    q.bindValue(":data", QString(QJsonDocument(data).toJson(QJsonDocument::Compact)));

    if (!q.exec() || !q.next()) {
        db.rollback();
        if (errOut) *errOut = q.lastError().text();
        return false;
    }
    const int expId = q.value(0).toInt();

    for (const QString& reqId : requestIds) {
        QSqlQuery link(db);
        link.prepare(R"(INSERT INTO experiment_requests (experiment_id, request_id)
                            VALUES (:eid, :rid)
                            ON CONFLICT DO NOTHING)");
        link.bindValue(":eid", expId);
        link.bindValue(":rid", reqId);
        if (!link.exec()) {
            qWarning() << "Failed to link request" << reqId << ":" << link.lastError();
        }
    }

    if (!db.commit()) {
        db.rollback();
        if (errOut) *errOut = "Failed to commit DB transaction.";
        return false;
    }

    return true;
}

QJsonObject ExperimentDao::loadExperiment(int expId, QString* expCodeOut, QString* errOut) const {
    QSqlQuery q;
    q.prepare("SELECT experiment_code, project_code, data FROM experiments WHERE experiment_id = :id");
    q.bindValue(":id", expId);

    if (!q.exec() || !q.next()) {
        if (errOut) *errOut = q.lastError().text();
        return QJsonObject();
    }

    if (expCodeOut) *expCodeOut = q.value("experiment_code").toString();
    const QString jsonData = q.value("data").toString();
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    
    if (doc.isNull() || !doc.isObject()) {
        if (errOut) *errOut = "Invalid JSON format in experiment.";
        return QJsonObject();
    }

    return doc.object();
}

bool ExperimentDao::markTestRequestsDone(const QStringList& requestIds, QString* errOut) const {
    if (requestIds.isEmpty()) return true;

    QStringList ph;
    ph.reserve(requestIds.size());
    for (int i = 0; i < requestIds.size(); ++i) {
        ph << QString(":id%1").arg(i);
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid() || !db.isOpen()) {
        if (errOut) *errOut = "Database is not open.";
        return false;
    }

    if (!db.transaction()) {
        if (errOut) *errOut = "Failed to start DB transaction.";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE test_requests SET done = TRUE WHERE request_id IN (%1)").arg(ph.join(',')));

    for (int i = 0; i < requestIds.size(); ++i) {
        q.bindValue(ph[i], requestIds[i]);
    }

    if (!q.exec()) {
        db.rollback();
        if (errOut) *errOut = q.lastError().text();
        return false;
    }

    if (!db.commit()) {
        db.rollback();
        if (errOut) *errOut = "Failed to commit DB transaction.";
        return false;
    }

    return true;
}

QSqlQueryModel* ExperimentDao::fetchExperimentListModel(QObject* parent) const {
    QSqlQueryModel* model = new QSqlQueryModel(parent);
    model->setQuery(R"(
        SELECT experiment_id, experiment_code, project_code, date_created, user
        FROM experiments
        ORDER BY date_created DESC
    )");
    return model;
}
