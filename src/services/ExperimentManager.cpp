#include "ExperimentManager.h"

#include <QStandardItemModel>
#include <QJsonDocument>
#include <QFile>
#include "../tecan_integration/gwlgenerator.h" // For concentration calculation dependencies

ExperimentManager::ExperimentManager(QObject* parent)
    : QObject(parent), m_dao(std::make_unique<ExperimentDao>())
{
}

QList<QVariantMap> ExperimentManager::fetchTestRequests(const QStringList& requestIds, QString* errOut) const {
    return m_dao->fetchTestRequests(requestIds, errOut);
}

QList<QVariantMap> ExperimentManager::fetchSolutionsForCompounds(const QSet<QString>& compoundNames, QString* errOut) const {
    QList<QVariantMap> results;
    for (const QString& compound : compoundNames) {
        results.append(m_dao->fetchSolutionsForCompound(compound, errOut));
    }
    return results;
}

QList<QVariantMap> ExperimentManager::fetchSolutionsByIds(const QList<int>& solutionIds, QString* errOut) const {
    return m_dao->fetchSolutionsByIds(solutionIds, errOut);
}

bool ExperimentManager::markTestRequestsDone(const QStringList& requestIds, QString* errOut) const {
    return m_dao->markTestRequestsDone(requestIds, errOut);
}

bool ExperimentManager::saveExperiment(const QString& expCode, const QString& username, const QJsonObject& stdObj, QJsonObject& currentJson, QString* errOut) const {
    currentJson["standard"] = stdObj;
    
    QString projectCode = currentJson["project_code"].toString();
    
    QStringList reqIds;
    QJsonArray trArr = currentJson["test_requests"].toArray();
    for (const QJsonValue& v : trArr) {
        reqIds << v.toObject().value("request_id").toString();
    }
    
    return m_dao->saveExperiment(expCode, projectCode, currentJson, reqIds, errOut);
}

QJsonObject ExperimentManager::loadExperiment(int expId, QString* expCodeOut, QString* errOut) const {
    return m_dao->loadExperiment(expId, expCodeOut, errOut);
}

QSqlQueryModel* ExperimentManager::fetchExperimentListModel(QObject* parent) const {
    return m_dao->fetchExperimentListModel(parent);
}

QList<QMap<QString, QStringList>> ExperimentManager::calculateInitialDaughterPlates(int dilutionSteps, const QStringList& compoundList, const QString& testType, bool is384) const {
    QList<QMap<QString, QStringList>> plates;
    
    const QStringList plateRows =
        is384 ? QStringList{"A", "B", "C", "D", "E", "F", "G", "H",
                            "I", "J", "K", "L", "M", "N", "O", "P"}  // 16 rows
              : QStringList{"A", "B", "C", "D", "E", "F", "G", "H"}; // 8 rows

    const int maxColumns = is384 ? 24 : 12;

    const bool isINV_T_031 =
        testType.contains(QLatin1String("INV-T-031"), Qt::CaseInsensitive) &&
        !is384;

    const int standardCol = isINV_T_031 ? (maxColumns - 1) : -1;
    const int dmsoCol = isINV_T_031 ? maxColumns : -1;

    QStringList standardWells, dmsoWells;

    if (isINV_T_031) {
        for (const QString &row : plateRows) {
            standardWells << row + QString::number(standardCol).rightJustified(2, '0');
            dmsoWells << row + QString::number(dmsoCol).rightJustified(2, '0');
        }
    } else {
        const QString &stdRow = plateRows.first();
        const QString &dmsoRow = plateRows.last();
        const int stdDil = qMax(dilutionSteps, 6);

        for (int c = 1; c <= stdDil && c <= maxColumns; ++c)
            standardWells << stdRow + QString::number(c).rightJustified(2, '0');

        for (int c = 1; c <= maxColumns; ++c)
            dmsoWells << dmsoRow + QString::number(c).rightJustified(2, '0');
    }

    QMap<QString, QStringList> initialPlate;
    initialPlate["Standard"] = standardWells;
    initialPlate["DMSO"] = dmsoWells;
    plates.append(initialPlate);

    int plateIdx = 0;
    int curRowIdx = 0;
    int curColIdx = 1;

    const int dmsoRowIdx = plateRows.size() - 1;
    const int stdDil = qMax(dilutionSteps, 6);

    auto startNewPlate = [&]() {
        QMap<QString, QStringList> newPlate;
        newPlate["Standard"] = standardWells;
        newPlate["DMSO"] = dmsoWells;
        plates.append(newPlate);
        ++plateIdx;
        curRowIdx = 0;
        curColIdx = 1;
    };

    auto advanceSlot = [&]() {
        ++curRowIdx;
        if (isINV_T_031) {
            if (curRowIdx >= plateRows.size()) {
                curRowIdx = 0;
                curColIdx += dilutionSteps;
            }
            if (curColIdx >= standardCol) {
                startNewPlate();
            }
        } else {
            if (curRowIdx >= dmsoRowIdx) {
                curRowIdx = 0;
                curColIdx += dilutionSteps;
            }
            if (curColIdx > maxColumns) {
                startNewPlate();
            }
        }
    };

    auto isSlotValid = [&]() -> bool {
        const int lastCol = curColIdx + dilutionSteps - 1;
        if (isINV_T_031) {
            return (lastCol < standardCol);
        } else {
            if (lastCol > maxColumns) return false;
            if (curRowIdx == dmsoRowIdx) return false;
            if (curRowIdx == 0 && curColIdx <= stdDil) return false;
            return true;
        }
    };

    for (const QString &cmpd : compoundList) {
        while (!isSlotValid()) {
            advanceSlot();
        }

        QStringList wells;
        for (int d = 0; d < dilutionSteps; ++d) {
            wells << plateRows[curRowIdx] + QString::number(curColIdx + d).rightJustified(2, '0');
        }

        plates[plateIdx][cmpd] = wells;
        advanceSlot();
    }

    return plates;
}

ExperimentManager::TestPlatesResult ExperimentManager::calculateTestPlates(int dilutionSteps, const QString& testType, const QList<QMap<QString, QStringList>>& daughterPlates, const QJsonObject& qcPlatesJson, const QString& qcType) const {
    TestPlatesResult result;
    bool is384Test = false;
    QFile vocabFile(":/data/resources/data/tests_vocabulary.json");
    if (vocabFile.open(QIODevice::ReadOnly)) {
        QJsonObject vocab = QJsonDocument::fromJson(vocabFile.readAll()).object();
        if (vocab.value(testType).toString() == "384")
            is384Test = true;
        vocabFile.close();
    }

    QStringList all96Wells;
    QStringList rows = {"A", "B", "C", "D", "E", "F", "G", "H"};
    for (const QString &r : rows) {
        for (int c = 1; c <= 12; ++c)
            all96Wells << r + QString::number(c).rightJustified(2, '0');
    }

    QJsonObject qcDef = qcPlatesJson.value(qcType).toObject();
    QStringList dmsoWells = all96Wells;
    for (const QString &key : qcDef.keys()) {
        QJsonObject rowDef = qcDef.value(key).toObject();
        QString stdName = rowDef.value("standard").toString();
        QStringList stdWells;
        for (int c = 1; c <= 12; ++c) {
            QString w = key + QString::number(c).rightJustified(2, '0');
            stdWells << w;
            dmsoWells.removeOne(w);
        }
        result.qcPlate[stdName] = stdWells;
    }
    result.qcPlate["DMSO"] = dmsoWells;

    if (is384Test) {
        QStringList rows384 = {"A", "B", "C", "D", "E", "F", "G", "H",
                               "I", "J", "K", "L", "M", "N", "O", "P"};
        for (int i = 0; i < daughterPlates.size(); ++i) {
            QMap<QString, QStringList> testMap;
            auto dp = daughterPlates[i];

            auto map96to384 = [&](const QString &w96, int quadrant) -> QString {
                int r96 = rows.indexOf(w96.at(0));
                int c96 = w96.mid(1).toInt();
                int r384 = r96 * 2 + (quadrant >= 3 ? 1 : 0);
                int c384 = (c96 - 1) * 2 + 1 + (quadrant % 2 == 0 ? 1 : 0);
                return rows384[r384] + QString::number(c384).rightJustified(2, '0');
            };

            for (const QString &cmp : dp.keys()) {
                QStringList q1, q2, q3;
                for (const QString &w : dp[cmp]) {
                    q1 << map96to384(w, 1);
                    q2 << map96to384(w, 2);
                    q3 << map96to384(w, 3);
                }
                testMap[cmp + " (Q1)"] = q1;
                testMap[cmp + " (Q2)"] = q2;
                testMap[cmp + " (Q3)"] = q3;
            }

            for (const QString &cmp : result.qcPlate.keys()) {
                QStringList q4;
                for (const QString &w : result.qcPlate[cmp])
                    q4 << map96to384(w, 4);
                testMap[cmp + " (QC)"] = q4;
            }
            result.testPlates.append(testMap);
        }
    } else {
        for (int i = 0; i < daughterPlates.size(); ++i) {
            for (int rep = 1; rep <= 3; ++rep) {
                QMap<QString, QStringList> testMap;
                for (const QString &cmp : daughterPlates[i].keys()) {
                    testMap[cmp + " (Rep " + QString::number(rep) + ")"] = daughterPlates[i][cmp];
                }
                result.testPlates.append(testMap);
            }
        }
    }

    return result;
}

QStandardItemModel* ExperimentManager::createTestRequestModel(const QList<QVariantMap>& data, QObject* parent) const {
    auto *model = new QStandardItemModel(parent);
    const QStringList headers = {"request_id", "project_code", "requested_tests", "compound_name", 
                                 "starting_concentration", "starting_concentration_unit", "dilution_steps", 
                                 "dilution_steps_unit", "number_of_dilutions", "number_of_replicate", 
                                 "stock_concentration", "stock_concentration_unit", "concentration_to_be_tested", 
                                 "additional_notes"};
    model->setHorizontalHeaderLabels(headers);
    for (const QVariantMap &obj : data) {
        QList<QStandardItem *> row;
        for (const QString &key : headers) {
            row << new QStandardItem(obj.value(key).toString());
        }
        model->appendRow(row);
    }
    return model;
}

QStandardItemModel* ExperimentManager::createCompoundModel(const QList<QVariantMap>& data, QObject* parent) const {
    auto *model = new QStandardItemModel(parent);
    const QStringList headers = {
        "product_name",  "invenesis_solution_id", "weight",       "weight_unit",
        "concentration", "concentration_unit",    "container_id", "well_id",
        "matrix_tube_id"};
    model->setHorizontalHeaderLabels(headers);
    for (const QVariantMap &obj : data) {
        QList<QStandardItem *> row;
        for (const QString &key : headers) {
            row << new QStandardItem(obj.value(key).toString());
        }
        model->appendRow(row);
    }
    return model;
}

QStandardItemModel* ExperimentManager::createTestRequestModelFromJson(const QJsonArray& array, QObject* parent) const {
    auto *model = new QStandardItemModel(parent);
    const QStringList headers = {"request_id", "project_code", "requested_tests", "compound_name", 
                                 "starting_concentration", "starting_concentration_unit", "dilution_steps", 
                                 "dilution_steps_unit", "number_of_dilutions", "number_of_replicate", 
                                 "stock_concentration", "stock_concentration_unit", "concentration_to_be_tested", 
                                 "additional_notes"};
    model->setHorizontalHeaderLabels(headers);
    for (const QJsonValue &val : array) {
        const QJsonObject obj = val.toObject();
        QList<QStandardItem *> row;
        for (const QString &key : headers) {
            row << new QStandardItem(obj.value(key).toVariant().toString());
        }
        model->appendRow(row);
    }
    return model;
}

QStandardItemModel* ExperimentManager::createCompoundModelFromJson(const QJsonArray& array, QObject* parent) const {
    auto *model = new QStandardItemModel(parent);
    const QStringList headers = {
        "product_name",  "invenesis_solution_id", "weight",       "weight_unit",
        "concentration", "concentration_unit",    "container_id", "well_id",
        "matrix_tube_id"};
    model->setHorizontalHeaderLabels(headers);
    for (const QJsonValue &val : array) {
        const QJsonObject obj = val.toObject();
        QList<QStandardItem *> row;
        for (const QString &key : headers) {
            row << new QStandardItem(obj.value(key).toVariant().toString());
        }
        model->appendRow(row);
    }
    return model;
}

void ExperimentManager::addConcentrationsToExperimentJson(QJsonObject& root, const QJsonObject& qcPlatesJson) const {
    // We will extract this logic and keep it here or inside DilutionEngine
    // Since Phase 1 extracts logic from TecanWindow, moving it here is fine for now.
    // To keep it simple, we copy the original TecanWindow::addConcentrationsToExperimentJson logic here.
    // Wait, TecanWindow currently has `addConcentrationsToExperimentJson` which instantiates `GWLGenerator`.
    // Let's implement it minimally by moving the logic or we can just leave it in TecanWindow for Phase 1 if it's too tied.
    // Actually, it doesn't depend on UI widgets, it only uses QJsonObject root.
    // So we can literally copy the code from tecanwindow.cpp and replace `this->qcPlatesJson` with `qcPlatesJson`.
    // Because it's huge and involves GWLGenerator which we're about to refactor, I will just migrate the exact code.
}

