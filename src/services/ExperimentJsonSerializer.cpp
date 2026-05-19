#include "ExperimentJsonSerializer.h"
#include "plate_management/daughterplatewidget.h"
#include "tecan_integration/gwlgenerator.h"
#include "services/DilutionEngine.h"

#include <QJsonDocument>
#include <QFile>
#include <QVariant>
#include <algorithm>

namespace ExperimentJsonSerializer {

QJsonObject buildExperimentJson(
    const QString &experimentCode,
    const QString &username,
    const QAbstractItemModel *trModel,
    const QAbstractItemModel *cmpModel,
    const QMap<QString, QSet<QString>> &matrixPlateMap,
    const QList<DaughterPlateWidget*> &daughterPlates,
    const QList<DaughterPlateWidget*> &testPlates,
    const QList<DaughterPlateWidget*> &qcPlates,
    DaughterPlateType daughterPlateType,
    const QString &qcPlateType
) {
    if (!trModel || trModel->rowCount() == 0 || !cmpModel || cmpModel->rowCount() == 0) {
        return {}; // return an EMPTY object
    }

    auto columnOf = [](const QAbstractItemModel *m, const QString &hdr) -> int {
        for (int c = 0; c < m->columnCount(); ++c)
            if (m->headerData(c, Qt::Horizontal).toString() == hdr)
                return c;
        return -1;
    };

    QJsonObject root;
    root["experiment_code"] = experimentCode;
    root["user"] = username;

    const int projCol = columnOf(trModel, "project_code");
    root["project_code"] = (projCol >= 0) ? trModel->index(0, projCol).data().toString() : QString();

    root["plate_type"] = (daughterPlateType == DaughterPlateType::Plate384) ? QStringLiteral("384") : QStringLiteral("96");
    if (!qcPlateType.isEmpty()) {
        root["qc_plate_type"] = qcPlateType;
    }

    QJsonArray trArray;
    for (int r = 0; r < trModel->rowCount(); ++r) {
        QJsonObject rowObj;
        for (int c = 0; c < trModel->columnCount(); ++c) {
            const QString key = trModel->headerData(c, Qt::Horizontal).toString();
            rowObj[key] = QJsonValue::fromVariant(trModel->index(r, c).data());
        }
        trArray.append(rowObj);
    }
    root["test_requests"] = trArray;

    QJsonArray cmpArray;
    for (int r = 0; r < cmpModel->rowCount(); ++r) {
        QJsonObject rowObj;
        for (int c = 0; c < cmpModel->columnCount(); ++c) {
            const QString key = cmpModel->headerData(c, Qt::Horizontal).toString();
            rowObj[key] = QJsonValue::fromVariant(cmpModel->index(r, c).data());
        }
        cmpArray.append(rowObj);
    }
    root["compounds"] = cmpArray;

    QJsonObject matrixObj;
    for (auto it = matrixPlateMap.cbegin(); it != matrixPlateMap.cend(); ++it) {
        QJsonArray wells;
        for (const QString &w : it.value()) wells.append(w);
        matrixObj[it.key()] = wells;
    }
    root["matrix_plates"] = matrixObj;

    const int dilCol = columnOf(trModel, "number_of_dilutions");
    const int dilutionSteps = (dilCol >= 0) ? trModel->index(0, dilCol).data().toInt() : 3;

    QJsonArray dghtArray;
    for (int i = 0; i < daughterPlates.size(); ++i) {
        if (!daughterPlates[i]) continue;
        QJsonObject plateObj;
        plateObj["plate_number"] = i + 1;
        plateObj["dilution_steps"] = dilutionSteps;
        plateObj["plate_type"] = root["plate_type"]; 
        plateObj["wells"] = daughterPlates[i]->toJson();
        dghtArray.append(plateObj);
    }
    root["daughter_plates"] = dghtArray;

    QJsonArray testArray;
    for (int i = 0; i < testPlates.size(); ++i) {
        if (!testPlates[i]) continue;
        QJsonObject plateObj;
        plateObj["plate_number"] = i + 1;
        plateObj["dilution_steps"] = dilutionSteps;
        plateObj["wells"] = testPlates[i]->toJson();
        testArray.append(plateObj);
    }
    root["test_plates"] = testArray;

    QJsonArray qcArray;
    for (int i = 0; i < qcPlates.size(); ++i) {
        if (!qcPlates[i]) continue;
        QJsonObject plateObj;
        plateObj["plate_number"] = i + 1;
        plateObj["dilution_steps"] = dilutionSteps;
        plateObj["wells"] = qcPlates[i]->toJson();
        qcArray.append(plateObj);
    }
    root["qc_plates"] = qcArray;

    return root;
}

void addConcentrationsToExperimentJson(QJsonObject &root, const QJsonObject& qcPlatesJson) {
    double dilutionFactor = 3.16;
    QString testId;
    double startingConcInTestMicroM = 0.0;
    QJsonArray trArr = root.value("test_requests").toArray();
    if (!trArr.isEmpty()) {
        QJsonObject tr0 = trArr.at(0).toObject();
        bool ok = false;
        double df = tr0.value("dilution_steps").toVariant().toDouble(&ok);
        if (ok && df > 0.0) dilutionFactor = df;

        testId = tr0.value("requested_tests").toString();
        startingConcInTestMicroM = tr0.value("starting_concentration").toVariant().toDouble();
        if (tr0.value("starting_concentration_unit").toString().compare("mM", Qt::CaseInsensitive) == 0) {
            startingConcInTestMicroM *= 1000.0;
        }
    }

    QMap<QString, double> cmpStockMap;
    QJsonArray cmpArr = root.value("compounds").toArray();
    for (const QJsonValue &v : cmpArr) {
        QJsonObject c = v.toObject();
        double sc = c.value("concentration").toVariant().toDouble();
        if (c.value("concentration_unit").toString().compare("mM", Qt::CaseInsensitive) == 0)
            sc *= 1000.0;
        cmpStockMap[c.value("product_name").toString().trimmed()] = sc;
    }

    double stdStockConc = 0.0;
    QJsonObject stdObj = root.value("standard").toObject();
    if (!stdObj.isEmpty()) {
        stdStockConc = stdObj.value("Concentration").toVariant().toDouble();
        if (stdObj.value("ConcentrationUnit").toString().compare("mM", Qt::CaseInsensitive) == 0) {
            stdStockConc *= 1000.0;
        }
    }

    QFile catFile(":/data/resources/data/invenesis_catalogue.json");
    QJsonObject catalogue;
    if (catFile.open(QIODevice::ReadOnly)) {
        catalogue = QJsonDocument::fromJson(catFile.readAll()).object();
    }

    QString stdName;
    double stdTopDoseMicroM = 0.0;
    double stdEc50MicroM = 0.0;
    if (!stdObj.isEmpty())
        stdName = stdObj.value("Samplealias").toString().trimmed();
    if (!stdName.isEmpty() && !testId.isEmpty()) {
        const QJsonObject catEntry = catalogue.value(testId).toObject();
        const QJsonObject stdSpecs = catEntry.value("standards").toObject();
        const QJsonObject stdSpec  = stdSpecs.value(stdName).toObject();
        if (stdSpec.isEmpty()) {
            qWarning() << "[WARN] No catalogue entry for standard" << stdName
                       << "in test" << testId
                       << "— standard concentrations will fall back to compound df.";
        } else {
            stdTopDoseMicroM = stdSpec.value("top_dose_uM").toVariant().toDouble();
            stdEc50MicroM    = stdSpec.value("ec50_uM").toVariant().toDouble();
            qDebug() << "[INFO] Standard catalogue:" << stdName
                     << "test=" << testId
                     << "top_dose=" << stdTopDoseMicroM << "uM"
                     << "ec50=" << stdEc50MicroM << "uM";
        }
    }
    
    auto wellToIndex = [](const QString &w) {
        if (w.size() < 2) return 0;
        int row = w.at(0).toUpper().unicode() - QChar('A').unicode();
        int col = w.mid(1).toInt();
        return col * 100 + row;
    };

    auto computePlateConcs = [&](const QJsonArray &plateArray, const QString &type) {
        QJsonArray newArray;
        for (const QJsonValue &v : plateArray) {
            QJsonObject plateObj = v.toObject();
            QJsonObject concObj;
            QJsonObject wells = plateObj.value("wells").toObject();

            QMap<QString, QStringList> cmpToWells;
            for (auto it = wells.constBegin(); it != wells.constEnd(); ++it) {
                QString cmp = it.value().toString().trimmed();
                if (cmp.isEmpty()) continue;
                if (cmp.startsWith("DMSO", Qt::CaseInsensitive)) {
                    concObj[it.key()] = 0.0;
                    continue;
                }
                cmpToWells[cmp].append(it.key());
            }

            for (auto it = cmpToWells.constBegin(); it != cmpToWells.constEnd(); ++it) {
                QString rawCmp = it.key();
                QString baseCmp = rawCmp;
                int idx = baseCmp.indexOf(" (");
                if (idx > 0) baseCmp = baseCmp.left(idx);

                const bool isStandard = baseCmp.startsWith("Standard", Qt::CaseInsensitive);

                double dfForChain = dilutionFactor;
                if (isStandard && stdTopDoseMicroM > 0.0 && stdEc50MicroM > 0.0
                    && stdEc50MicroM < stdTopDoseMicroM
                    && it.value().size() >= 2) {
                    const int nDil = it.value().size();
                    const double bottom =
                        (stdEc50MicroM * stdEc50MicroM) / stdTopDoseMicroM;
                    dfForChain = std::pow(stdTopDoseMicroM / bottom,
                                          1.0 / (nDil - 1));
                }

                double startConc = 0.0;
                if (type == "qc") {
                    QString qcType = root.value("qc_plate_type").toString();
                    QJsonObject qcDef = qcPlatesJson.value(qcType).toObject();
                    for (const QString &rowKey : qcDef.keys()) {
                        QJsonObject rowDef = qcDef.value(rowKey).toObject();
                        if (rowDef.value("standard").toString() == baseCmp) {
                            startConc = rowDef.value("conc").toVariant().toDouble();
                            break;
                        }
                    }
                } else if (type == "test") {
                    if (isStandard && stdTopDoseMicroM > 0.0)
                        startConc = stdTopDoseMicroM;
                    else
                        startConc = startingConcInTestMicroM;
                } else if (type == "daughter") {
                    double stock = cmpStockMap.value(baseCmp, 0.0);
                    double topForBackCalc = startingConcInTestMicroM;
                    if (isStandard) {
                        stock = stdStockConc;
                        if (stdTopDoseMicroM > 0.0)
                            topForBackCalc = stdTopDoseMicroM;
                    }
                    GWLGenerator::VolumePlanEntry vp;
                    DilutionEngine engine;
                    if (engine.loadVolumePlan(catalogue, testId, stock, topForBackCalc, &vp)) {
                        startConc = vp.concMother;
                    }
                }

                QStringList wlist = it.value();
                std::sort(wlist.begin(), wlist.end(), [&](const QString &a, const QString &b) {
                    return wellToIndex(a) < wellToIndex(b);
                });

                double currentConc = startConc;
                for (const QString &w : wlist) {
                    concObj[w] = currentConc;
                    currentConc /= dfForChain;
                }
            }
            plateObj["concentrations"] = concObj;
            newArray.append(plateObj);
        }
        return newArray;
    };

    root["daughter_plates"] = computePlateConcs(root.value("daughter_plates").toArray(), "daughter");
    root["test_plates"] = computePlateConcs(root.value("test_plates").toArray(), "test");
    root["qc_plates"] = computePlateConcs(root.value("qc_plates").toArray(), "qc");
}

QStandardItemModel* parseTestRequests(const QJsonArray &array, QObject* parent) {
    if (array.isEmpty()) return nullptr;

    auto *model = new QStandardItemModel(parent);
    const QStringList headers = {"request_id", "project_code", "requested_tests",
                                 "compound_name", "starting_concentration",
                                 "starting_concentration_unit", "dilution_steps",
                                 "dilution_steps_unit", "number_of_dilutions",
                                 "number_of_replicate", "stock_concentration",
                                 "stock_concentration_unit", "concentration_to_be_tested",
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

QStandardItemModel* parseCompounds(const QJsonArray &array, QObject* parent) {
    if (array.isEmpty()) return nullptr;

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

} // namespace ExperimentJsonSerializer
