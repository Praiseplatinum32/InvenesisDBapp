#include "gwlgenerator.h"

// Qt
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDebug>
#include <cmath>
#include <algorithm>

/* ===================================================================== */
/*                    constants / helpers in anon ns                     */
/* ===================================================================== */
namespace {
constexpr double kStdMotherVol = 20.0;   // µL compound for STD row A
constexpr double kStdDmsoVol   = 5.0;    // µL DMSO    for STD row A
constexpr int    kLinesPerUnit = 7;      // Touch→Final Wash per single‑tip unit
}

/* ------------------------------------------------------------------ */
/* helper: convert JSON → stable bytes (for sorting arrays by element) */
static QByteArray jsonBytes(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Object: return QJsonDocument(v.toObject())
                                   .toJson(QJsonDocument::Compact);
    case QJsonValue::Array:  return QJsonDocument(v.toArray())
                                   .toJson(QJsonDocument::Compact);
    case QJsonValue::String: return v.toString().toUtf8();
    case QJsonValue::Double: return QByteArray::number(v.toDouble(),'f',4);
    case QJsonValue::Bool:   return v.toBool() ? "true" : "false";
    default:                 return "null";
    }
}

/* ===================================================================== */
/*                           ctor                                        */
/* ===================================================================== */
GWLGenerator::GWLGenerator(double factor) : dilutionFactor_{factor} {}

/* ===================================================================== */
/*                      STANDARD hit‑pick block                          */
/* ===================================================================== */
static QStringList makeStandardHitPick(const QString &matrixBarcode,
                                       const QString &matrixWell,
                                       int            plateNum)
{
    const QString plateBarcode = QString("DP%1").arg(plateNum);
    const QString tecSrc = QString("SRC_%1_%2").arg(matrixBarcode, matrixWell);
    const QString tecDst = QString("DST_%1_A1").arg(plateBarcode);
    const double  total  = kStdMotherVol + kStdDmsoVol;

    return {
        //"C;Commentaire**************  Hit-Pick STD  **************Commentaire",
        //"A;TroughDMSOPreAspi;;;1;;50;DMSO_pre_aspiration",
        // QString("A;;%1;Matrix05Tube;16;;30;P00DMSO_copy_Matrix").arg(matrixBarcode),
        // "D;MPDaughter;;96daughterInv;1;;30;DMSONOMixHitList",
        // "W;",
        "A;TouchTip;;;DMSO_DIP_TANK;",
        "WASH;Wash;;;WASH;",
        "Aspirate;1;uL;DMSO_CUSHION_TANK;",
        QString("Aspirate;%1;uL;DMSO_TANK;").arg(QString::number(kStdDmsoVol,'f',2)),
        QString("Aspirate;%1;uL;%2;").arg(QString::number(kStdMotherVol,'f',2), tecSrc),
        QString("Dispense;%1;uL;%2;").arg(QString::number(total,'f',2), tecDst),
        "WASH;Final;;;WASH;"
    };
}

/* ===================================================================== */
/*                     public entry: generate file                        */
/* ===================================================================== */
QStringList GWLGenerator::generateTransferCommands(const QJsonObject &exp) const
{
    QStringList gwl;         // final result
    QStringList warnings;

    const QJsonArray plates     = exp["daughter_plates"].toArray();
    const QJsonArray compounds  = exp["compounds"].toArray();
    const QJsonArray requests   = exp["test_requests"].toArray();

    const auto cmpIdx  = buildCompoundIndex(compounds);
    const auto testIdx = buildTestIndex(requests);
    const QJsonObject volMap = loadVolumeMap();

    const QJsonObject stdObj   = exp["standard"].toObject();
    const QString stdBarcode   = stdObj["Containerbarcode"].toString();
    const QString stdWell      = stdObj["Containerposition"].toString();
    const bool hasInv031       = [&]{
        for (const QJsonValue &v : requests)
            if (v.toObject()["requested_tests"].toString()
                    .contains("INV-T-031", Qt::CaseInsensitive))
                return true;
        return false;
    }();

    /* ===== iterate plates =========================================== */
    for (const QJsonValue &plateVal : plates)
    {
        const QJsonObject plate = plateVal.toObject();
        const int plateNum      = plate["plate_number"].toInt();
        const int steps         = plate["dilution_steps"].toInt(6);
        const QJsonObject wells = plate["wells"].toObject();

        /* 0‑‑write STANDARD block directly to final list -------------- */
        gwl += makeStandardHitPick(stdBarcode, stdWell, plateNum);

        QStringList singleTip;   // un‑batched 7‑line units

        /* 1‑‑process every compound well ------------------------------ */
        for (const QString &well : wells.keys())
        {
            const QString compound = wells[well].toString();
            if (compound == "DMSO" || compound == "Standard") continue;

            const auto cmpIt  = cmpIdx.find(compound);
            const auto testIt = testIdx.find(compound);
            if (cmpIt == cmpIdx.end() || testIt == testIdx.end()) {
                warnings << QString("Compound '%1' missing lookup").arg(compound);
                continue;
            }

            const double stock   = cmpIt->value("concentration").toString().toDouble();
            const QString testId = testIt.value();
            const QJsonObject rule = pickVolumeRule(testId, stock, volMap, &warnings);
            if (rule.isEmpty()) continue;

            /* first dilution into well (row B/C/…) */
            singleTip += makeFirstDilution(compound, well, plateNum, *cmpIt, rule);

            /* serial chain to the right */
            const QString rowLetter = well.left(1);
            const int     startCol  = well.mid(1).toInt();
            singleTip += makeSerialDilution(rowLetter, startCol,
                                            plateNum, steps-1,
                                            totalWellVol_);

            /* pure DMSO row H – same column */
            const QString dmsoWell = QStringLiteral("H%1").arg(startCol);
            const QString tecDst   = QStringLiteral("DST_DP%1_%2")
                                       .arg(plateNum)          // %1  ← plate number
                                       .arg(dmsoWell);         // %2  ← "H<n>"

            singleTip << "A;TouchTip;;;DMSO_DIP_TANK;"
                      << "WASH;Wash;;;WASH;"
                      << "Aspirate;1;uL;DMSO_CUSHION_TANK;"
                      << QStringLiteral("Aspirate;%1;uL;DMSO_TANK;")
                             .arg(QString::number(totalWellVol_,'f',2))
                      << QStringLiteral("Dispense;%1;uL;%2;")
                             .arg(QString::number(totalWellVol_,'f',2), tecDst)
                      << "WASH;Final;;;WASH;";

        }

        /* 2‑‑special INV‑T‑031 columns 11+12 -------------------------- */
        if (hasInv031) {
            /* col 11 standard */
            singleTip += makeFirstDilution("STD031", "A11", plateNum,
                                           {{"container_id",stdBarcode},
                                            {"well_id",stdWell}},
                                           {{"VolMother",kStdMotherVol},
                                            {"DMSO",kStdDmsoVol},
                                            {"test","INV-T-031"}});
            /* col 12 pure DMSO */
            const QString dmsoDst = QString("DST_DP%1_A12").arg(plateNum);
            singleTip << "A;TouchTip;;;DMSO_DIP_TANK;"
                      << "WASH;Wash;;;WASH;"
                      << "Aspirate;1;uL;DMSO_CUSHION_TANK;"
                      << QString("Aspirate;%1;uL;DMSO_TANK;")
                           .arg(QString::number(totalWellVol_,'f',2))
                      << QString("Dispense;%1;uL;%2;")
                           .arg(QString::number(totalWellVol_,'f',2), dmsoDst)
                      << "WASH;Final;;;WASH;";
        }

        /* 3‑‑batch compound/DMSO units column‑wise -------------------- */
        gwl += makeEightChannelBlock(singleTip);
    }

    /* -------- prepend warnings ---------------------------- */
    if (!warnings.isEmpty()) {
        gwl.prepend("; Warnings:");
        for (const QString &w : warnings) gwl << "; " + w;
        gwl << "";
    }
    return gwl;
}

/* ===================================================================== */
/*                      mapping helpers                                  */
/* (unchanged from previous version)                                     */
/* ===================================================================== */
QJsonObject GWLGenerator::loadVolumeMap() const
{
    QFile f(":/standardjson/jsonfile/volumeMap.json");
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "❌ volumeMap.json not found";
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QJsonObject GWLGenerator::pickVolumeRule(const QString      &testId,
                                         double              stockConc,
                                         const QJsonObject  &map,
                                         QStringList        *warn) const
{
    if (!map.contains(testId)) {
        if (warn) warn->append(QString("No volume rule for test '%1'").arg(testId));
        return {};
    }
    const QJsonObject buckets = map[testId].toObject();
    QJsonObject best;
    double bestDiff = 1e9;

    for (const QString &k : buckets.keys()) {
        bool ok=false; double c = k.toDouble(&ok); if (!ok) continue;
        double diff = std::abs(c - stockConc);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = buckets[k].toArray().first().toObject();
        }
    }
    if (best.isEmpty() && warn)
        warn->append(QString("No conc. rule for test '%1' (stock %2)")
                         .arg(testId).arg(stockConc));
    return best;
}

QMap<QString,QJsonObject>
GWLGenerator::buildCompoundIndex(const QJsonArray &arr) const
{
    QMap<QString,QJsonObject> m;
    for (const QJsonValue &v : arr)
        m[v["product_name"].toString()] = v.toObject();
    return m;
}

QMap<QString,QString>
GWLGenerator::buildTestIndex(const QJsonArray &arr) const
{
    QMap<QString,QString> m;
    for (const QJsonValue &v : arr)
        m[v["compound_name"].toString()] = v["requested_tests"].toString();
    return m;
}

/* ===================================================================== */
/*               command generators (unchanged logic)                    */
/* ===================================================================== */
QStringList GWLGenerator::makeFirstDilution(const QString &compound,
                                            const QString &dstWell,
                                            int            plate,
                                            const QJsonObject &cmp,
                                            const QJsonObject &rule) const
{
    const QString plateBarcode = QString("DP%1").arg(plate);
    const QString srcMatrix = cmp["container_id"].toString();
    const QString srcWell   = cmp["well_id"].toString();
    const QString tecSrc    = QString("SRC_%1_%2").arg(srcMatrix, srcWell);
    const QString tecDst    = QString("DST_%1_%2").arg(plateBarcode, dstWell);

    const double volCmp  = rule["VolMother"].toDouble();
    const double volDmso = rule["DMSO"].toDouble();
    const double total   = volCmp + volDmso;

    QStringList cmd;
    cmd << "A;TouchTip;;;DMSO_DIP_TANK;"
        << "WASH;Wash;;;WASH;"
        << "Aspirate;1;uL;DMSO_CUSHION_TANK;"
        << QString("Aspirate;%1;uL;DMSO_TANK;")
              .arg(QString::number(volDmso,'f',2))
        << QString("Aspirate;%1;uL;%2;")
              .arg(QString::number(volCmp,'f',2), tecSrc)
        << QString("Dispense;%1;uL;%2;")
              .arg(QString::number(total,'f',2), tecDst)
        << "WASH;Final;;;WASH;";
    return cmd;
}

QStringList GWLGenerator::makeSerialDilution(const QString &row,
                                             int            startCol,
                                             int            plate,
                                             int            steps,
                                             double         firstVol) const
{
    if (steps <= 0) return {};

    const QString plateBarcode = QString("DP%1").arg(plate);
    const double srcVol = firstVol / dilutionFactor_;
    const QString volStr = QString::number(srcVol,'f',2);

    QStringList cmd;
    for (int i = 0; i < steps; ++i) {
        const int srcCol = startCol + i;
        const int dstCol = srcCol + 1;
        const QString srcWell = row + QString::number(srcCol);
        const QString dstWell = row + QString::number(dstCol);
        const QString tecSrc = QString("DST_%1_%2").arg(plateBarcode, srcWell);
        const QString tecDst = QString("DST_%1_%2").arg(plateBarcode, dstWell);

        cmd << "A;TouchTip;;;DMSO_DIP_TANK;"
            << "WASH;Wash;;;WASH;"
            << "Aspirate;1;uL;DMSO_CUSHION_TANK;"
            << QString("Aspirate;%1;uL;%2;").arg(volStr, tecSrc)
            << QString("Dispense;%1;uL;%2;").arg(volStr, tecDst)
            << "WASH;Final;;;WASH;";
    }
    return cmd;
}

/* ===================================================================== */
/*     batch 7‑line units by destination column (8‑channel head)         */
/* ===================================================================== */
QStringList GWLGenerator::makeEightChannelBlock(const QStringList &single) const
{
    if (single.isEmpty()) return {};

    QMap<int,QList<QStringList>> columnBuckets;

    for (int i = 0; i + kLinesPerUnit - 1 < single.size(); i += kLinesPerUnit) {
        const QStringList unit = single.mid(i, kLinesPerUnit);
        const QString dispLine = unit[5];                            // Dispense line
        const int pos = dispLine.lastIndexOf('_');
        const int col = dispLine.mid(pos+2).remove(';').toInt();
        columnBuckets[col].append(unit);
    }

    QStringList out;
    for (auto it = columnBuckets.begin(); it != columnBuckets.end(); ++it)
        for (const QStringList &u : it.value())
            out += u;
    return out;
}

/* ===================================================================== */
/*                       optional file‑save                              */
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

    QMessageBox::information(parent, "Saved",
                             QObject::tr("GWL saved to:\n%1").arg(path));
    return true;
}
