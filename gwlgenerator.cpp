#include "gwlgenerator.h"

// Qt
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDebug>
#include <cmath>

/* ===================================================================== */
/*                       public entry point                              */
/* ===================================================================== */
GWLGenerator::GWLGenerator() = default;

QStringList GWLGenerator::generateTransferCommands(const QJsonObject &expJson)
{
    QStringList cmds, errors;

    const QJsonArray  plates        = expJson["daughter_plates"].toArray();
    const QJsonArray  compounds     = expJson["compounds"].toArray();
    const QJsonArray  testReqs      = expJson["test_requests"].toArray();
    const QJsonObject standardObj   = expJson["standard"].toObject();

    const QString stdBarcode   = standardObj["Containerbarcode"].toString();
    const QString stdBaseWell  = standardObj["Containerposition"].toString();

    const auto cmpLookup  = prepareCompoundLookup(compounds);
    const auto testLookup = prepareTestLookup(testReqs);
    const QJsonObject volMap = loadVolumeMapJson();

    const bool isInv031 = isInvT031Test(testReqs);

    for (const QJsonValue &plateVal : plates) {
        const QJsonObject plateObj   = plateVal.toObject();
        const int         plateNum   = plateObj["plate_number"].toInt();
        const int         dilutions  = plateObj.value("dilution_steps").toInt(6);
        const QJsonObject wellsObj   = plateObj["wells"].toObject();

        if (isInv031)
            cmds += generateStandardCommandsINV031(stdBarcode, stdBaseWell, plateNum);

        for (const QString &wellKey : wellsObj.keys())
        {
            const QString compound = wellsObj[wellKey].toString();
            if (compound == "DMSO" || compound == "Standard") continue;

            const QJsonObject cmpData = cmpLookup.value(compound);
            const QString     testId  = testLookup.value(compound);
            const double      stock   = cmpData["concentration"].toString().toDouble();

            const QJsonObject volEntry = getVolumeEntry(testId, stock, volMap, &errors);
            if (volEntry.isEmpty()) continue;               // already logged in errors

            cmds += generateCompoundTransfer(compound, wellKey, plateNum,
                                             cmpData, volEntry);
        }
    }

    /* prepend any warnings */
    if (!errors.isEmpty()) {
        cmds.prepend("; Warnings:");
        for (const QString &e : std::as_const(errors)) cmds << "; " + e;
        cmds << "";      // blank line
    }
    return cmds;
}

/* ===================================================================== */
/*                      1) JSON mapping helpers                          */
/* ===================================================================== */
QJsonObject GWLGenerator::loadVolumeMapJson()
{
    QFile f(":/standardjson/jsonfile/volumeMap.json");
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "❌ Cannot open volumeMap.json";
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        qWarning() << "⚠️ volumeMap.json root is not an object";
        return {};
    }
    return doc.object();
}

QJsonObject GWLGenerator::getVolumeEntry(const QString      &testId,
                                         double              stockConc,
                                         const QJsonObject   &volMap,
                                         QStringList        *errors) const
{
    if (!volMap.contains(testId) || !volMap[testId].isObject()) {
        if (errors) errors->append(QString("No volume map for test '%1'").arg(testId));
        return {};
    }

    /* choose the concentration‑bucket closest to stockConc */
    const QJsonObject testObj = volMap[testId].toObject();
    QJsonObject bestArrayObj;
    double bestDiff = std::numeric_limits<double>::max();

    for (const QString &concKey : testObj.keys()) {
        bool ok = false;
        const double concVal = concKey.toDouble(&ok);
        if (!ok) continue;

        const double diff = std::abs(concVal - stockConc);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestArrayObj = testObj[concKey].toArray().first().toObject(); // first recipe
        }
    }

    if (bestArrayObj.isEmpty() && errors) {
        errors->append(QString("No match in volumeMap for test '%1' "
                               "and stock concentration '%2'")
                           .arg(testId).arg(stockConc));
    }
    return bestArrayObj;
}

/* ===================================================================== */
/*                       2) small look‑up helpers                        */
/* ===================================================================== */
QMap<QString,QJsonObject>
GWLGenerator::prepareCompoundLookup(const QJsonArray &arr) const
{
    QMap<QString,QJsonObject> map;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        map[o["product_name"].toString()] = o;
    }
    return map;
}

QMap<QString,QString>
GWLGenerator::prepareTestLookup(const QJsonArray &arr) const
{
    QMap<QString,QString> map;
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        map[o["compound_name"].toString()] = o["requested_tests"].toString();
    }
    return map;
}

bool GWLGenerator::isInvT031Test(const QJsonArray &reqs) const
{
    for (const QJsonValue &v : reqs)
        if (v.toObject().value("requested_tests").toString()
                .contains("INV-T-031", Qt::CaseInsensitive))
            return true;
    return false;
}

/* ===================================================================== */
/*                  3) command‑string generators                         */
/* ===================================================================== */
QStringList GWLGenerator::generateCompoundTransfer(const QString     &compound,
                                                   const QString     &dstWell,
                                                   int                plateNum,
                                                   const QJsonObject &cmpData,
                                                   const QJsonObject &vol)
{
    QStringList out;
    const QString plateBarcode = QString("DP%1").arg(plateNum);

    const QString srcMatrix = cmpData["container_id"].toString();
    const QString srcWell   = cmpData["well_id"].toString();
    const QString tecSrc    = QString("SRC_%1_%2").arg(srcMatrix, srcWell);
    const QString tecDst    = QString("DST_%1_%2").arg(plateBarcode, dstWell);

    const double  cmpVol = vol["VolMother"].toDouble();
    const double  dmsoVol= vol["DMSO"].toDouble();

    out << "A;TouchTip;;;DMSO_DIP_TANK;";
    out << "WASH;Wash;;;WASH;";
    out << "Aspirate;1;uL;DMSO_CUSHION_TANK;";
    out << QString("Aspirate;%.2f;uL;DMSO_TANK;").arg(dmsoVol);
    out << QString("Aspirate;%.2f;uL;%1;").arg(cmpVol).arg(tecSrc);
    out << QString("Dispense;%.2f;uL;%1;").arg(cmpVol + dmsoVol).arg(tecDst);
    out << "WASH;Final;;;WASH;";
    return out;
}

QStringList GWLGenerator::generateStandardCommandsINV031(const QString &barcode,
                                                         const QString &baseWell,
                                                         int            plateNum) const
{
    QStringList cmds;
    const QString plateBarcode = QString("DP%1").arg(plateNum);

    /* wells A11..H11 on daughter, same column from mother plate */
    const QStringList dstWells = {"A11","B11","C11","D11","E11","F11","G11","H11"};
    QStringList srcWells;

    const int col = baseWell.mid(1).toInt();
    for (int i = 0; i < 8; ++i)
        srcWells << QString("%1%2").arg(QChar('A'+i)).arg(col);

    cmds << "A;TouchTip;;;DMSO_DIP_TANK;";
    cmds << "WASH;Wash;;;WASH;";
    cmds << "Aspirate;1;uL;DMSO_CUSHION_TANK;";

    for (const QString &w : srcWells)
        cmds << QString("Aspirate;10.0;uL;SRC_%1_%2;").arg(barcode, w);
    for (const QString &w : dstWells)
        cmds << QString("Dispense;10.0;uL;DST_%1_%2;").arg(plateBarcode, w);

    cmds << "WASH;Final;;;WASH;";
    return cmds;
}

/* ===================================================================== */
/*                        4) optional file‑save                          */
/* ===================================================================== */
bool GWLGenerator::saveToFile(const QStringList &lines,
                              const QString     &defaultName,
                              QWidget           *parent)
{
    const QString path = QFileDialog::getSaveFileName(parent, "Save GWL File",
                                                      defaultName + ".gwl",
                                                      "GWL files (*.gwl)");
    if (path.isEmpty()) return false;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(parent, "File Error", f.errorString());
        return false;
    }
    QTextStream(&f) << lines.join('\n');
    f.close();

    QMessageBox::information(parent, "Saved",  QObject::tr("GWL saved to:\n%1").arg(path));
    return true;
}
