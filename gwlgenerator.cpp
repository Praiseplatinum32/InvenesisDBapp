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

// Append A;/D; pair
static void appendAD(QStringList &out,
                     const QString &srcLabel, const QString &srcWell,
                     const QString &dstLabel, const QString &dstWell,
                     double volUL, const QString &liqClass)
{
    const QString v = QString::number(volUL, 'f', 2);
    out << QString("A;%1;;;%2;;%3;%4").arg(srcLabel, normWell(srcWell), v, liqClass);
    out << QString("D;%1;;;%2;;%3;%4").arg(dstLabel, normWell(dstWell), v, liqClass);
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

// ----- NEW: build standard chains strictly from the daughter layout -----
// Chains start at any "Standard" well whose predecessor (step −8 or −1) is NOT Standard.
// Step is inferred from the second well: if (start+8) is Standard => step=8;
// else if (start+1) is Standard => step=1; otherwise a single-well chain.
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

    const QJsonObject stdObj = exp.value("standard").toObject();
    const QString stdBarcode = stdObj.value("Containerbarcode").toString();
    const QString stdSrcWell = normWell(stdObj.value("Containerposition").toString());
    const QString stdName    = stdObj.value("Samplealias").toString();

    const double df          = (outer_.dilutionFactor_ > 0.0) ? outer_.dilutionFactor_ : 3.16;
    const QString testId     = outer_.testId_;
    const double stockMicroM = outer_.stockConc_;

    // volume plan
    VolumePlanEntry vpe; QString verr;
    if (!outer_.loadVolumePlan(testId, stockMicroM, &vpe, &verr)) {
        qWarning() << "[WARN][FluentBackend] volume plan:" << verr;
    }
    const double volMother   = vpe.volMother;                     // target µL in daughter wells
    const double dmsoStart   = vpe.dmso;                          // µL DMSO in start well
    const double transferVol = (df > 0.0 ? volMother / df : 0.0); // µL per hop
    const double dmsoDilute  = (transferVol > 0.0 ? volMother - transferVol : 0.0);

    // product_name -> (barcode, well)
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

    // matrix plate maps (CSV)
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

    // Daughter plate map CSV
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
                r.volumeUL         = volMother;
                r.volumeUnit       = "ul";
                r.conc             = 0.0; r.concUnit = "uM";
                r.u1 = QString("%1_%2").arg(r.containerBarcode, r.wellA01);
                r.u2 = stdObj.value("invenesis_solution_ID").toString();
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

    // ---- per daughter plate ----
    for (int di = 0; di < plates.size(); ++di) {
        const QJsonObject plate  = plates.at(di).toObject();
        const QJsonObject wells  = plate.value("wells").toObject();
        const QString dghtLabel  = QString("Daughter[%1]").arg(QString("%1").arg(di+1, 3, 10, QChar('0')));
        const int nDil = std::max(1, numberOfDilutionsFromJson(exp));

        // Hits: compounds from layout
        struct Hit { QString product; QString dstWell; QString srcBarcode; QString srcWell; };
        QVector<Hit> hits;
        for (auto it = wells.begin(); it != wells.end(); ++it) {
            const QString dstWell = normWell(it.key());
            const QString who = it.value().toString().trimmed();
            if (who.isEmpty() || who.compare("DMSO", Qt::CaseInsensitive)==0 || who.compare("Standard", Qt::CaseInsensitive)==0)
                continue;
            if (!cmpIndex.contains(who)) continue;
            const auto src = cmpIndex.value(who);
            hits.push_back(Hit{who, dstWell, src.barcode, src.well});
        }
        // group hits by matrix barcode
        QMap<QString, QVector<Hit>> byMatrix;
        for (const auto &h : hits) byMatrix[h.srcBarcode].push_back(h);

        // Standard chains (now layout-driven, no cap by number_of_dilutions)
        const auto stdChains = buildStandardChainsFromLayout(wells);

        // sets for reagent distribution (excluding standard wells later)
        QSet<QString> startWells, diluteWells, controlDmsoWells;
        QMap<QString,int> perHitStep; // start well -> step

        for (const auto &h : hits) {
            startWells.insert(h.dstWell);
            const int step = pickStepFromLayout(h.dstWell, nDil, wells);
            perHitStep[h.dstWell] = step;
            // build dilution chain
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
        if (!stdChains.isEmpty() && !stdBarcode.isEmpty() && !stdSrcWell.isEmpty()) {
            FileOut fo;
            fo.relativePath = QString("dght_%1/standard.gwl").arg(di);
            QStringList L;
            L << QString("C;Standard %1 (%2) DMSO start (A/D), DMSO dil (R;), pick, serial dil")
                     .arg(stdName, stdBarcode);

            for (const auto &chain : stdChains) {
                if (chain.isEmpty()) continue;
                const QString start = chain.first();

                // 1) A/D DMSO to starting well (S;7)
                if (dmsoStart > 1e-6) {
                    L << "B;";
                    L << "S;7";
                    appendAD(L, "DMSO_25ml", "A1", dghtLabel, start,
                             dmsoStart, "DMSO Contact Dry Multi Invenesis");
                    L << "F;";
                    L << "W;";
                    L << "B;";
                }

                // 2) R; DMSO to dilution wells only (S;19) — chain minus first
                if (chain.size() > 1 && dmsoDilute > 1e-6) {
                    QSet<QString> dilNames;
                    for (int i=1;i<chain.size();++i) dilNames.insert(chain.at(i));
                    L << "S;19";
                    L << buildRLine("100ml_Higher", dghtLabel, namesToIndices(dilNames),
                                    dmsoDilute, "DMSO Contact Dry Multi Invenesis");
                    L << "F;";
                    L << "W;";
                    L << "B;";
                }

                // 3) A/D pick standard to start (S;7)
                const double volStartCompound = std::max(0.0, volMother - dmsoStart);
                if (volStartCompound > 1e-6) {
                    L << "S;7";
                    appendAD(L, "Standard_Matrix", stdSrcWell, dghtLabel, start,
                             volStartCompound, "DMSO Matrix");
                }

                // 4) A/D serial dilution along the full chain (layout-driven)
                if (transferVol > 1e-6 && chain.size() > 1) {
                    for (int i=0; i+1<chain.size(); ++i) {
                        appendAD(L, dghtLabel, chain.at(i), dghtLabel, chain.at(i+1),
                                 transferVol, "DMSO Contact Wet Single Invenesis");
                    }
                }

                // close A/D block
                L << "F;";
                L << "W;";
                L << "B;";
            }

            fo.lines = L;
            outs.push_back(std::move(fo));
        }

        // ---- 2) Reagent_distrib.gwl (exclude all standard wells) ----
        {
            // remove *all* std wells (starts+dilutions) from reagent distribution
            QSet<QString> allStd;
            for (const auto &chain : stdChains)
                for (const auto &w : chain) allStd.insert(w);
            for (const auto &w : allStd) { startWells.remove(w); diluteWells.remove(w); }

            const QSet<int> startIdx = namesToIndices(startWells);

            QSet<QString> dilAndCtrl = diluteWells;
            for (const auto &w : controlDmsoWells) dilAndCtrl.insert(w);
            const QSet<int> dilAndCtrlIdx = namesToIndices(dilAndCtrl);

            FileOut fo;
            fo.relativePath = QString("dght_%1/Reagent_distrib.gwl").arg(di);

            QStringList L;
            L << "C;Reagent distribution (DMSO) for starts, dilutions, control DMSO";
            L << "B;";
            L << "S;19";

            if (!startIdx.isEmpty() && dmsoStart > 1e-6) {
                L << buildRLine("100ml_Higher", dghtLabel, startIdx,
                                dmsoStart, "DMSO Contact Dry Multi Invenesis");
                L << "F;";
            }
            if (!dilAndCtrlIdx.isEmpty()) {
                const double volForThis = (dmsoDilute > 1e-6 ? dmsoDilute : volMother);
                L << buildRLine("100ml_Higher", dghtLabel, dilAndCtrlIdx,
                                volForThis, "DMSO Contact Dry Multi Invenesis");
                L << "F;";
            }

            L << "W;";
            L << "B;";
            fo.lines = L;
            outs.push_back(std::move(fo));
        }

        // ---- 3) per-matrix: place compounds ----
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
                        appendAD(L, matrixLabel, h.srcWell, dghtLabel, h.dstWell,
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

        // ---- 4) serial_dilution.gwl for compounds ----
        {
            FileOut fo;
            fo.relativePath = QString("dght_%1/serial_dilution.gwl").arg(di);

            QStringList L;
            L << "C;Serial dilutions on daughter for all compounds";
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
                        appendAD(L, dghtLabel, indexToWellName96(cur),
                                 dghtLabel, indexToWellName96(nxt),
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

        // ---- Plate maps ----
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
