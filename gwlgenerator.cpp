#include "gwlgenerator.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QDebug>
#include <cmath>

GWLGenerator::GWLGenerator() {}

QStringList GWLGenerator::generateTransferCommands(const QJsonObject& experimentJson)
{
    QStringList commands;

    const QJsonArray daughterPlates = experimentJson["daughter_plates"].toArray();
    const QJsonArray compounds = experimentJson["compounds"].toArray();
    const QJsonArray testRequests = experimentJson["test_requests"].toArray();
    const QJsonObject selectedStandard = experimentJson["standard"].toObject();

    const QString standardBarcode = selectedStandard["Containerbarcode"].toString();
    const QString standardBaseWell = selectedStandard["Containerposition"].toString();

    const QMap<QString, QJsonObject> compoundLookup = prepareCompoundLookup(compounds);
    const QMap<QString, QString> testMap = prepareTestLookup(testRequests);
    const QJsonArray volumeMap = loadVolumeMapJson();

    const bool isInv031 = isInvT031Test(testRequests);

    for (const QJsonValue& plateVal : daughterPlates) {
        QJsonObject plate = plateVal.toObject();
        int plateNum = plate["plate_number"].toInt();
        int dilutionSteps = plate.value("dilution_steps").toInt(6);
        QJsonObject wells = plate["wells"].toObject();

        if (isInv031) {
            commands += generateStandardCommandsINV031(standardBarcode, standardBaseWell, plateNum);
        }

        for (const QString& well : wells.keys()) {
            QString compound = wells[well].toString();
            if (compound == "DMSO" || compound == "Standard") continue;

            const QJsonObject& cmpData = compoundLookup.value(compound);
            QString testId = testMap.value(compound);

            double stockConc = cmpData["concentration"].toString().toDouble();
            QJsonObject volumeEntry = getVolumeEntry(testId, stockConc, volumeMap);

            if (volumeEntry.isEmpty()) {
                qWarning() << "⚠️ No match in volume map for compound" << compound << "in test" << testId << "at conc" << stockConc;
                continue;
            }

            commands += generateCompoundTransfer(compound, well, plateNum, cmpData, volumeEntry);
        }
    }

    return commands;
}

QMap<QString, QJsonObject> GWLGenerator::prepareCompoundLookup(const QJsonArray& compounds)
{
    QMap<QString, QJsonObject> map;
    for (const QJsonValue& cmp : compounds) {
        QJsonObject obj = cmp.toObject();
        map[obj["product_name"].toString()] = obj;
    }
    return map;
}

QMap<QString, QString> GWLGenerator::prepareTestLookup(const QJsonArray& testRequests)
{
    QMap<QString, QString> map;
    for (const QJsonValue& val : testRequests) {
        QJsonObject obj = val.toObject();
        map[obj["compound_name"].toString()] = obj["requested_tests"].toString();
    }
    return map;
}

QJsonArray GWLGenerator::loadVolumeMapJson()
{
    QFile file(":/standardjson/jsonfile/volumeMap.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "❌ Failed to open volumeMap.json";
        return {};
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        qWarning() << "⚠️ volumeMap.json is not a JSON array";
        return {};
    }

    return doc.array();
}

QJsonObject GWLGenerator::getVolumeEntry(const QString& testId, double stockConc, const QJsonArray& volumeMap)
{
    QJsonObject bestMatch;
    double bestDiff = std::numeric_limits<double>::max();

    for (const QJsonValue& val : volumeMap) {
        QJsonObject obj = val.toObject();
        if (obj["INVENesisTestID"].toString() != testId)
            continue;

        double mapConc = obj["ConcMother"].toString().toDouble();
        double diff = std::abs(mapConc - stockConc);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestMatch = obj;
        }
    }

    return bestMatch;
}

QStringList GWLGenerator::generateCompoundTransfer(const QString& compound, const QString& targetWell, int plateNumber,
                                                   const QJsonObject& cmpData, const QJsonObject& volumeEntry)
{
    QStringList commands;
    QString plateBarcode = QString("DP%1").arg(plateNumber);

    QString matrixLabel = cmpData["container_id"].toString();
    QString sourceWell = cmpData["well_id"].toString();
    QString tecanSource = QString("SRC_%1_%2").arg(matrixLabel, sourceWell);
    QString tecanTarget = QString("DST_%1_%2").arg(plateBarcode, targetWell);

    double compoundVol = volumeEntry["compound_uL"].toDouble();
    double dmsoVol = volumeEntry["dmso_uL"].toDouble();

    commands << "A;TouchTip;;;DMSO_DIP_TANK;";
    commands << "WASH;Wash;;;WASH;";
    commands << "Aspirate;1;uL;DMSO_CUSHION_TANK;";
    commands << QString("Aspirate;%.2f;uL;DMSO_TANK;").arg(dmsoVol);
    commands << QString("Aspirate;%.2f;uL;%1;").arg(compoundVol).arg(tecanSource);
    commands << QString("Dispense;%.2f;uL;%1;").arg(compoundVol + dmsoVol).arg(tecanTarget);
    commands << "WASH;Final;;;WASH;";

    return commands;
}

QStringList GWLGenerator::generateStandardCommandsINV031(const QString& barcode, const QString& baseWell, int plateNumber)
{
    QStringList commands;

    QStringList stdWells = {"A11", "B11", "C11", "D11", "E11", "F11", "G11", "H11"};
    QStringList matrixWells;

    int col = baseWell.mid(1).toInt();
    for (int i = 0; i < 8; ++i) {
        QChar row = QChar('A' + i);
        matrixWells << QString("%1%2").arg(row).arg(col);
    }

    QString plateBarcode = QString("DP%1").arg(plateNumber);

    commands << "A;TouchTip;;;DMSO_DIP_TANK;";
    commands << "WASH;Wash;;;WASH;";
    commands << "Aspirate;1;uL;DMSO_CUSHION_TANK;";

    for (int i = 0; i < 8; ++i) {
        commands << QString("Aspirate;10.0;uL;SRC_%1_%2;").arg(barcode, matrixWells[i]);
    }
    for (int i = 0; i < 8; ++i) {
        commands << QString("Dispense;10.0;uL;DST_%1_%2;").arg(plateBarcode, stdWells[i]);
    }

    commands << "WASH;Final;;;WASH;";
    return commands;
}

bool GWLGenerator::isInvT031Test(const QJsonArray& testRequests)
{
    for (const QJsonValue& val : testRequests) {
        QString testType = val.toObject().value("requested_tests").toString();
        if (testType.contains("INV-T-031", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}
