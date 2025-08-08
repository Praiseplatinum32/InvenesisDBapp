#include "gwlgenerator.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QLocale>
#include <QtGlobal>

#include <limits>   // std::numeric_limits

// === ctor: cache volume map + core params ===
GWLGenerator::GWLGenerator(double dilutionFactor,
                           const QString &testId,
                           double         stockConcMicroM)
    : dilutionFactor_{dilutionFactor}
    , testId_{testId}
    , stockConcMicroM_{stockConcMicroM}
    , volumeMap_{ loadVolumeMap() }
{
}

// === load the JSON resource containing your volumeMap ===
QJsonObject GWLGenerator::loadVolumeMap() const
{
    QFile f(":/standardjson/jsonfile/volumeMap.json");
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

// === choose nearest bucket by numeric key to stockConcMicroM ===
QJsonArray
GWLGenerator::pickVolumeRules(const QString &testId,
                              double         stockConcMicroM) const
{
    if (!volumeMap_.contains(testId)) return {};

    const QJsonObject buckets = volumeMap_.value(testId).toObject();
    double  bestDiff = std::numeric_limits<double>::infinity();
    QString bestKey;

    for (const QString &k : buckets.keys()) {
        bool ok = false;
        const double c = k.toDouble(&ok);
        if (!ok) continue;
        const double d = qAbs(c - stockConcMicroM);
        if (d < bestDiff) { bestDiff = d; bestKey = k; }
    }
    if (bestKey.isEmpty()) return {};
    return buckets.value(bestKey).toArray();
}

// === 96-well mapping: A1..H12 → 1..96 (0 if invalid) ===
int GWLGenerator::wellToIndex(const QString &well) const
{
    if (well.size() < 2) return 0;

    const QString w = well.trimmed().toUpper();
    const QChar rowCh = w.at(0);
    if (rowCh < 'A' || rowCh > 'H') return 0;

    bool ok = false;
    const int col = w.mid(1).toInt(&ok);
    if (!ok || col < 1 || col > 12) return 0;

    const int row = rowCh.unicode() - 'A' + 1;  // 'A'->1 .. 'H'->8
    return (col - 1) * 8 + row;                 // col-major
}

bool GWLGenerator::isValidWell(const QString &well) const
{
    return wellToIndex(well) > 0;
}

// === Formatting helper: force '.' decimal regardless of OS ===
static inline QString fmtVol(double v)
{
    return QLocale::c().toString(v, 'f', 2);
}

// === GWL record builders =====================================================

QString GWLGenerator::recA(const Labware &lab, int pos, double vol,
                           const QString &lc, const QString &tubeID) const
{
    // NOTE: field order must match your site’s interpreter.
    // Here: A;label;rackID;type;pos;tubeID;vol;lc
    return QString("A;%1;%2;%3;%4;%5;%6;%7")
        .arg(lab.label)
        .arg(lab.rackID)
        .arg(lab.type)
        .arg(pos)
        .arg(tubeID)
        .arg(fmtVol(vol))
        .arg(lc);
}

QString GWLGenerator::recD(const Labware &lab, int pos, double vol,
                           const QString &lc, const QString &tubeID) const
{
    // D;label;rackID;type;pos;tubeID;vol;lc
    return QString("D;%1;%2;%3;%4;%5;%6;%7")
        .arg(lab.label)
        .arg(lab.rackID)
        .arg(lab.type)
        .arg(pos)
        .arg(tubeID)
        .arg(fmtVol(vol))
        .arg(lc);
}

QString GWLGenerator::recW(int scheme) const
{
    return scheme == 1 ? "W;" : QString("W%1;").arg(scheme);
}

QString GWLGenerator::recC(const QString &text) const
{
    return "C;" + text;
}

// NOTE on R;: many sites expect TOTAL volume across all tips.
// If your parser expects PER-TIP volume, adjust totalVol accordingly.
QString GWLGenerator::recMultiAsp(const Labware &lab,
                                  int start, int end,
                                  double totalVol,
                                  const QString &lc) const
{
    const int channels = end - start + 1;
    // REV = aspirate with multi-channel head
    return QString("R;%1;;%2;%3;%4;%5;%6;0;%7;REV")
        .arg(lab.label)
        .arg(lab.rackID)
        .arg(start)
        .arg(end)
        .arg(fmtVol(totalVol))
        .arg(lc)
        .arg(channels);
}

QString GWLGenerator::recMultiDisp(const Labware &lab,
                                   int start, int end,
                                   double totalVol,
                                   const QString &lc) const
{
    const int channels = end - start + 1;
    // FWD = dispense with multi-channel head
    return QString("R;%1;;%2;%3;%4;%5;%6;0;%7;FWD")
        .arg(lab.label)
        .arg(lab.rackID)
        .arg(start)
        .arg(end)
        .arg(fmtVol(totalVol))
        .arg(lc)
        .arg(channels);
}

// === Block primitive =========================================================
void GWLGenerator::addDispenseBlock(const DispenseBlock &b,
                                    QStringList &cmds) const
{
    const int nTips = b.tgtEnd - b.tgtStart + 1;
    if (nTips <= 0) {
        cmds << recC("WARNING: addDispenseBlock called with nTips <= 0");
        return;
    }

    // 1) DMSO pre-asp across all tips
    if (b.preAspVol > 0.0 && !b.preAspLC.isEmpty()) {
        cmds << recMultiAsp(troughPreAspi_,
                            1, nTips,
                            b.preAspVol * nTips,
                            b.preAspLC);
    }

    // 2) Diluent aspirate
    if (b.diluentVolPer > 0.0 && !b.diluentLC.isEmpty()) {
        cmds << recMultiAsp(b.diluentLab,
                            1, nTips,
                            b.diluentVolPer * nTips,
                            b.diluentLC);
    }

    // 3) Optional: mother compound aspirate (one position)
    if (b.motherVolPer > 0.0 && b.motherPos > 0 && !b.motherLC.isEmpty()) {
        cmds << recMultiAsp(b.motherLab,
                            b.motherPos, b.motherPos,
                            b.motherVolPer * nTips,
                            b.motherLC);
    }

    // 4) Multi-dispense into daughter wells
    if (b.mixVolPer > 0.0 && !b.mixLC.isEmpty()) {
        cmds << recMultiDisp(b.targetLab,
                             b.tgtStart, b.tgtEnd,
                             b.mixVolPer * nTips,
                             b.mixLC);
    }

    // 5) Wash
    cmds << recW(b.washScheme);
}

// === Feature emitters ========================================================

QStringList
GWLGenerator::makeStandardHitPick(const QJsonObject &stdObj,
                                  int /*plateNum*/) const
{
    QStringList cmds;

    const QString bc   = stdObj.value("Containerbarcode").toString().trimmed();
    const QString well = stdObj.value("Containerposition").toString().trimmed();

    if (bc.isEmpty() || !isValidWell(well)) {
        cmds << recC("ERROR: Invalid standard (barcode or well).");
        return cmds;
    }
    const int pos = wellToIndex(well);

    cmds << recC("**************  Hit-Pick STD  **************")
         << recA(troughPreAspi_, 1, 50.0, "DMSO_pre_aspiration");

    {
        Labware lw = stdMatrixLab_;
        lw.rackID  = bc;
        cmds << recA(lw, pos, 30.0, "P00DMSO_copy_Matrix");
    }

    // STD into A1 (adjust if policy changes)
    cmds << recD(daughterLab_, wellToIndex("A1"), 30.0, "DMSONOMixHitList")
         << recW(1);

    return cmds;
}

QStringList
GWLGenerator::makeCompoundHitPick(const QJsonObject &cmpObj,
                                  int /*plateNum*/) const
{
    QStringList cmds;

    const QString bc    = cmpObj.value("container_id").toString().trimmed();
    const QString well  = cmpObj.value("well_id").toString().trimmed();

    if (bc.isEmpty() || !isValidWell(well)) {
        cmds << recC(QString("WARNING: Skipping compound (invalid bc/well): %1/%2")
                         .arg(bc, well));
        return cmds;
    }

    // Destination column inferred from mother well's column
    bool okCol = false;
    const int col = well.mid(1).toInt(&okCol);
    if (!okCol || col < 1 || col > 12) {
        cmds << recC(QString("WARNING: Skipping compound (bad column in %1)").arg(well));
        return cmds;
    }

    const QString dstWell = QString("A%1").arg(col);
    const int     dstIdx  = wellToIndex(dstWell);
    if (dstIdx == 0) {
        cmds << recC(QString("WARNING: Skipping compound (bad dst well %1)").arg(dstWell));
        return cmds;
    }

    // Build block
    DispenseBlock b;
    // 1) pre-asp
    b.preAspVol      = 50.0;
    b.preAspLC       = "DMSO_pre_aspiration";
    // 2) diluent
    b.diluentLab     = troughDilDMSO_;
    b.diluentVolPer  = 15.0;
    b.diluentLC      = "DMSO";
    // 3) mother
    b.motherLab      = cmpMatrixLab_;
    b.motherLab.rackID = bc;
    b.motherPos      = wellToIndex(well);
    b.motherVolPer   = 10.0;
    b.motherLC       = "P00DMSO_copy_Matrix";
    // 4) targets
    b.targetLab      = daughterLab_;
    b.tgtStart       = dstIdx;
    b.tgtEnd         = dstIdx;
    b.mixVolPer      = 25.0;  // 15 DMSO + 10 mother
    b.mixLC          = "DMSONOMixHitList";
    b.washScheme     = 1;

    addDispenseBlock(b, cmds);
    return cmds;
}

QStringList
GWLGenerator::makeSerialDilution(const QJsonObject &plateObj,
                                 const QJsonObject &cmpObj,
                                 int /*plateNum*/) const
{
    // Skeleton implementation showing how to reuse DispenseBlock.
    // Fill per-step volumes from your policy/volume map if desired.

    QStringList cmds;

    const int steps = plateObj.value("dilution_steps").toInt(0);
    if (steps <= 0) return cmds;

    // We derive starting column from the placed column of the compound.
    const QString motherWell = cmpObj.value("well_id").toString().trimmed();
    bool okCol = false;
    int  startCol = motherWell.mid(1).toInt(&okCol);
    if (!okCol || startCol < 1 || startCol > 12) {
        cmds << recC("WARNING: Serial dilution skipped (bad mother column).");
        return cmds;
    }

    // Policy: row A, columns startCol..startCol+steps-1
    for (int s = 0; s < steps; ++s) {
        const int dstCol = startCol + s;
        if (dstCol > 12) {
            cmds << recC("WARNING: Serial dilution hit column >12; stopping.");
            break;
        }

        // In step 0, mother is the matrix; next steps use previous daughter well.
        const bool useMotherFromMatrix = (s == 0);

        DispenseBlock b;
        b.preAspVol      = 50.0;
        b.preAspLC       = "DMSO_pre_aspiration";

        b.diluentLab     = troughDilDMSO_;
        b.diluentVolPer  = 15.0;                   // example
        b.diluentLC      = "DMSO";

        if (useMotherFromMatrix) {
            b.motherLab       = cmpMatrixLab_;
            b.motherLab.rackID= cmpObj.value("container_id").toString().trimmed();
            b.motherPos       = wellToIndex(motherWell);
            b.motherVolPer    = 10.0;              // example
            b.motherLC        = "P00DMSO_copy_Matrix";
        } else {
            b.motherLab       = daughterLab_;
            const QString prevWell = QString("A%1").arg(dstCol - 1);
            b.motherPos       = wellToIndex(prevWell);
            b.motherVolPer    = 10.0;              // example
            b.motherLC        = "DMSONOMixHitList";
        }

        b.targetLab      = daughterLab_;
        const QString dstWell = QString("A%1").arg(dstCol);
        b.tgtStart       = wellToIndex(dstWell);
        b.tgtEnd         = b.tgtStart;

        b.mixVolPer      = b.diluentVolPer + b.motherVolPer;
        b.mixLC          = "DMSONOMixHitList";
        b.washScheme     = 1;

        if (b.motherPos == 0 || b.tgtStart == 0) {
            cmds << recC("WARNING: Skipping a dilution step due to invalid well.");
            continue;
        }

        addDispenseBlock(b, cmds);
    }

    return cmds;
}

// === Top-level ===============================================================
QStringList
GWLGenerator::generateTransferCommands(const QJsonObject &exp) const
{
    QStringList gwl;

    // Header comments (self-describing)
    gwl << recC("===== Invenesis GWL =====")
        << recC(QString("TestID=%1 | StockConc=%2uM | DilutionFactor=%3")
                    .arg(testId_)
                    .arg(stockConcMicroM_, 0, 'f', 2)
                    .arg(dilutionFactor_, 0, 'f', 2));

    // Unwrap experiment
    const QJsonObject stdObj = exp.value("standard").toObject();
    const QJsonArray  plates = exp.value("daughter_plates").toArray();
    const QJsonArray  comps  = exp.value("compounds").toArray();

    // Preflight: basic checks
    if (stdObj.isEmpty()) {
        gwl << recC("ERROR: Missing standard object.");
        return gwl;
    }
    if (plates.isEmpty()) {
        gwl << recC("ERROR: No daughter plates provided.");
        return gwl;
    }

    // Emit per-plate content
    for (const QJsonValue &pval : plates) {
        const QJsonObject plateObj = pval.toObject();

        // Standard hit-pick
        gwl += makeStandardHitPick(stdObj, plateObj.value("plate_number").toInt());

        // Compounds hit-pick (exclude standard matrix)
        const QString stdBC = stdObj.value("Containerbarcode").toString();
        for (const QJsonValue &cval : comps) {
            const QJsonObject cmpObj = cval.toObject();
            if (cmpObj.value("container_id").toString() != stdBC)
                gwl += makeCompoundHitPick(cmpObj, plateObj.value("plate_number").toInt());
        }

        // Dilutions
        gwl << recC("**************  Hit-Pick Dilutions  **************");
        for (const QJsonValue &cval : comps) {
            const QJsonObject cmpObj = cval.toObject();
            if (cmpObj.value("container_id").toString() != stdBC)
                gwl += makeSerialDilution(plateObj, cmpObj, plateObj.value("plate_number").toInt());
        }
    }

    // Final extra-wash — batched in one 8-channel block
    gwl << recC("**********  Lavage supplementaire (scheme 2)  **********");
    {
        const int n = 8;
        gwl << recMultiAsp (lavageLab_, 1, n, 50.0 * n, "LavageDMSO")
            << recMultiDisp(lavageLab_, 1, n, 50.0 * n, "LavageDMSO")
            << recW(2);
    }

    return gwl;
}
