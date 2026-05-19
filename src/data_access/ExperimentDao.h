#pragma once

#include <QStringList>
#include <QSet>
#include <QList>
#include <QVariantMap>
#include <QJsonObject>
#include <QString>

class ExperimentDao {
public:
    ExperimentDao() = default;
    ~ExperimentDao() = default;

    QList<QVariantMap> fetchTestRequests(const QStringList& requestIds, QString* errOut = nullptr) const;
    QList<QVariantMap> fetchSolutionsForCompound(const QString& compoundName, QString* errOut = nullptr) const;
    QList<QVariantMap> fetchSolutionsByIds(const QList<int>& solutionIds, QString* errOut = nullptr) const;
    
    bool saveExperiment(const QString& expCode, const QString& projectCode, const QJsonObject& data, const QStringList& requestIds, QString* errOut = nullptr) const;
    QJsonObject loadExperiment(int expId, QString* expCodeOut = nullptr, QString* errOut = nullptr) const;
    bool markTestRequestsDone(const QStringList& requestIds, QString* errOut = nullptr) const;
    
    class QSqlQueryModel* fetchExperimentListModel(class QObject* parent = nullptr) const;
};
