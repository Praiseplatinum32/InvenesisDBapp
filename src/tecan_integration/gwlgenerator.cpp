#include "gwlgenerator.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
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

// A1..H12 <-> 1..96 (column-major)
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

// Build R; (with excludes)
static QString buildRLine(const QString &srcLabel,
                          const QString &dstLabel,
                          const QSet<int> &targetIdx,
                          double volumeUL,
                          const QString &liquidClass,
                          int ditiReuse = 4,
                          int multiDisp = 12,
                          int direction = 0)
{
    const int srcStart=1, srcEnd=1, destStart=1, destEnd=96;

    QString line = QString("R;%1;;;%2;%3;%4;;;%5;%6;%7;%8;%9")
                       .arg(srcLabel)
                       .arg(srcStart).arg(srcEnd)
                       .arg(dstLabel)
                       .arg(destStart).arg(destEnd)
                       .arg(QString::number(volumeUL, 'f', 2))
                       .arg(liquidClass)
                       .arg(ditiReuse);
    line += QString(";%1;%2").arg(multiDisp).arg(direction);

    QSet<int> all = fullPlate96Indices();
    QList<int> exList = (all - targetIdx).values();
    std::sort(exList.begin(), exList.end());
    for (int p : exList) line += ";" + QString::number(p);
    return line;
}

// For Fluent: Generate A;/D; lines with numeric positions ONLY
static void appendADFluent(QStringList &out,
                           const QString &srcLabel, int srcPos,
                           const QString &dstLabel, int dstPos,
                           double volUL, const QString &liqClass)
{
    const QString v = QString::number(volUL, 'f', 2);
    out << QString("A;%1;;;%2;;%3;%4").arg(srcLabel).arg(srcPos).arg(v, liqClass);
    out << QString("D;%1;;;%2;;%3;%4").arg(dstLabel).arg(dstPos).arg(v, liqClass);
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

// Prefer across row (+8), else down (+1). Validate against layout.
static int pickStepFromLayout(const QString &startWell,
                              int nDil,
                              const QJsonObject &wellsObj)
{
    auto okLine = [&](int step)->bool {
        int curIdx = wellNameToIndex96(startWell);
        if (curIdx < 1) return false;
        for (int s=1; s<nDil; ++s) {
            const int nxt = curIdx + step;
            if (nxt < 1 || nxt > 96) return false;
            const QString w = indexToWellName96(nxt);
            const QString lab = wellsObj.value(w).toString().trimmed();
            if (!(lab.isEmpty()
                  || lab.compare("DMSO", Qt::CaseInsensitive) == 0
                  || lab.compare("Standard", Qt::CaseInsensitive) == 0))
                return false;
            curIdx = nxt;
        }
        return true;
    };

    if (okLine(8)) return 8;
    if (okLine(1)) return 1;

    const int startIdx = wellNameToIndex96(startWell);
    const int roomAcross = (96 - startIdx) / 8;
    const int rowInCol   = (startIdx - 1) % 8;
    const int roomDown   = 7 - rowInCol;
    return (roomAcross >= roomDown) ? 8 : 1;
}

// Build standard chains from layout
static QVector<QStringList> buildStandardChainsFromLayout(const QJsonObject &wellsObj)
{
    // collect all "Standard" wells
    QSet<QString> stdSet;
    for (auto it = wellsObj.begin(); it != wellsObj.end(); ++it) {
        if (it.value().toString().trimmed().compare("Standard", Qt::CaseInsensitive)==0)
            stdSet.insert(normWell(it.key()));
    }
    // nothing to do
    if (stdSet.isEmpty()) return {};

    // sorted order for stable output
    QStringList sorted = QStringList(stdSet.values());
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b){
        return wellNameToIndex96(a) < wellNameToIndex96(b);
    });

    // helper to know if a well in layout is Standard
    auto isStd = [&](const QString &w)->bool { return stdSet.contains(normWell(w)); };

    QSet<QString> visited;
    QVector<QStringList> chains;

    for (const QString &start : sorted) {
        if (visited.contains(start)) continue;

        // detect if this is a true chain start (previous along +8/+1 not Standard)
        const int startIdx = wellNameToIndex96(start);
        const QString prev8 = indexToWellName96(startIdx - 8);
        const QString prev1 = indexToWellName96(startIdx - 1);
        if (isStd(prev8) || isStd(prev1)) continue; // not a start

        // infer step
        int step = 0;
        if (isStd(indexToWellName96(startIdx + 8))) step = 8;
        else if (isStd(indexToWellName96(startIdx + 1))) step = 1;

        // build chain
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

// plate maps rows ----------------------------------------------------------

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
        L << QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12")
        .arg(r.containerBarcode,
             r.sampleAlias,
             r.wellA01,
             QString::number(r.volumeUL, 'f', 2),
             r.volumeUnit,
             QString::number(r.conc, 'f', 4),
             r.concUnit,
             r.u1, r.u2, r.u3, r.u4, r.u5);
    }
    return L;
}

} // namespace

// ========================== Standards Matrix Loading ==========================

bool GWLGenerator::loadStandardsMatrix(QVector<StandardSource>& standards, QString* err) const
{
    QFile f(":/standardjson/jsonfile/standards_matrix.json");
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

        // Handle concentration - could be string or number
        const auto concVal = obj.value("Concentration");
        if (concVal.isString()) {
            bool ok = false;
            src.concentration = concVal.toString().toDouble(&ok);
            if (!ok) src.concentration = 0.0;
        } else {
            src.concentration = concVal.toDouble();
        }

        src.concentrationUnit = obj.value("ConcentrationUnit").toString();

        // Convert to µM if needed
        if (src.concentrationUnit.compare("mM", Qt::CaseInsensitive) == 0) {
            src.concentration *= 1000.0;
            src.concentrationUnit = "uM";
        } else if (src.concentrationUnit.compare("ppm", Qt::CaseInsensitive) == 0) {
            // Skip ppm entries for now or convert if molecular weight is known
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
        // Match by name (case insensitive)
        if (src.sampleAlias.compare(standardName, Qt::CaseInsensitive) != 0)
            continue;

        // Skip if no valid concentration
        if (src.concentration <= 0.0)
            continue;

        // Prefer concentrations that are higher than target (for dilution)
        // but not excessively high
        double score = 0.0;
        if (src.concentration >= targetConc) {
            // Slightly prefer higher concentrations for dilution
            score = src.concentration / targetConc;
            if (score > 100.0) score = 100.0 + (score - 100.0) * 0.1; // Penalize very high ratios
        } else {
            // Can still use lower concentrations but with penalty
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

    // Load standards matrix
    QVector<StandardSource> availableStandards;
    QString stdErr;
    if (!outer_.loadStandardsMatrix(availableStandards, &stdErr)) {
        qWarning() << "[WARN] Failed to load standards matrix:" << stdErr;
    }

    // Get standard info from experiment
    const QJsonObject stdObj = exp.value("standard").toObject();
    QString stdName = stdObj.value("Samplealias").toString();

    // If standard is specified in experiment but not with full details,
    // try to find it in standards_matrix
    StandardSource selectedStandard;
    bool useMatrixStandard = false;

    if (!stdName.isEmpty() && !availableStandards.isEmpty()) {
        // Determine target concentration for standard
        double targetStdConc = 20000.0; // Default high concentration in µM

        // Check if we have a specific concentration requirement
        const QJsonArray testRequests = exp.value("test_requests").toArray();
        if (!testRequests.isEmpty()) {
            const auto tr0 = testRequests.at(0).toObject();
            double startConc = readDouble(tr0, "starting_concentration", 100.0);
            const QString startUnit = tr0.value("starting_concentration_unit").toString();
            if (startUnit.compare("mM", Qt::CaseInsensitive) == 0) {
                startConc *= 1000.0;
            }
            // For standards, we might want higher concentration than compound start
            targetStdConc = startConc * 10.0; // 10x higher than compound start
        }

        selectedStandard = outer_.selectBestStandard(stdName, targetStdConc, availableStandards);
        if (!selectedStandard.barcode.isEmpty()) {
            useMatrixStandard = true;
            qDebug() << "[INFO] Selected standard:" << selectedStandard.sampleAlias
                     << "at" << selectedStandard.concentration << "µM"
                     << "from" << selectedStandard.barcode << selectedStandard.well;
        }
    }

    // Fallback to experiment-provided standard if no matrix match
    QString stdBarcode = useMatrixStandard ? selectedStandard.barcode
                                           : stdObj.value("Containerbarcode").toString();
    QString stdSrcWell = useMatrixStandard ? selectedStandard.well
                                           : normWell(stdObj.value("Containerposition").toString());
    double stdConc = useMatrixStandard ? selectedStandard.concentration
                                       : readDouble(stdObj, "Concentration", 20000.0);

    // Convert standard source well to numeric position
    const int stdSrcPos = wellNameToIndex96(stdSrcWell);

    // Ensure standard matrix has proper label
    const QString standardMatrixLabel = "Standard_Matrix";

    const double df          = (outer_.dilutionFactor_ > 0.0) ? outer_.dilutionFactor_ : 3.16;
    const QString testId     = outer_.testId_;
    const double stockMicroM = outer_.stockConc_;

    // Load volume plan for compounds
    VolumePlanEntry vpe;
    QString verr;
    if (!outer_.loadVolumePlan(testId, stockMicroM, &vpe, &verr)) {
        qWarning() << "[WARN][FluentBackend] volume plan:" << verr;
        // Use defaults
        vpe.volMother = 30.0;
        vpe.dmso = 0.0;
    }

    // For standards, check if there's a specific volume plan
    VolumePlanEntry stdVpe = vpe; // Start with compound volumes
    if (useMatrixStandard && !testId.isEmpty()) {
        VolumePlanEntry tempVpe;
        if (outer_.loadVolumePlan(testId, stdConc, &tempVpe, &verr)) {
            stdVpe = tempVpe;
            qDebug() << "[INFO] Using specific volume plan for standard at" << stdConc << "µM";
        }
    }

    const double volMother   = vpe.volMother;
    const double dmsoStart   = vpe.dmso;
    const double transferVol = (df > 0.0 ? volMother / df : 0.0);
    const double dmsoDilute  = (transferVol > 0.0 ? volMother - transferVol : 0.0);

    // Standard-specific volumes
    const double stdVolMother   = stdVpe.volMother;
    const double stdDmsoStart   = stdVpe.dmso;
    const double stdTransferVol = (df > 0.0 ? stdVolMother / df : 0.0);
    const double stdDmsoDilute  = (stdTransferVol > 0.0 ? stdVolMother - stdTransferVol : 0.0);

    // Build compound index
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
            r.volumeUL         = readDouble(o, "weight", 0.0);
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

        QList<DaughterPlateEntry> rows;

        // Standard wells (layout-driven)
        QSet<QString> stdSet;
        for (auto it = wells.begin(); it != wells.end(); ++it)
            if (it.value().toString().trimmed().compare("Standard", Qt::CaseInsensitive)==0)
                stdSet.insert(normWell(it.key()));

        // split into chains (same logic as builder)
        QStringList sortedStd = QStringList(stdSet.values());
        std::sort(sortedStd.begin(), sortedStd.end(), [](const QString &a, const QString &b){
            return wellNameToIndex96(a) < wellNameToIndex96(b);
        });
        auto isStd = [&](const QString &w)->bool { return stdSet.contains(normWell(w)); };
        QSet<QString> visited;
        for (const QString &start : sortedStd) {
            if (visited.contains(start)) continue;
            const int startIdx = wellNameToIndex96(start);
            const QString prev8 = indexToWellName96(startIdx - 8);
            const QString prev1 = indexToWellName96(startIdx - 1);
            if (isStd(prev8) || isStd(prev1)) continue;

            int step = 0;
            if (isStd(indexToWellName96(startIdx + 8))) step = 8;
            else if (isStd(indexToWellName96(startIdx + 1))) step = 1;

            // walk full layout chain
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

        // Compounds + their dilutions
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

            const int step = perHitStep.value(dst, 8);
            int cur = wellNameToIndex96(dst);
            for (int s=1;s<nDil;++s) {
                int nxt = cur + step;
                if (nxt < 1 || nxt > 96) break;
                DaughterPlateEntry rd;
                rd.containerBarcode = dghtBarcode;
                rd.sampleAlias = QString("%1_dil%2").arg(name).arg(s);
                rd.wellA01     = toA01(indexToWellName96(nxt));
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

        // Build well sets for reagent distribution
        QSet<QString> startWells, diluteWells, controlDmsoWells;
        QMap<QString,int> perHitStep;

        for (const auto &h : hits) {
            startWells.insert(h.dstWell);
            const int step = pickStepFromLayout(h.dstWell, nDil, wells);
            perHitStep[h.dstWell] = step;

            int cur = wellNameToIndex96(h.dstWell);
            for (int s=1; s<nDil; ++s) {
                int nxt = cur + step;
                if (nxt < 1 || nxt > 96) break;
                diluteWells.insert(indexToWellName96(nxt));
                cur = nxt;
            }
        }

        for (auto it = wells.begin(); it != wells.end(); ++it)
            if (it.value().toString().trimmed().compare("DMSO", Qt::CaseInsensitive)==0)
                controlDmsoWells.insert(normWell(it.key()));

        // ---- 1) standard.gwl ----
        if (!stdChains.isEmpty() && !stdBarcode.isEmpty() && stdSrcPos >= 1) {
            FileOut fo;
            fo.relativePath = QString("dght_%1/standard.gwl").arg(di);
            QStringList L;

            // Add header with standard info
            L << QString("C;Standard %1 (%2) from %3 position %4 at %5 µM")
                     .arg(stdName)
                     .arg(useMatrixStandard ? selectedStandard.solutionId : stdObj.value("invenesis_solution_ID").toString())
                     .arg(stdBarcode)
                     .arg(stdSrcPos)
                     .arg(QString::number(stdConc, 'f', 1));

            L << QString("C;Volume plan: Mother=%1µL, DMSO start=%2µL, Transfer=%3µL, DMSO dilute=%4µL")
                     .arg(QString::number(stdVolMother, 'f', 2))
                     .arg(QString::number(stdDmsoStart, 'f', 2))
                     .arg(QString::number(stdTransferVol, 'f', 2))
                     .arg(QString::number(stdDmsoDilute, 'f', 2));

            for (const auto &chain : stdChains) {
                if (chain.isEmpty()) continue;
                const int startPos = wellNameToIndex96(chain.first());

                // 1) A/D DMSO to starting well if needed
                if (stdDmsoStart > 1e-6) {
                    L << "B;";
                    L << "S;7";
                    appendADFluent(L, "DMSO_25ml", 1, dghtLabel, startPos,
                                   stdDmsoStart, "DMSO Contact Dry Multi Invenesis");
                    L << "F;";
                    L << "W;";
                    L << "B;";
                }

                // 2) Individual A/D DMSO to dilution wells (ONE ASP + excess, multi-dispense)
                if (chain.size() > 1 && stdDmsoDilute > 1e-6) {
                    // Collect all dilution positions
                    QList<int> dilPositions;
                    for (int i = 1; i < chain.size(); ++i) {
                        const int dilPos = wellNameToIndex96(chain.at(i));
                        if (dilPos >= 1) dilPositions.append(dilPos);
                    }

                    if (!dilPositions.isEmpty()) {
                        // Same excess policy as Reagent_distrib.gwl: 15% or >= 30 µL
                        const double totalNeeded   = stdDmsoDilute * static_cast<double>(dilPositions.size());
                        const double excessVolume  = std::max(30.0, totalNeeded * 0.15);
                        const double totalAspirate = totalNeeded + excessVolume;

                        // Ensure S; is preceded by B;
                        L << "B;";
                        L << "S;19";

                        // One aspirate with excess, then multi-dispense
                        L << QString("A;100ml_Higher;;;1;;%1;DMSO Contact Dry Multi Invenesis")
                                 .arg(QString::number(totalAspirate, 'f', 2));

                        for (int dilPos : dilPositions) {
                            L << QString("D;%1;;;%2;;%3;DMSO Contact Dry Multi Invenesis")
                            .arg(dghtLabel)
                                .arg(dilPos)
                                .arg(QString::number(stdDmsoDilute, 'f', 2));
                        }

                        // Finish block so Fluent can change tips if needed
                        L << "F;";
                        L << "W;";
                        L << "B;";
                    }
                }


                // 3) Pick standard to start well
                const double volStartStandard = std::max(0.0, stdVolMother - stdDmsoStart);
                if (volStartStandard > 1e-6) {
                    L << "S;7";
                    appendADFluent(L, standardMatrixLabel, stdSrcPos, dghtLabel, startPos,
                                   volStartStandard, "DMSO Matrix");
                }

                // 4) Serial dilution - using numeric positions
                if (stdTransferVol > 1e-6 && chain.size() > 1) {
                    for (int i=0; i+1<chain.size(); ++i) {
                        const int srcPos = wellNameToIndex96(chain.at(i));
                        const int dstPos = wellNameToIndex96(chain.at(i+1));
                        appendADFluent(L, dghtLabel, srcPos, dghtLabel, dstPos,
                                       stdTransferVol, "DMSO Contact Wet Single Invenesis");
                    }
                }

                L << "F;";
                L << "W;";
                L << "B;";
            }

            fo.lines = L;
            outs.push_back(std::move(fo));
        }

        // ---- 2) Reagent_distrib.gwl ----
        {
            // Exclude standard wells from reagent distribution
            QSet<QString> allStd;
            for (const auto &chain : stdChains)
                for (const auto &w : chain) allStd.insert(w);
            for (const auto &w : allStd) {
                startWells.remove(w);
                diluteWells.remove(w);
            }

            FileOut fo;
            fo.relativePath = QString("dght_%1/Reagent_distrib.gwl").arg(di);

            QStringList L;
            L << "C;Reagent distribution (DMSO) for compound starts, dilutions, and control DMSO wells";
            L << "B;";
            L << "S;19";

            // Group wells by volume for optimized multi-dispense with excess volume

            // 1) DMSO to compound start wells - single aspirate with excess, multiple dispense
            if (!startWells.isEmpty() && dmsoStart > 1e-6) {
                const double totalNeeded = dmsoStart * startWells.size();
                const double excessVolume = std::max(30.0, totalNeeded * 0.15); // 15% excess, minimum 30 µL
                const double totalStartVol = totalNeeded + excessVolume;

                // Single aspirate with excess
                L << QString("A;100ml_Higher;;;1;;%1;DMSO Contact Dry Multi Invenesis")
                         .arg(QString::number(totalStartVol, 'f', 2));

                // Multiple dispenses
                for (const QString &startWell : startWells) {
                    const int pos = wellNameToIndex96(startWell);
                    if (pos >= 1) {
                        L << QString("D;%1;;;%2;;%3;DMSO Contact Dry Multi Invenesis")
                        .arg(dghtLabel)
                            .arg(pos)
                            .arg(QString::number(dmsoStart, 'f', 2));
                    }
                }
            }

            // 2) DMSO to dilution wells - single aspirate with excess, multiple dispense
            const double volForDilution = (dmsoDilute > 1e-6 ? dmsoDilute : volMother);

            if (!diluteWells.isEmpty() && volForDilution > 1e-6) {
                const double totalNeeded = volForDilution * diluteWells.size();
                const double excessVolume = std::max(30.0, totalNeeded * 0.15); // 15% excess, minimum 30 µL
                const double totalDilVol = totalNeeded + excessVolume;

                // Single aspirate with excess
                L << QString("A;100ml_Higher;;;1;;%1;DMSO Contact Dry Multi Invenesis")
                         .arg(QString::number(totalDilVol, 'f', 2));

                // Multiple dispenses
                for (const QString &dilWell : diluteWells) {
                    const int pos = wellNameToIndex96(dilWell);
                    if (pos >= 1) {
                        L << QString("D;%1;;;%2;;%3;DMSO Contact Dry Multi Invenesis")
                        .arg(dghtLabel)
                            .arg(pos)
                            .arg(QString::number(volForDilution, 'f', 2));
                    }
                }
            }

            // 3) DMSO to control wells - single aspirate with excess, multiple dispense
            if (!controlDmsoWells.isEmpty() && volMother > 1e-6) {
                const double totalNeeded = volMother * controlDmsoWells.size();
                const double excessVolume = std::max(30.0, totalNeeded * 0.15); // 15% excess, minimum 30 µL
                const double totalControlVol = totalNeeded + excessVolume;

                // Single aspirate with excess
                L << QString("A;100ml_Higher;;;1;;%1;DMSO Contact Dry Multi Invenesis")
                         .arg(QString::number(totalControlVol, 'f', 2));

                // Multiple dispenses
                for (const QString &dmsoWell : controlDmsoWells) {
                    const int pos = wellNameToIndex96(dmsoWell);
                    if (pos >= 1) {
                        L << QString("D;%1;;;%2;;%3;DMSO Contact Dry Multi Invenesis")
                        .arg(dghtLabel)
                            .arg(pos)
                            .arg(QString::number(volMother, 'f', 2));
                    }
                }
            }

            L << "F;";
            L << "W;";
            L << "B;";
            fo.lines = L;
            outs.push_back(std::move(fo));
        }

        // ---- 3) Matrix compound placement files ----
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
                         .arg(matrixLabel, matrixBarcode);
                L << "B;";
                L << "S;7";

                const double volCompound = std::max(0.0, volMother - dmsoStart);
                if (volCompound > 1e-6) {
                    for (const auto &h : mhits) {
                        // Convert wells to numeric positions
                        const int srcPos = wellNameToIndex96(h.srcWell);
                        const int dstPos = wellNameToIndex96(h.dstWell);
                        appendADFluent(L, matrixLabel, srcPos, dghtLabel, dstPos,
                                       volCompound, "DMSO Matrix");
                    }
                    L << "F;";
                }
                L << "W;";
                L << "B;";
                fo.lines = L;
                outs.push_back(std::move(fo));
            }
        }

        // ---- 4) serial_dilution.gwl ----
        {
            FileOut fo;
            fo.relativePath = QString("dght_%1/serial_dilution.gwl").arg(di);

            QStringList L;
            L << "C;Serial dilutions for all compounds";
            L << "B;";
            L << "S;7";

            if (transferVol > 1e-6) {
                QSet<QString> seen;
                for (const auto &h : hits) {
                    if (seen.contains(h.dstWell)) continue;
                    seen.insert(h.dstWell);
                    const int step = perHitStep.value(h.dstWell, 8);
                    int cur = wellNameToIndex96(h.dstWell);
                    for (int s=1; s<nDil; ++s) {
                        int nxt = cur + step;
                        if (nxt < 1 || nxt > 96) break;
                        // Use numeric positions directly
                        appendADFluent(L, dghtLabel, cur, dghtLabel, nxt,
                                       transferVol, "DMSO Contact Wet Single Invenesis");
                        cur = nxt;
                    }
                }
                L << "F;";
            }
            L << "W;";
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
    QFile f(":/standardjson/jsonfile/volumeMap.json");
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
