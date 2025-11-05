#include "gwlgenerator.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include <QObject>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QTextStream>

// ========================== Façade (public API) ==========================

GWLGenerator::GWLGenerator() {}

GWLGenerator::GWLGenerator(double dilutionFactor,
                           const QString &testId,
                           double stockConcMicroM,
                           Instrument instrument)
    : dilutionFactor_(dilutionFactor),
    testId_(testId.trimmed()),
    stockConc_(stockConcMicroM),
    instrument_(instrument)
{
    if (instrument_ == Instrument::EVO150)
        backend_ = std::make_unique<Evo150Backend>(*this);
    else
        backend_ = std::make_unique<FluentBackend>(*this);
}

GWLGenerator::~GWLGenerator() = default;

bool GWLGenerator::generate(const QJsonObject &root,
                            QVector<FileOut> &outs,
                            QString *err) const
{
    if (!backend_) { if (err) *err = "No backend"; return false; }
    return backend_->generate(root, outs, err);
}

bool GWLGenerator::generateAuxiliary(const QJsonObject &root,
                                     QVector<FileOut> &outs,
                                     QString *err) const
{
    if (!backend_) { if (err) *err = "No backend"; return false; }
    return backend_->generateAux(root, outs, err);
}

// ============================= EVO backend (stubs) ===============================

bool GWLGenerator::Evo150Backend::generate(const QJsonObject &,
                                           QVector<FileOut> &,
                                           QString *err) const
{
    if (err) *err = "EVO150 backend not implemented yet.";
    return true;
}

bool GWLGenerator::Evo150Backend::generateAux(const QJsonObject &,
                                              QVector<FileOut> &,
                                              QString *) const
{
    return true;
}

// ========================== Helpers (file local) ================================

namespace {

// normalize "A01"/"a1" -> "A1"
static QString normWell(const QString &s) {
    if (s.isEmpty()) return s;
    QString t = s.trimmed().toUpper();
    if (t.size() < 2) return t;
    QChar row = t.at(0);
    bool ok=false; int col = t.mid(1).toInt(&ok);
    if (!ok) return t;
    return QString("%1%2").arg(row).arg(col);
}

// "A1" -> "A01" for CSV
static QString toA01(const QString &s) {
    const QString n = normWell(s);
    if (n.size() < 2) return n;
    const QChar row = n.at(0);
    bool ok=false; int col = n.mid(1).toInt(&ok);
    if (!ok) return n;
    return QString("%1%2").arg(row).arg(col, 2, 10, QChar('0'));
}

// A1..H12 <-> 1..96 (column-major: down A..H then next column)
static int wellNameToIndex96(const QString &w) {
    const QString n = normWell(w);
    if (n.size() < 2) return -1;
    const QChar rowCh = n.at(0);
    bool ok=false; int col = n.mid(1).toInt(&ok);
    if (!ok || col < 1 || col > 12) return -1;
    int row = rowCh.unicode() - QChar('A').unicode();
    if (row < 0 || row > 7) return -1;
    return (col - 1) * 8 + row + 1; // 1..96
}

static QString indexToWellName96(int idx) {
    if (idx < 1 || idx > 96) return {};
    const int zero = idx - 1;
    const int row  = zero % 8;           // 0..7 -> A..H
    const int col  = zero / 8 + 1;       // 1..12
    const QChar rowCh = QChar('A' + row);
    return QString("%1%2").arg(rowCh).arg(col);
}

static QSet<int> fullPlate96Indices() {
    QSet<int> s; for (int i=1;i<=96;++i) s.insert(i); return s;
}

static QSet<int> namesToIndices(const QSet<QString> &wells) {
    QSet<int> out;
    for (const auto &w : wells) {
        int idx = wellNameToIndex96(w);
        if (idx >= 1) out.insert(idx);
    }
    return out;
}

// ---------- Volume rounding (always round UP to 0.1 µL) ----------
static inline double roundUp01(double v) {
    if (v <= 0.0) return 0.0;
    return std::ceil(v * 10.0) / 10.0; // e.g., 60.52 -> 60.6
}

// For Fluent: Generate A;/D; lines (one A then one D)
static void appendADFluent(QStringList &out,
                           const QString &srcLabel, int srcPos,
                           const QString &dstLabel, int dstPos,
                           double volUL, const QString &liqClass)
{
    const double vUp = roundUp01(volUL);
    const QString vStr = QString::number(vUp, 'f', 1);
    out << QString("A;%1;;;%2;;%3;%4")
               .arg(srcLabel)
               .arg(srcPos)
               .arg(vStr)
               .arg(liqClass);
    out << QString("D;%1;;;%2;;%3;%4")
               .arg(dstLabel)
               .arg(dstPos)
               .arg(vStr)
               .arg(liqClass);
}

// One-shot: A; + D; + W; (close immediately)
static void appendADFluentOneShot(QStringList &out,
                                  const QString &srcLabel, int srcPos,
                                  const QString &dstLabel, int dstPos,
                                  double volUL, const QString &liqClass)
{
    appendADFluent(out, srcLabel, srcPos, dstLabel, dstPos, volUL, liqClass);
    out << "W;";
}

// One aspirate, many dispenses (for DMSO multi-dispense)
static void appendAThenManyD(QStringList &out,
                             const QString &srcLabel, int srcPos,
                             const QString &dstLabel, const QList<int> &dstPositions,
                             double perWellUL, const QString &liqClass)
{
    if (dstPositions.isEmpty()) return;
    const double vPer = roundUp01(perWellUL);
    if (vPer <= 0) return;

    const double vTotal = roundUp01(vPer * dstPositions.size());
    const QString totStr = QString::number(vTotal, 'f', 1);
    const QString perStr = QString::number(vPer,   'f', 1);

    out << QString("A;%1;;;%2;;%3;%4")
               .arg(srcLabel).arg(srcPos).arg(totStr).arg(liqClass);

    for (int pos : dstPositions) {
        out << QString("D;%1;;;%2;;%3;%4")
        .arg(dstLabel).arg(pos).arg(perStr).arg(liqClass);
    }
}

// number_of_dilutions default 3
static int numberOfDilutionsFromJson(const QJsonObject &exp) {
    const auto arr = exp.value("test_requests").toArray();
    if (arr.isEmpty()) return 3;
    const auto tr0 = arr.at(0).toObject();
    bool ok=false;
    int n = tr0.value("number_of_dilutions").toString().toInt(&ok);
    if (!ok) n = tr0.value("number_of_dilutions").toInt();
    return (n > 0 ? n : 3);
}

// pull double (supports string)
static double readDouble(const QJsonObject &o, const char *key, double def = 0.0) {
    double v = o.value(key).toDouble(def);
    if (v == 0.0) {
        bool ok=false;
        const QString s = o.value(key).toString();
        if (!s.isEmpty()) {
            v = s.toDouble(&ok);
            if (!ok) v = def;
        }
    }
    return v;
}

// Build standard chains from layout (contiguous "Standard" along +8 or +1)
static QVector<QStringList> buildStandardChainsFromLayout(const QJsonObject &wellsObj)
{
    QSet<QString> stdSet;
    for (auto it = wellsObj.begin(); it != wellsObj.end(); ++it) {
        if (it.value().toString().trimmed().compare("Standard", Qt::CaseInsensitive)==0)
            stdSet.insert(normWell(it.key()));
    }
    if (stdSet.isEmpty()) return {};

    QStringList sorted = QStringList(stdSet.values());
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b){
        return wellNameToIndex96(a) < wellNameToIndex96(b);
    });

    auto isStd = [&](const QString &w)->bool { return stdSet.contains(normWell(w)); };

    QSet<QString> visited;
    QVector<QStringList> chains;

    for (const QString &start : sorted) {
        if (visited.contains(start)) continue;

        const int startIdx = wellNameToIndex96(start);
        const QString prev8 = indexToWellName96(startIdx - 8);
        const QString prev1 = indexToWellName96(startIdx - 1);
        if (isStd(prev8) || isStd(prev1)) continue; // not a chain start

        int step = 0;
        if (isStd(indexToWellName96(startIdx + 8))) step = 8;
        else if (isStd(indexToWellName96(startIdx + 1))) step = 1;

        QStringList chain; chain << start; visited.insert(start);
        if (step != 0) {
            int cur = startIdx;
            while (true) {
                const int nxt = cur + step;
                const QString wn = indexToWellName96(nxt);
                if (!isStd(wn)) break;
                chain << wn;
                visited.insert(wn);
                cur = nxt;
            }
        }
        chains.push_back(chain);
    }
    return chains;
}

// ---------------- Plate map rows & CSV rendering -------------------------

struct DaughterPlateEntry {
    QString containerBarcode;
    QString sampleAlias;
    QString wellA01;
    double  volumeUL = 0.0;
    QString volumeUnit = "ul";
    double  conc = 0.0;
    QString concUnit;
    QString u1, u2, u3, u4, u5;
};

static QStringList renderPlateMapCSV(const QList<DaughterPlateEntry> &rows) {
    QStringList L;
    L << "Containerbarcode,Samplealias,Containerposition,Volume,VolumeUnit,Concentration,ConcentrationUnit,UserdefValue1,UserdefValue2,UserdefValue3,UserdefValue4,UserdefValue5";
    for (const auto &r : rows) {
        const QString volStr = QString::number(roundUp01(r.volumeUL), 'f', 1);
        L << QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12")
                 .arg(r.containerBarcode,
                      r.sampleAlias,
                      r.wellA01,
                      volStr,
                      r.volumeUnit,
                      QString::number(r.conc, 'f', 4),
                      r.concUnit,
                      r.u1, r.u2, r.u3, r.u4, r.u5);
    }
    return L;
}

// ---------------------- Audit CSV rows & renderers ------------------------

struct SeedAuditRow {
    QString daughterBarcode;   // "Daughter_1"
    QString analyte;           // compound name or standard name
    QString matrixBarcode;     // matrix barcode (or standard matrix barcode)
    QString matrixWell;        // e.g. "E12"
    QString startWell;         // e.g. "B1"
    double  seedVolumeUL = 0;  // rounded
    QString notes;             // "compound" or "standard"
};

struct DilutionAuditRow {
    QString daughterBarcode;   // "Daughter_1"
    QString analyte;           // compound name or standard name
    QString srcWell;           // e.g. "B1"
    QString dstWell;           // e.g. "B2"
    double  transferUL = 0;    // rounded
    QString notes;             // "compound" or "standard"
};

static QStringList renderSeedAuditCSV(const QList<SeedAuditRow>& rows) {
    QStringList L;
    L << "Daughter,Analyte,MatrixBarcode,MatrixWell,StartWell,SeedVolume_uL,Notes";
    for (const auto& r : rows) {
        L << QString("%1,%2,%3,%4,%5,%6,%7")
        .arg(r.daughterBarcode,
             r.analyte,
             r.matrixBarcode,
             toA01(r.matrixWell),
             toA01(r.startWell),
             QString::number(roundUp01(r.seedVolumeUL), 'f', 1),
             r.notes);
    }
    return L;
}

static QStringList renderDilutionAuditCSV(const QList<DilutionAuditRow>& rows) {
    QStringList L;
    L << "Daughter,Analyte,From,To,Transfer_uL,Notes";
    for (const auto& r : rows) {
        L << QString("%1,%2,%3,%4,%5,%6")
        .arg(r.daughterBarcode,
             r.analyte,
             toA01(r.srcWell),
             toA01(r.dstWell),
             QString::number(roundUp01(r.transferUL), 'f', 1),
             r.notes);
    }
    return L;
}

// -------- Direction parsing (LTR default) --------
static bool dmsoDispenseRightToLeft(const QJsonObject& exp) {
    const QString s = exp.value("dmso_direction").toString().trimmed().toLower();
    if (s == "ltr" || s == "left-to-right" || s == "left" || s == "0") return false;
    if (s == "rtl" || s == "right-to-left" || s == "right" || s == "1") return true;
    // Default: LTR
    return false;
}


// Row/Col from 1..96 index
static inline int rowFromIndex96(int idx) { return (idx - 1) % 8; }     // 0..7 (A..H)
static inline int colFromIndex96(int idx) { return (idx - 1) / 8 + 1; } // 1..12

// One aspirate, many dispenses (varying per-dispense volumes)
static void appendAThenManyD_Vary(QStringList &out,
                                  const QString &srcLabel, int srcPos,
                                  const QString &dstLabel,
                                  const QList<QPair<int,double>> &posVols, // [(destIdx, volUL_rounded)]
                                  const QString &liqClass)
{
    // Sum of already-rounded per-dispense volumes so A; matches D; exactly
    double total = 0.0;
    for (const auto &pv : posVols) total += pv.second;

    const QString vStr = QString::number(roundUp01(total), 'f', 1);
    out << QString("A;%1;;;%2;;%3;%4").arg(srcLabel).arg(srcPos).arg(vStr).arg(liqClass);

    for (const auto &pv : posVols) {
        const QString dVol = QString::number(pv.second, 'f', 1);
        out << QString("D;%1;;;%2;;%3;%4").arg(dstLabel).arg(pv.first).arg(dVol).arg(liqClass);
    }
}


} // namespace

// ========================== Standards Matrix Loading ==========================

bool GWLGenerator::loadStandardsMatrix(QVector<StandardSource>& standards, QString* err) const
{
    QFile f(":/data/resources/data/standards_matrix.json");
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = "Cannot open standards_matrix.json";
        return false;
    }

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) {
        if (err) *err = "standards_matrix.json is not an array";
        return false;
    }

    const auto arr = doc.array();
    for (const auto& val : arr) {
        const auto obj = val.toObject();
        StandardSource src;
        src.barcode = obj.value("Containerbarcode").toString();
        src.well = normWell(obj.value("Containerposition").toString());
        src.sampleAlias = obj.value("Samplealias").toString();
        src.solutionId = obj.value("invenesis_solution_ID").toString();

        const auto concVal = obj.value("Concentration");
        if (concVal.isString()) {
            bool ok = false;
            src.concentration = concVal.toString().toDouble(&ok);
            if (!ok) src.concentration = 0.0;
        } else {
            src.concentration = concVal.toDouble();
        }

        src.concentrationUnit = obj.value("ConcentrationUnit").toString();

        if (src.concentrationUnit.compare("mM", Qt::CaseInsensitive) == 0) {
            src.concentration *= 1000.0;
            src.concentrationUnit = "uM";
        } else if (src.concentrationUnit.compare("ppm", Qt::CaseInsensitive) == 0) {
            continue;
        }

        standards.push_back(src);
    }

    return true;
}

GWLGenerator::StandardSource GWLGenerator::selectBestStandard(
    const QString& standardName,
    double targetConc,
    const QVector<StandardSource>& available) const
{
    StandardSource best;
    double bestScore = 1e300;

    for (const auto& src : available) {
        if (src.sampleAlias.compare(standardName, Qt::CaseInsensitive) != 0)
            continue;
        if (src.concentration <= 0.0)
            continue;

        double score = 0.0;
        if (src.concentration >= targetConc) {
            score = src.concentration / targetConc;
            if (score > 100.0) score = 100.0 + (score - 100.0) * 0.1;
        } else {
            score = 1000.0 + (targetConc / src.concentration);
        }

        if (score < bestScore) {
            bestScore = score;
            best = src;
        }
    }

    return best;
}

// ========================== Fluent backend ================================

bool GWLGenerator::FluentBackend::generate(const QJsonObject &exp,
                                           QVector<FileOut> &outs,
                                           QString *err) const
{
    if (outer_.instrument_ != Instrument::FLUENT1080) {
        if (err) *err = "Fluent backend selected for non-Fluent instrument.";
        return false;
    }

    const QJsonArray plates = exp.value("daughter_plates").toArray();
    if (plates.isEmpty()) {
        if (err) *err = QObject::tr("No daughter_plates in JSON.");
        return false;
    }

    // ---- Audit collectors (across ALL daughter plates) ----
    QList<SeedAuditRow>     seedAudit;
    QList<DilutionAuditRow> dilutionAudit;

    // Load standards matrix
    QVector<StandardSource> availableStandards;
    QString stdErr;
    if (!outer_.loadStandardsMatrix(availableStandards, &stdErr)) {
        qWarning() << "[WARN] Failed to load standards matrix:" << stdErr;
    }

    // Standard info
    const QJsonObject stdObj = exp.value("standard").toObject();
    QString stdName = stdObj.value("Samplealias").toString();

    StandardSource selectedStandard;
    bool useMatrixStandard = false;

    if (!stdName.isEmpty() && !availableStandards.isEmpty()) {
        double targetStdConc = 20000.0;

        const QJsonArray testRequests = exp.value("test_requests").toArray();
        if (!testRequests.isEmpty()) {
            const auto tr0 = testRequests.at(0).toObject();
            double startConc = readDouble(tr0, "starting_concentration", 100.0);
            const QString startUnit = tr0.value("starting_concentration_unit").toString();
            if (startUnit.compare("mM", Qt::CaseInsensitive) == 0) {
                startConc *= 1000.0;
            }
            targetStdConc = startConc * 10.0;
        }

        selectedStandard = outer_.selectBestStandard(stdName, targetStdConc, availableStandards);
        if (!selectedStandard.barcode.isEmpty()) {
            useMatrixStandard = true;
            qDebug() << "[INFO] Selected standard:" << selectedStandard.sampleAlias
                     << "at" << selectedStandard.concentration << "µM"
                     << "from" << selectedStandard.barcode << selectedStandard.well;
        }
    }

    QString stdBarcode = useMatrixStandard ? selectedStandard.barcode
                                           : stdObj.value("Containerbarcode").toString();
    QString stdSrcWell = useMatrixStandard ? selectedStandard.well
                                           : normWell(stdObj.value("Containerposition").toString());
    double stdConc = useMatrixStandard ? selectedStandard.concentration
                                       : readDouble(stdObj, "Concentration", 20000.0);

    const int stdSrcPos = wellNameToIndex96(stdSrcWell);
    const QString standardMatrixLabel = "Standard_Matrix";

    const double df          = (outer_.dilutionFactor_ > 0.0) ? outer_.dilutionFactor_ : 3.16;
    const QString testId     = outer_.testId_;
    const double stockMicroM = outer_.stockConc_;

    // Volume plans
    VolumePlanEntry vpe;
    QString verr;
    if (!outer_.loadVolumePlan(testId, stockMicroM, &vpe, &verr)) {
        qWarning() << "[WARN][FluentBackend] volume plan:" << verr;
        vpe.volMother = 30.0;
        vpe.dmso = 0.0;
    }

    VolumePlanEntry stdVpe = vpe;
    if (useMatrixStandard && !testId.isEmpty()) {
        VolumePlanEntry tempVpe;
        if (outer_.loadVolumePlan(testId, stdConc, &tempVpe, &verr)) {
            stdVpe = tempVpe;
            qDebug() << "[INFO] Using specific volume plan for standard at" << stdConc << "µM";
        }
    }

    // Round up volumes to 0.1 µL
    const double volMother   = roundUp01(vpe.volMother);
    const double dmsoStart   = roundUp01(vpe.dmso);
    const double transferVol = roundUp01(df > 0.0 ? vpe.volMother / df : 0.0);
    const double dmsoDilute  = roundUp01(transferVol > 0.0 ? vpe.volMother - transferVol : 0.0);

    const double stdVolMother   = roundUp01(stdVpe.volMother);
    const double stdDmsoStart   = roundUp01(stdVpe.dmso);
    const double stdTransferVol = roundUp01(df > 0.0 ? stdVpe.volMother / df : 0.0);
    const double stdDmsoDilute  = roundUp01(stdTransferVol > 0.0 ? stdVpe.volMother - stdTransferVol : 0.0);

    // Build compound index map (name -> matrix barcode + well)
    struct SrcLoc { QString barcode; QString well; };
    QMap<QString, SrcLoc> cmpIndex;
    for (const auto &v : exp.value("compounds").toArray()) {
        const auto o = v.toObject();
        const QString name = o.value("product_name").toString().trimmed();
        const QString bc   = o.value("container_id").toString().trimmed();
        const QString w    = normWell(o.value("well_id").toString().trimmed());
        if (!name.isEmpty() && !bc.isEmpty() && !w.isEmpty()) {
            cmpIndex.insert(name, SrcLoc{bc, w});
        }
    }

    // Matrix plate maps generation helper
    auto produceMatrixPlateMaps = [&](QVector<FileOut> &outVec) {
        QMap<QString, QList<DaughterPlateEntry>> rowsByBarcode;
        const QString projectCode = exp.value("project_code").toString();

        for (const auto &v : exp.value("compounds").toArray()) {
            const auto o = v.toObject();
            DaughterPlateEntry r;
            r.containerBarcode = o.value("container_id").toString();
            r.sampleAlias      = o.value("product_name").toString();
            r.wellA01          = toA01(o.value("well_id").toString());
            r.volumeUL         = roundUp01(readDouble(o, "weight", 0.0));
            r.volumeUnit       = o.value("weight_unit").toString("uL");
            r.conc             = readDouble(o, "concentration", 0.0);
            r.concUnit         = o.value("concentration_unit").toString("uM");
            r.u1 = QString("%1_%2").arg(r.containerBarcode, r.wellA01);
            r.u2 = o.value("invenesis_solution_id").toString();
            r.u3 = QString::number(r.conc, 'f', 4);
            r.u4 = QString::number(readDouble(exp.value("test_requests").toArray().at(0).toObject(), "dilution_steps", 0.0));
            r.u5 = projectCode;
            rowsByBarcode[r.containerBarcode].push_back(r);
        }

        for (auto it = rowsByBarcode.cbegin(); it != rowsByBarcode.cend(); ++it) {
            FileOut fcsv;
            fcsv.relativePath = QString("PlateMapHitLW/%1.csv").arg(it.key());
            fcsv.lines = renderPlateMapCSV(it.value());
            fcsv.isAux = true;
            outVec.push_back(std::move(fcsv));
        }
    };

    // Daughter plate map CSV generation helper
    auto produceDaughterPlateMap = [&](int di,
                                       const QJsonObject &plate,
                                       const QList<QPair<QString, QString>> &placedStart,
                                       const QMap<QString,int> &perHitStep) -> FileOut {
        const QString dghtBarcode = QString("Daughter_%1").arg(di+1);
        const QString projectCode = exp.value("project_code").toString();
        const QJsonObject wells   = plate.value("wells").toObject();
        const int nDil = std::max(1, numberOfDilutionsFromJson(exp));

        auto labelOf = [&](const QString& wn)->QString {
            return wells.value(wn).toString().trimmed();
        };

        QList<DaughterPlateEntry> rows;

        // Standard wells (layout-driven)
        QSet<QString> stdSet;
        for (auto it = wells.begin(); it != wells.end(); ++it)
            if (it.value().toString().trimmed().compare("Standard", Qt::CaseInsensitive)==0)
                stdSet.insert(normWell(it.key()));

        // split into chains
        QStringList sortedStd = QStringList(stdSet.values());
        std::sort(sortedStd.begin(), sortedStd.end(), [](const QString &a, const QString &b){
            return wellNameToIndex96(a) < wellNameToIndex96(b);
        });
        auto isStd = [&](const QString &w)->bool { return stdSet.contains(normWell(w)); };
        QSet<QString> visitedStd;
        for (const QString &start : sortedStd) {
            if (visitedStd.contains(start)) continue;
            const int startIdx = wellNameToIndex96(start);
            const QString prev8 = indexToWellName96(startIdx - 8);
            const QString prev1 = indexToWellName96(startIdx - 1);
            if (isStd(prev8) || isStd(prev1)) continue;

            int step = 0;
            if (isStd(indexToWellName96(startIdx + 8))) step = 8;
            else if (isStd(indexToWellName96(startIdx + 1))) step = 1;

            QStringList chain; chain << start; visitedStd.insert(start);
            if (step != 0) {
                int cur = startIdx;
                while (true) {
                    const int nxt = cur + step;
                    const QString wn = indexToWellName96(nxt);
                    if (!isStd(wn)) break;
                    chain << wn;
                    visitedStd.insert(wn);
                    cur = nxt;
                }
            }

            for (int i=0;i<chain.size();++i) {
                DaughterPlateEntry r;
                r.containerBarcode = dghtBarcode;
                r.sampleAlias      = (i==0 ? stdName : QString("%1_dil").arg(stdName));
                r.wellA01          = toA01(chain.at(i));
                r.volumeUL         = stdVolMother;
                r.volumeUnit       = "ul";
                r.conc             = 0.0; r.concUnit = "uM";
                r.u1 = QString("%1_%2").arg(r.containerBarcode, r.wellA01);
                r.u2 = useMatrixStandard ? selectedStandard.solutionId : stdObj.value("invenesis_solution_ID").toString();
                r.u3 = "0";
                r.u4 = QString::number(readDouble(exp.value("test_requests").toArray().at(0).toObject(), "dilution_steps", 0.0));
                r.u5 = projectCode;
                rows.push_back(r);
            }
        }

        // Compounds + same-label dilutions (for CSV completeness)
        QSet<QString> seenStart;
        for (const auto &p : placedStart) {
            const QString &name = p.first;
            const QString  dst  = normWell(p.second);
            if (seenStart.contains(dst)) continue;
            seenStart.insert(dst);

            DaughterPlateEntry r0;
            r0.containerBarcode = dghtBarcode;
            r0.sampleAlias = name;
            r0.wellA01     = toA01(dst);
            r0.volumeUL    = volMother;
            r0.volumeUnit  = "ul";
            r0.conc        = 0.0; r0.concUnit = "uM";
            r0.u1 = QString("%1_%2").arg(r0.containerBarcode, r0.wellA01);
            r0.u2 = cmpIndex.value(name).barcode;
            r0.u3 = "0";
            r0.u4 = QString::number(readDouble(exp.value("test_requests").toArray().at(0).toObject(), "dilution_steps", 0.0));
            r0.u5 = projectCode;
            rows.push_back(r0);

            const int step = perHitStep.value(dst, 0);
            if (step == 0) continue;

            const QString lab = wells.value(dst).toString().trimmed();
            int cur = wellNameToIndex96(dst);
            for (int s=1;s<nDil;++s) {
                const int nxt = cur + step;
                if (nxt < 1 || nxt > 96) break;
                const QString wn = indexToWellName96(nxt);
                if (wells.value(wn).toString().trimmed() != lab) break;

                DaughterPlateEntry rd;
                rd.containerBarcode = dghtBarcode;
                rd.sampleAlias = QString("%1_dil%2").arg(name).arg(s);
                rd.wellA01     = toA01(wn);
                rd.volumeUL    = volMother;
                rd.volumeUnit  = "ul";
                rd.conc        = 0.0; rd.concUnit = "uM";
                rd.u1 = QString("%1_%2").arg(rd.containerBarcode, rd.wellA01);
                rd.u2 = r0.u2;
                rd.u3 = "0";
                rd.u4 = r0.u4;
                rd.u5 = r0.u5;
                rows.push_back(rd);

                cur = nxt;
            }
        }

        // DMSO controls
        for (auto it = wells.begin(); it != wells.end(); ++it) {
            if (it.value().toString().trimmed().compare("DMSO", Qt::CaseInsensitive) == 0) {
                DaughterPlateEntry rc;
                rc.containerBarcode = dghtBarcode;
                rc.sampleAlias      = "DMSO";
                rc.wellA01          = toA01(normWell(it.key()));
                rc.volumeUL         = volMother;
                rc.volumeUnit       = "ul";
                rc.conc             = 100.0;
                rc.concUnit         = "%";
                rc.u1 = QString("%1_%2").arg(rc.containerBarcode, rc.wellA01);
                rc.u2 = "DMSO";
                rc.u3 = "100";
                rc.u4 = QString::number(readDouble(exp.value("test_requests").toArray().at(0).toObject(), "dilution_steps", 0.0));
                rc.u5 = projectCode;
                rows.push_back(rc);
            }
        }

        FileOut fcsv;
        fcsv.relativePath = QString("PlateMapHitLW/%1.csv").arg(QString("Daughter_%1").arg(di+1));
        fcsv.lines = renderPlateMapCSV(rows);
        fcsv.isAux = true;
        return fcsv;
    };

    // ---- Process each daughter plate ----
    for (int di = 0; di < plates.size(); ++di) {
        const QJsonObject plate  = plates.at(di).toObject();
        const QJsonObject wells  = plate.value("wells").toObject();
        const QString dghtLabel  = QString("Daughter[%1]").arg(QString("%1").arg(di+1, 3, 10, QChar('0')));
        const QString dghtBarcodeStr = QString("Daughter_%1").arg(di+1);
        const int nDil = std::max(1, numberOfDilutionsFromJson(exp));

        // Collect hits (compounds)
        struct Hit { QString product; QString dstWell; QString srcBarcode; QString srcWell; };
        QVector<Hit> hits;
        for (auto it = wells.begin(); it != wells.end(); ++it) {
            const QString dstWell = normWell(it.key());
            const QString who = it.value().toString().trimmed();
            if (who.isEmpty() || who.compare("DMSO", Qt::CaseInsensitive)==0 ||
                who.compare("Standard", Qt::CaseInsensitive)==0)
                continue;
            if (!cmpIndex.contains(who)) continue;
            const auto src = cmpIndex.value(who);
            hits.push_back(Hit{who, dstWell, src.barcode, src.well});
        }

        // Group hits by matrix barcode
        QMap<QString, QVector<Hit>> byMatrix;
        for (const auto &h : hits) byMatrix[h.srcBarcode].push_back(h);

        // Build standard chains from layout
        const auto stdChains = buildStandardChainsFromLayout(wells);

        // ------------------ SAME-LABEL chain discovery for compounds ------------------
        QSet<QString> startWells, diluteWells, controlDmsoWells;
        QMap<QString,int> perHitStep;
        QMap<QString, QString> startWell2Product; // normalized start well -> compound name


        auto labelOf = [&](const QString& wn)->QString {
            return wells.value(wn).toString().trimmed();
        };

        QVector<Hit> hitsSorted = hits;
        std::sort(hitsSorted.begin(), hitsSorted.end(), [](const Hit& a, const Hit& b){
            return wellNameToIndex96(a.dstWell) < wellNameToIndex96(b.dstWell);
        });

        QSet<QString> visited;
        for (const auto &h : hitsSorted) {
            const QString start = normWell(h.dstWell);
            if (visited.contains(start)) continue;

            const QString lab = labelOf(start);
            const int startIdx = wellNameToIndex96(start);
            if (startIdx < 1) continue;

            const QString plus8 = indexToWellName96(startIdx + 8);
            const QString plus1 = indexToWellName96(startIdx + 1);
            const bool canAcross = (!plus8.isEmpty() && labelOf(plus8) == lab);
            const bool canDown   = (!plus1.isEmpty() && labelOf(plus1) == lab);

            int step = 0;
            if (canAcross) step = 8;
            else if (canDown) step = 1;

            if (step != 0) {
                const QString prev = indexToWellName96(startIdx - step);
                if (!prev.isEmpty() && labelOf(prev) == lab) {
                    visited.insert(start);
                    continue;
                }
            }

            startWells.insert(start);
            startWell2Product[start] = h.product;
            visited.insert(start);
            perHitStep[start] = step; // 0 if single well

            if (step != 0) {
                int cur = startIdx;
                for (int s=1; s<nDil; ++s) {
                    const int nxt = cur + step;
                    if (nxt < 1 || nxt > 96) break;
                    const QString wn = indexToWellName96(nxt);
                    if (labelOf(wn) != lab) break;
                    diluteWells.insert(wn);
                    visited.insert(wn);
                    cur = nxt;
                }
            }
        }

        // DMSO controls
        for (auto it = wells.begin(); it != wells.end(); ++it)
            if (it.value().toString().trimmed().compare("DMSO", Qt::CaseInsensitive)==0)
                controlDmsoWells.insert(normWell(it.key()));

        // Standards start/dilute wells (from chains)
        QSet<QString> stdStartWells, stdDiluteWells;
        for (const auto &chain : stdChains) {
            if (chain.isEmpty()) continue;
            stdStartWells.insert(chain.first());
            for (int i=1;i<chain.size();++i) stdDiluteWells.insert(chain.at(i));
        }

        // ---- 1) Reagent_distrib.gwl : ROW-WISE single aspirate (S;19, direction toggle) ----
        {
            FileOut fo;
            fo.relativePath = QString("dght_%1/Reagent_distrib.gwl").arg(di);

            QStringList L;
            L << "C;Reagent distribution (DMSO) by row — ONE aspirate, many dispenses per row";
            L << "B;";
            L << "S;19"; // 350 uL tips
            L << "C;Direction toggle via dmso_direction (default LTR)";

            const bool rtl = dmsoDispenseRightToLeft(exp); // false => LTR

            // Build per-row map: row -> (destPos -> per-well volume)
            // Use rounded per-dispense volumes so A; total equals sum(D;)
            QMap<int, QMap<int,double>> row2pos2vol;

            auto addSet = [&](const QSet<QString>& names, double volRaw){
                const double v = roundUp01(volRaw);
                if (v <= 0.0) return;
                for (int idx : namesToIndices(names)) {
                    const int r = rowFromIndex96(idx);
                    // If a position appears twice (shouldn't), volumes add up.
                    row2pos2vol[r][idx] += v;
                }
            };

            const double volDilute = (dmsoDilute > 1e-6 ? dmsoDilute : volMother);
            addSet(startWells,       dmsoStart);
            addSet(diluteWells,      volDilute);
            addSet(controlDmsoWells, volMother);
            addSet(stdStartWells,    stdDmsoStart);
            addSet(stdDiluteWells,   stdDmsoDilute);

            // Emit per row
            for (int r = 0; r < 8; ++r) {
                if (!row2pos2vol.contains(r) || row2pos2vol[r].isEmpty()) continue;

                // Make a sorted list of (destIdx, roundedVol) in column order
                QList<QPair<int,double>> posVols;
                posVols.reserve(row2pos2vol[r].size());
                for (auto it = row2pos2vol[r].cbegin(); it != row2pos2vol[r].cend(); ++it)
                    posVols.push_back({ it.key(), roundUp01(it.value()) });

                std::sort(posVols.begin(), posVols.end(),
                          [&](const QPair<int,double>& a, const QPair<int,double>& b){
                              const int ca = colFromIndex96(a.first);
                              const int cb = colFromIndex96(b.first);
                              return rtl ? (ca > cb) : (ca < cb);
                          });

                // Optional safety: split if total > ~340 µL (350 µL tips)
                const double CHUNK_LIMIT = 340.0;
                QList<QPair<int,double>> chunk;
                double chunkSum = 0.0;

                auto flushChunk = [&](){
                    if (chunk.isEmpty()) return;
                    appendAThenManyD_Vary(L,
                                          QStringLiteral("100ml_Higher"), 1,
                                          dghtLabel,
                                          chunk,
                                          QStringLiteral("DMSO Contact Dry Multi Invenesis"));
                    chunk.clear();
                    chunkSum = 0.0;
                    L << "W;"; // close this aspirate
                };

                for (const auto &pv : posVols) {
                    const double v = pv.second; // already rounded
                    if (chunkSum + v > CHUNK_LIMIT && !chunk.isEmpty()) {
                        flushChunk();
                    }
                    chunk.push_back(pv);
                    chunkSum += v;
                }
                flushChunk(); // last chunk
            }

            L << "B;";
            fo.lines = L;
            outs.push_back(std::move(fo));
        }


        // ---- 2) Matrix compound placement files (first also seeds Standard start) ----
        {
            int mIdx = 0;
            for (auto it = byMatrix.cbegin(); it != byMatrix.cend(); ++it, ++mIdx) {
                const QString matrixBarcode = it.key();
                const QString matrixLabel   = QString("Matrix[%1]").arg(QString("%1").arg(mIdx+1, 3, 10, QChar('0')));
                const QVector<Hit> &mhits   = it.value();

                FileOut fo;
                fo.relativePath = QString("dght_%1/%2.gwl").arg(di).arg(matrixBarcode);

                QStringList L;
                L << QString("C;Place compounds from %1 (barcode %2)")
                         .arg(matrixLabel)
                         .arg(matrixBarcode);
                L << "B;";
                L << "S;7";

                // Seed Standard into start well(s) ONLY in first matrix file — one-shot A/D/W
                if (mIdx == 0 && !stdChains.isEmpty() && !stdBarcode.isEmpty() && stdSrcPos >= 1) {
                    L << QString("C;Standard %1 seeded in start well(s); no serial dilution here").arg(stdName);
                    const double volStartStandard = roundUp01(stdVolMother - stdDmsoStart);
                    if (volStartStandard > 1e-6) {
                        for (const auto &chain : stdChains) {
                            if (chain.isEmpty()) continue;
                            const int startPos = wellNameToIndex96(chain.first());
                            appendADFluentOneShot(L, standardMatrixLabel, stdSrcPos, dghtLabel, startPos,
                                                  volStartStandard, "DMSO Matrix");
                            // Audit: standard seeding
                            SeedAuditRow ar;
                            ar.daughterBarcode = dghtBarcodeStr;
                            ar.analyte         = (stdName.isEmpty() ? "Standard" : stdName);
                            ar.matrixBarcode   = stdBarcode;
                            ar.matrixWell      = stdSrcWell;
                            ar.startWell       = chain.first();              // start well string, e.g. "A1"
                            ar.seedVolumeUL    = volStartStandard;
                            ar.notes           = "standard";
                            seedAudit.push_back(ar);

                        }
                    }
                }

                // Compounds: seed ONLY the starting wells (one-shot per placement)
                const double volCompound = roundUp01(std::max(0.0, volMother - dmsoStart));
                if (volCompound > 1e-6) {
                    // keep only hits whose destination is a starting well
                    QVector<Hit> startSeeds;
                    startSeeds.reserve(mhits.size());
                    for (const auto &h : mhits) {
                        if (startWells.contains(normWell(h.dstWell))) {
                            startSeeds.push_back(h);
                        }
                    }

                    // deterministic order (by destination well index)
                    std::sort(startSeeds.begin(), startSeeds.end(),
                              [](const Hit& a, const Hit& b){
                                  return wellNameToIndex96(a.dstWell) < wellNameToIndex96(b.dstWell);
                              });

                    for (const auto &h : startSeeds) {
                        const int srcPos = wellNameToIndex96(h.srcWell);
                        const int dstPos = wellNameToIndex96(h.dstWell);
                        appendADFluentOneShot(L, matrixLabel, srcPos, dghtLabel, dstPos,
                                              volCompound, "DMSO Matrix");
                        // Audit: compound seeding
                        SeedAuditRow ar;
                        ar.daughterBarcode = dghtBarcodeStr;
                        ar.analyte         = h.product;
                        ar.matrixBarcode   = h.srcBarcode;
                        ar.matrixWell      = h.srcWell;
                        ar.startWell       = h.dstWell;
                        ar.seedVolumeUL    = volCompound;
                        ar.notes           = "compound";
                        seedAudit.push_back(ar);

                    }
                }

                L << "B;";
                fo.lines = L;
                outs.push_back(std::move(fo));
            }
        }

        // ---- 3) serial_dilution.gwl (standards first; then compounds; one tip per chain) ----
        {
            FileOut fo;
            fo.relativePath = QString("dght_%1/serial_dilution.gwl").arg(di);

            QStringList L;
            L << "C;Serial dilutions — standards first, then compounds; one tip per chain (W; between chains)";
            L << "B;";
            L << "S;7"; // 50 uL tips

            auto labelOf = [&](const QString& wn)->QString { return wells.value(wn).toString().trimmed(); };

            auto emitChain = [&](const QList<int>& pos, double volUL){
                if (pos.size() < 2 || roundUp01(volUL) <= 0.0) return;
                for (int i = 0; i + 1 < pos.size(); ++i) {
                    appendADFluent(L, dghtLabel, pos[i], dghtLabel, pos[i+1],
                                   volUL, "DMSO Contact Wet Single Invenesis");
                }
                L << "W;";
            };

            // ---------------- Standards first (sorted by start index) ----------------
            if (stdTransferVol > 1e-6 && !stdChains.isEmpty()) {
                QVector<QStringList> stdSorted = stdChains;
                std::sort(stdSorted.begin(), stdSorted.end(),
                          [&](const QStringList& a, const QStringList& b){
                              return wellNameToIndex96(a.first()) < wellNameToIndex96(b.first());
                          });

                for (const auto& chain : stdSorted) {
                    QList<int> pos;
                    for (const auto& wn : chain) pos.push_back(wellNameToIndex96(wn));
                    emitChain(pos, stdTransferVol);
                    // Audit: standard dilution steps
                    for (int i = 0; i + 1 < pos.size(); ++i) {
                        DilutionAuditRow dr;
                        dr.daughterBarcode = dghtBarcodeStr;
                        dr.analyte         = (stdName.isEmpty() ? "Standard" : stdName);
                        dr.srcWell         = indexToWellName96(pos[i]);
                        dr.dstWell         = indexToWellName96(pos[i+1]);
                        dr.transferUL      = stdTransferVol;
                        dr.notes           = "standard";
                        dilutionAudit.push_back(dr);
                    }

                }
            }

            // ---------------- Compounds (sorted by the first A; index) ----------------
            if (transferVol > 1e-6 && !startWells.isEmpty()) {
                struct CChain { int startIdx; QList<int> pos; };
                QVector<CChain> chains;

                // collect chains from each start
                QList<QString> starts = startWells.values();
                for (const auto& start : starts) {
                    const int step = perHitStep.value(start, 0);
                    if (step == 0) continue; // no dilution chain

                    const QString lab = labelOf(start);
                    QList<int> pos;
                    int cur = wellNameToIndex96(start);
                    pos.push_back(cur);

                    for (int s = 1; s < nDil; ++s) {
                        const int nxt = cur + step;
                        if (nxt < 1 || nxt > 96) break;
                        const QString wn = indexToWellName96(nxt);
                        if (labelOf(wn) != lab) break;
                        pos.push_back(nxt);
                        cur = nxt;
                    }

                    if (pos.size() >= 2) {
                        CChain c; c.startIdx = wellNameToIndex96(start); c.pos = std::move(pos);
                        chains.push_back(std::move(c));
                    }
                }

                // sort by the first aspirate index (strict ascending)
                std::sort(chains.begin(), chains.end(),
                          [](const CChain& a, const CChain& b){ return a.startIdx < b.startIdx; });

                for (const auto& c : chains){

                    emitChain(c.pos, transferVol);
                    // Audit: compound dilution steps
                    const QString startWellName = indexToWellName96(c.startIdx);
                    const QString analyteName   = startWell2Product.value(startWellName, QString("Compound_%1").arg(startWellName));
                    for (int i = 0; i + 1 < c.pos.size(); ++i) {
                        DilutionAuditRow dr;
                        dr.daughterBarcode = dghtBarcodeStr;
                        dr.analyte         = analyteName;
                        dr.srcWell         = indexToWellName96(c.pos[i]);
                        dr.dstWell         = indexToWellName96(c.pos[i+1]);
                        dr.transferUL      = transferVol;
                        dr.notes           = "compound";
                        dilutionAudit.push_back(dr);
                    }

                }
            }

            L << "B;";
            fo.lines = L;
            outs.push_back(std::move(fo));
        }



        // Generate plate maps
        if (di == 0) {
            produceMatrixPlateMaps(outs);
        }
        {
            QList<QPair<QString, QString>> placedStart;
            for (const auto &h : hits) placedStart.append({h.product, h.dstWell});
            outs.push_back(produceDaughterPlateMap(di, plate, placedStart, perHitStep));
        }
    }

    // ---- Export experiment JSON alongside GWLs ----
    {
        FileOut fjson;
        fjson.relativePath = QString("Audit/experiment.json");
        const QString jsonPretty = QString::fromUtf8(QJsonDocument(exp).toJson(QJsonDocument::Indented));
        fjson.lines = jsonPretty.split('\n');
        fjson.isAux = true;
        outs.push_back(std::move(fjson));
    }

    // ---- Export seed volumes audit ----
    if (!seedAudit.isEmpty()) {
        FileOut fseed;
        fseed.relativePath = QString("Audit/SeedVolumes.csv");
        fseed.lines = renderSeedAuditCSV(seedAudit);
        fseed.isAux = true;
        outs.push_back(std::move(fseed));
    }

    // ---- Export dilution steps audit ----
    if (!dilutionAudit.isEmpty()) {
        FileOut fdil;
        fdil.relativePath = QString("Audit/DilutionSteps.csv");
        fdil.lines = renderDilutionAuditCSV(dilutionAudit);
        fdil.isAux = true;
        outs.push_back(std::move(fdil));
    }


    return true;
}

bool GWLGenerator::FluentBackend::generateAux(const QJsonObject &,
                                              QVector<FileOut> &,
                                              QString *) const
{
    return true;
}

// ============================= Shared helpers (class) ============================

QMap<QString, GWLGenerator::CompoundSrc>
GWLGenerator::buildCompoundIndex(const QJsonArray &compounds) const
{
    QMap<QString, CompoundSrc> idx;
    for (const auto &vv : compounds) {
        const auto o    = vv.toObject();
        const QString n = o.value("product_name").toString().trimmed();
        const QString bc= o.value("container_id").toString().trimmed();
        const QString w = o.value("well_id").toString().trimmed();
        if (n.isEmpty() || bc.isEmpty() || w.isEmpty()) continue;
        CompoundSrc s; s.barcode = bc; s.position = tubePosFromWell(w);
        if (s.position > 0) idx.insert(n, s);
    }
    return idx;
}

QVector<GWLGenerator::Hit>
GWLGenerator::collectHitsFromDaughterLayout(const QJsonObject &daughter,
                                            const QMap<QString, CompoundSrc> &cmpIdx) const
{
    QVector<Hit> hits;
    const QJsonObject wells = daughter.value("wells").toObject();

    for (const QString &dstWell : wells.keys()) {
        const QString who = wells.value(dstWell).toString().trimmed();
        if (who.isEmpty() || isStandardLabel(who) || isDMSOLabel(who)) continue;
        if (!cmpIdx.contains(who)) continue;

        Hit h;
        h.dstWell    = dstWell;
        h.dstIdx     = wellToIndex96(dstWell);
        h.srcBarcode = cmpIdx[who].barcode;
        h.srcPos     = cmpIdx[who].position;
        hits.push_back(h);
    }
    std::sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b){
        return a.dstIdx < b.dstIdx;
    });
    return hits;
}

QMap<QString, QVector<GWLGenerator::Hit>>
GWLGenerator::groupHitsByMatrix(const QVector<Hit> &hits) const
{
    QMap<QString, QVector<Hit>> g;
    for (const auto &h : hits) g[h.srcBarcode].push_back(h);
    return g;
}

bool GWLGenerator::loadVolumePlan(const QString &testId,
                                  double stockConc,
                                  VolumePlanEntry *out,
                                  QString *err) const
{
    QFile f(":/data/resources/data/volumeMap.json");
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = "Cannot open volumeMap.json";
        return false;
    }
    const auto doc  = QJsonDocument::fromJson(f.readAll());
    const auto root = doc.object();
    const auto test = root.value(testId).toObject();
    if (test.isEmpty()) { if (err) *err = "Test id not in volumeMap"; return false; }

    QString bestKey;
    double bestDiff = 1e300;
    for (const auto &k : test.keys()) {
        bool ok=false; const double v = k.toDouble(&ok);
        if (!ok) continue;
        const double d = std::fabs(v - stockConc);
        if (d < bestDiff) { bestDiff = d; bestKey = k; }
    }
    const auto arr = test.value(bestKey).toArray();
    if (arr.isEmpty()) { if (err) *err = "Empty volumeMap entry"; return false; }

    const auto o = arr.first().toObject();
    VolumePlanEntry vp;
    vp.volMother  = o.value("VolMother").toDouble();
    vp.dmso       = o.value("DMSO").toDouble();
    vp.volDght    = o.value("volDght").toDouble();
    vp.volFinal   = o.value("volFinal").toDouble();
    vp.finalConc  = o.value("FinalConc").toDouble();
    vp.concMother = o.value("ConcMother").toDouble();
    if (out) *out = vp;
    return true;
}

int GWLGenerator::tubePosFromWell(const QString &well)
{
    if (well.size() < 2) return 0;
    const QChar rowCh = well.at(0).toUpper();
    bool ok=false; int col = well.mid(1).toInt(&ok);
    if (!ok || col < 1 || col > 12) return 0;
    int row = rowCh.unicode() - QChar('A').unicode();
    if (row < 0 || row > 7) return 0;
    return row * 12 + col; // 1..96
}

int GWLGenerator::wellToIndex96(const QString &well)
{
    if (well.size() < 2) return -1;
    const QChar rowCh = well.at(0).toUpper();
    bool ok=false; int col = well.mid(1).toInt(&ok);
    if (!ok || col < 1 || col > 12) return -1;
    int row = rowCh.unicode() - QChar('A').unicode();
    if (row < 0 || row > 7) return -1;
    return (col - 1) * 8 + row + 1; // 1..96
}

bool GWLGenerator::isStandardLabel(const QString &s)
{
    const QString t = s.trimmed().toLower();
    return (t == "standard" || t == "std");
}

bool GWLGenerator::isDMSOLabel(const QString &s)
{
    const QString t = s.trimmed().toLower();
    return (t == "dmso");
}

bool GWLGenerator::saveMany(const QString &rootDir,
                            const QVector<FileOut> &outs,
                            QString *err)
{
    for (const auto &fo : outs) {
        const QString path = QDir(rootDir).filePath(fo.relativePath);
        QDir().mkpath(QFileInfo(path).dir().path());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            if (err) *err = QString("Cannot open %1 for write").arg(path);
            return false;
        }
        QTextStream ts(&f);
        for (const auto &ln : fo.lines) ts << ln << "\n";
    }
    return true;
}
