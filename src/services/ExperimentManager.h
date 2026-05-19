#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <memory>
#include "data_access/ExperimentDao.h"

class QStandardItemModel;

class ExperimentManager : public QObject {
    Q_OBJECT
public:
    explicit ExperimentManager(QObject* parent = nullptr);
    ~ExperimentManager() = default;

    // Database access exposed through Manager
    QList<QVariantMap> fetchTestRequests(const QStringList& requestIds, QString* errOut = nullptr) const;
    QList<QVariantMap> fetchSolutionsForCompounds(const QSet<QString>& compoundNames, QString* errOut = nullptr) const;
    QList<QVariantMap> fetchSolutionsByIds(const QList<int>& solutionIds, QString* errOut = nullptr) const;
    bool markTestRequestsDone(const QStringList& requestIds, QString* errOut = nullptr) const;

    // Plate Calculations
    QList<QMap<QString, QStringList>> calculateInitialDaughterPlates(int dilutionSteps, const QStringList& compoundList, const QString& testType, bool is384) const;
    
    struct TestPlatesResult {
        QList<QMap<QString, QStringList>> testPlates;
        QMap<QString, QStringList> qcPlate;
    };
    TestPlatesResult calculateTestPlates(int dilutionSteps, const QString& testType, const QList<QMap<QString, QStringList>>& daughterPlates, const QJsonObject& qcPlatesJson, const QString& qcType) const;

    // JSON Manipulation & Persistence
    bool saveExperiment(const QString& expCode, const QString& username, const QJsonObject& stdObj, QJsonObject& currentJson, QString* errOut = nullptr) const;
    QJsonObject loadExperiment(int expId, QString* expCodeOut = nullptr, QString* errOut = nullptr) const;
    void addConcentrationsToExperimentJson(QJsonObject& root, const QJsonObject& qcPlatesJson) const;

    // Helpers
    QStandardItemModel* createTestRequestModel(const QList<QVariantMap>& data, QObject* parent = nullptr) const;
    QStandardItemModel* createCompoundModel(const QList<QVariantMap>& data, QObject* parent = nullptr) const;
    QStandardItemModel* createTestRequestModelFromJson(const QJsonArray& array, QObject* parent = nullptr) const;
    QStandardItemModel* createCompoundModelFromJson(const QJsonArray& array, QObject* parent = nullptr) const;
    
    class QSqlQueryModel* fetchExperimentListModel(QObject* parent = nullptr) const;

private:
    std::unique_ptr<ExperimentDao> m_dao;
};
