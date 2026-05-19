#include "FluentDriver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <cmath>

#include "../services/gwl_helpers.h"
#include "../tecan_integration/gwlgenerator.h"

using FileOut = GWLGenerator::FileOut;
using StandardSource = GWLGenerator::StandardSource;
using VolumePlanEntry = GWLGenerator::VolumePlanEntry;
using CompoundSrc = GWLGenerator::CompoundSrc;
using Hit = GWLGenerator::Hit;
using Instrument = GWLGenerator::Instrument;
using namespace GWLHelpers;


FluentDriver::FluentDriver(const DilutionEngine& engine) : m_engine(engine) {}

bool FluentDriver::generate(const QJsonObject &exp,
                                           QVector<FileOut> &outs,
                                           QString *err) const {

  const QJsonArray plates = exp.value("daughter_plates").toArray();
  if (plates.isEmpty()) {
    if (err)
      *err = QObject::tr("No daughter_plates in JSON.");
    return false;
  }

  const QString tipSizeStr = exp.value("_tip_size").toString("350ul");
  QString sValueReagentAndDilution = "19"; // 350ul default
  double tipCapacityUL = 313.0;
  if (tipSizeStr == "200ul") {
    sValueReagentAndDilution = "10";
    tipCapacityUL = 178.0;
  } else if (tipSizeStr == "1000ul") {
    sValueReagentAndDilution = "12";
    tipCapacityUL = 871.0;
  }

  const bool optimizeGwl = exp.value("_optimize_gwl").toBool(false);

  // ---- Build per-compound (testId, maxStartConc) lookup from test_requests
  // ---- A compound may appear in multiple requests (different tests /
  // concentrations). We keep the pair whose starting concentration is highest,
  // so the daughter plate is prepared for the most demanding dilution. testId
  // and startConc always travel together — storing them separately (as before)
  // would let the wrong testId be paired with the max concentration if the
  // compound appears in multiple tests.
  struct CmpPlanKey {
    QString testId;
    double startConcMicroM = 0.0;
  };
  QMap<QString, CmpPlanKey>
      cmpBestPlan; // compound_name -> {testId, maxStartConc}
  {
    const QJsonArray allReqs = exp.value("test_requests").toArray();
    for (const auto &rv : allReqs) {
      const auto ro = rv.toObject();
      const QString cn = ro.value("compound_name").toString().trimmed();
      const QString tid = ro.value("requested_tests").toString().trimmed();
      double sc = readDouble(ro, "starting_concentration", 0.0);
      const QString su =
          ro.value("starting_concentration_unit").toString().trimmed();
      if (su.compare("mM", Qt::CaseInsensitive) == 0)
        sc *= 1000.0;
      if (cn.isEmpty() || tid.isEmpty())
        continue;
      if (!cmpBestPlan.contains(cn) || sc > cmpBestPlan[cn].startConcMicroM)
        cmpBestPlan[cn] = {tid, sc};
    }
  }
  // Global starting concentration — from the first test_request.
  // Used for standards and as fallback for compounds missing from
  // test_requests.
  double globalStartConcMicroM = 0.0;
  {
    const QJsonArray allReqs = exp.value("test_requests").toArray();
    if (!allReqs.isEmpty()) {
      const auto tr0 = allReqs.at(0).toObject();
      globalStartConcMicroM = readDouble(tr0, "starting_concentration", 0.0);
      const QString su =
          tr0.value("starting_concentration_unit").toString().trimmed();
      if (su.compare("mM", Qt::CaseInsensitive) == 0)
        globalStartConcMicroM *= 1000.0;
    }
  }

  // ---- Load invenesis_catalogue.json once for the entire generate() call ----
  // Previously loadVolumePlan() opened and parsed this file on every call (once
  // per compound per call site). With N compounds and 3 call sites that is O(N)
  // redundant file I/O per generate() invocation. Load it here and pass it down
  // instead.
  QJsonObject catalogue;
  {
    QFile catFile(":/data/resources/data/invenesis_catalogue.json");
    if (!catFile.open(QIODevice::ReadOnly)) {
      qWarning() << "[WARN] Cannot open invenesis_catalogue.json — volume "
                    "plans will use fallback";
    } else {
      catalogue = QJsonDocument::fromJson(catFile.readAll()).object();
    }
  }

  // ---- DMSO control well volume (independent of compound rules) ----
  // Rule: max(15 µL, vol_from_daughter × 3 × 1.1)
  //   - 15 µL: minimum default well volume
  //   - ×3:    safety factor (plate is copied 3 times downstream)
  //   - ×1.1:  10% headroom so wells aren't fully drained on the 3rd copy
  // No serial-dilution correction (DMSO controls aren't diluted), and no
  // compound-rescue scaling (no compound to pipette in these wells).
  // Use exp.value("test_requests").toArray().first().toObject().value("requested_tests").toString() here because the local `testId` variable isn't declared
  // yet.
    double dmsoCtrlVol = 15.0; // hard minimum, applies if catalogue lookup fails
    const double overrideMinVol = exp.value("_override_min_vol").toDouble(-1.0);
    {
        const auto entry = catalogue.value(exp.value("test_requests").toArray().first().toObject().value("requested_tests").toString()).toObject();
        const double volFromDaughter = entry.value("vol_from_daughter_\u00b5l").toDouble();
        if (volFromDaughter > 0.0) {
            dmsoCtrlVol = std::max(15.0, volFromDaughter * 3.0 * 1.1);
        } else {
            qWarning() << "[WARN] DMSO control volume falling back to 15 µL —"
                       << "vol_from_daughter not found for test" << exp.value("test_requests").toArray().first().toObject().value("requested_tests").toString();
        }
        
        if (overrideMinVol > 0.0) {
            dmsoCtrlVol = std::max(overrideMinVol, volFromDaughter * 3.0 * 1.1);
        }

        dmsoCtrlVol = roundUp01(dmsoCtrlVol);
        qDebug() << "[INFO] DMSO control well volume:" << dmsoCtrlVol << "µL";
    }

  // ---- Audit collectors (across ALL daughter plates) ----
  QList<SeedAuditRow> seedAudit;
  QList<DilutionAuditRow> dilutionAudit;

  // Load standards matrix
  QVector<StandardSource> availableStandards;
  QString stdErr;
  if (!m_engine.loadStandardsMatrix(availableStandards, &stdErr)) {
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
      const QString startUnit =
          tr0.value("starting_concentration_unit").toString();
      if (startUnit.compare("mM", Qt::CaseInsensitive) == 0) {
        startConc *= 1000.0;
      }
      targetStdConc = startConc * kStandardConcFactor;
    }

    selectedStandard =
        m_engine.selectBestStandard(stdName, targetStdConc, availableStandards);
    if (!selectedStandard.barcode.isEmpty()) {
      useMatrixStandard = true;
      qDebug() << "[INFO] Selected standard:" << selectedStandard.sampleAlias
               << "at" << selectedStandard.concentration << "µM"
               << "from" << selectedStandard.barcode << selectedStandard.well;
    }
  }

  stdName = useMatrixStandard
                           ? selectedStandard.sampleAlias
                           : stdObj.value("Samplealias").toString();
  QString stdBarcode = useMatrixStandard
                           ? selectedStandard.barcode
                           : stdObj.value("Containerbarcode").toString();
  QString stdSrcWell =
      useMatrixStandard
          ? selectedStandard.well
          : normWell(stdObj.value("Containerposition").toString());
  double stdConc = useMatrixStandard
                       ? selectedStandard.concentration
                       : readDouble(stdObj, "Concentration", 20000.0);

  const int stdSrcPos = wellNameToIndex96(stdSrcWell);
  const QString standardMatrixLabel = "Standard_Matrix";

  const double df =
      (readDouble(exp, "dilutionFactor", 3.16) > 0.0) ? readDouble(exp, "dilutionFactor", 3.16) : 3.16;
  const QString testId = exp.value("test_requests").toArray().first().toObject().value("requested_tests").toString();
  const double stockMicroM = readDouble(exp.value("test_requests").toArray().first().toObject(), "stock_concentration", 10000.0);

  // Volume plans (global defaults)
  VolumePlanEntry vpe;
  QString verr;
  if (!m_engine.loadVolumePlan(catalogue, testId, stockMicroM,
                             globalStartConcMicroM, &vpe, &verr, overrideMinVol)) {
    qWarning() << "[WARN][FluentBackend] volume plan:" << verr;
    vpe.volMother = overrideMinVol > 0.0 ? overrideMinVol * 2.0 : 30.0;
    vpe.dmso = 0.0;
  }

  VolumePlanEntry stdVpe = vpe;
  double stdDf = df;
  if (useMatrixStandard && !testId.isEmpty() && !stdName.isEmpty()) {
      const auto catEntry = catalogue.value(testId).toObject();
      const auto stdSpecs = catEntry.value("standards").toObject();
      const auto stdSpec  = stdSpecs.value(stdName).toObject();

      const double topDose = readDouble(stdSpec, "top_dose_uM", 0.0);
      const double ec50    = readDouble(stdSpec, "ec50_uM",     0.0);

      if (stdSpec.isEmpty()) {
          qWarning() << "[WARN] No standards entry for" << stdName
                     << "under test" << testId
                     << "in invenesis_catalogue.json — falling back to compound df.";
      } else if (topDose <= 0.0 || ec50 <= 0.0) {
          qWarning() << "[WARN] top_dose_uM or ec50_uM is zero/missing for"
                     << stdName << "in test" << testId
                     << "— falling back to compound df.";
      } else if (ec50 >= topDose) {
          qWarning() << "[WARN] EC50 (" << ec50 << "uM) >= top_dose ("
                     << topDose << "uM) for" << stdName << "in test" << testId
                     << "— chain cannot straddle EC50, falling back to compound df.";
      } else {
          VolumePlanEntry tempVpe;
          if (m_engine.loadVolumePlan(catalogue, testId, stdConc, topDose,
                                    &tempVpe, &verr, overrideMinVol)) {
              stdVpe = tempVpe;
              qDebug() << "[INFO] Standard volume plan:" << stdName
                       << "tube=" << stdConc << "uM"
                       << "top_dose(test)=" << topDose << "uM"
                       << "ec50=" << ec50 << "uM"
                       << "-> daughter seed=" << tempVpe.concMother << "uM";
          } else {
              qWarning() << "[WARN] loadVolumePlan failed for standard:" << verr;
          }

          const int stdNDil = std::max(2, numberOfDilutionsFromJson(exp));
          const double bottomDose = (ec50 * ec50) / topDose;
          stdDf = std::pow(topDose / bottomDose, 1.0 / (stdNDil - 1));
          qDebug() << "[INFO] Standard df:" << stdDf << "across" << stdNDil
                   << "wells (top=" << topDose << "uM, bottom=" << bottomDose
                   << "uM, ec50=" << ec50 << "uM)";
      }
  }

  // Round seed/DMSO volumes UP (never under-fill); transfer volumes to nearest
  // (avoid drift). Serial dilution logic: V_transfer = V_final / (df - 1).
  // First well working volume = V_final + V_transfer.
  // DMSO and compound in first well must be scaled up to match the higher
  // working volume.
  const double volMother = roundUp01(vpe.volMother);
  const double transferVol =
      roundNearest01(df > 1.0 ? vpe.volMother / (df - 1.0) : 0.0);
  const double startTotalVol = volMother + transferVol;
  const double dmsoStart = roundUp01(
      vpe.volMother > 0.0 ? vpe.dmso * (startTotalVol / vpe.volMother) : 0.0);
  const double dmsoDilute = roundUp01(transferVol > 0.0 ? vpe.volMother : 0.0);

  const double stdVolMother = roundUp01(stdVpe.volMother);
  const double stdTransferVol =
      roundNearest01(stdDf > 1.0 ? stdVpe.volMother / (stdDf - 1.0) : 0.0);
  const double stdStartTotalVol = stdVolMother + stdTransferVol;
  const double stdDmsoStart =
      roundUp01(stdVpe.volMother > 0.0
                    ? stdVpe.dmso * (stdStartTotalVol / stdVpe.volMother)
                    : 0.0);
  const double stdDmsoDilute =
      roundUp01(stdTransferVol > 0.0 ? stdVpe.volMother : 0.0);

  // ---- Per-compound volume/dilution plans ----
  struct PerCmpPlan {
    double volMother;     // µL (target final volume)
    double startTotalVol; // µL (working volume in first well)
    double dmsoStart;     // µL (seed DMSO for first well)
    double transferVol;   // µL (per dilution transfer)
    double dmsoDilute;    // µL (DMSO top-up for dilution wells)
    double df;            // dilution factor
    int nDil;             // number of dilution steps
    double stockConc;     // µM
  };

  // Make a default plan from the GLOBAL values computed above
  auto makeDefaultPlan = [&]() -> PerCmpPlan {
    PerCmpPlan p;
    p.volMother = volMother;
    p.startTotalVol = startTotalVol;
    p.dmsoStart = dmsoStart;
    p.transferVol = transferVol;
    p.dmsoDilute = dmsoDilute;
    p.df = df;
    p.nDil = std::max(1, numberOfDilutionsFromJson(exp));
    p.stockConc = stockMicroM;
    return p;
  };

  QMap<QString, PerCmpPlan> perPlan; // key: product_name

  auto computePlanFor = [&](const QJsonObject &co) -> PerCmpPlan {
    PerCmpPlan p = makeDefaultPlan();

    // per-compound stock (normalize to uM if provided in mM)
    double cStock = readDouble(co, "concentration", p.stockConc);
    if (cStock != p.stockConc) {
      const QString cUnit =
          co.value("concentration_unit").toString().trimmed().toLower();
      if (cUnit == "mm")
        cStock *= 1000.0;
    }

    // optional per-compound overrides
    double cDf = readDouble(co, "dilution_factor", p.df);
    int cNDil = co.value("number_of_dilutions").toInt(p.nDil);

    // Resolve per-compound testId and starting concentration from the paired
    // lookup. cmpBestPlan holds the (testId, startConc) pair with the highest
    // startConc, so both values are guaranteed to come from the same
    // test_request row.
    const QString cName = co.value("product_name").toString().trimmed();
    const CmpPlanKey &key =
        cmpBestPlan.value(cName, CmpPlanKey{testId, globalStartConcMicroM});
    const QString cTestId = key.testId.isEmpty() ? testId : key.testId;
    const double cStartConc =
        key.startConcMicroM > 0.0 ? key.startConcMicroM : globalStartConcMicroM;

    // Try to load a volume plan for this compound
    VolumePlanEntry cvp;
    QString cvpErr;
    if (m_engine.loadVolumePlan(catalogue, cTestId, cStock, cStartConc, &cvp,
                              &cvpErr, overrideMinVol)) {
      p.volMother = roundUp01(cvp.volMother);
      p.df = (cDf > 0.0 ? cDf : p.df);
      p.transferVol =
          roundNearest01(p.df > 1.0 ? cvp.volMother / (p.df - 1.0) : 0.0);
      p.startTotalVol = p.volMother + p.transferVol;
      p.dmsoStart = roundUp01(cvp.volMother > 0.0
                                  ? cvp.dmso * (p.startTotalVol / cvp.volMother)
                                  : 0.0);
      p.dmsoDilute = roundUp01(p.transferVol > 0.0 ? cvp.volMother : 0.0);
    } else {
      // Fall back to the default plan but still honor per-compound DF
      p.df = (cDf > 0.0 ? cDf : p.df);
      p.transferVol =
          roundNearest01(p.df > 1.0 ? p.volMother / (p.df - 1.0) : 0.0);
      p.startTotalVol = p.volMother + p.transferVol;
      p.dmsoStart = roundUp01(
          p.volMother > 0.0 ? vpe.dmso * (p.startTotalVol / p.volMother) : 0.0);
      p.dmsoDilute = roundUp01(p.transferVol > 0.0 ? p.volMother : 0.0);
    }

    p.nDil = std::max(1, cNDil);
    p.stockConc = cStock;
    return p;
  };

  // Build plans for every compound in the JSON
  const auto cmpArr = exp.value("compounds").toArray();
  for (const auto &vv : cmpArr) {
    const auto co = vv.toObject();
    const QString nm = co.value("product_name").toString().trimmed();
    if (nm.isEmpty())
      continue;
    perPlan.insert(nm, computePlanFor(co));
  }

  // Build compound index map (name -> matrix barcode + well)
  struct SrcLoc {
    QString barcode;
    QString well;
  };
  QMap<QString, SrcLoc> cmpIndex;
  for (const auto &v : exp.value("compounds").toArray()) {
    const auto o = v.toObject();
    const QString name = o.value("product_name").toString().trimmed();
    const QString bc = o.value("container_id").toString().trimmed();
    const QString w = normWell(o.value("well_id").toString().trimmed());
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
      r.sampleAlias = o.value("product_name").toString();
      r.wellA01 = toA01(o.value("well_id").toString());
      r.volumeUL = roundUp01(readDouble(o, "weight", 0.0));
      r.volumeUnit = o.value("weight_unit").toString("uL");
      r.conc = readDouble(o, "concentration", 0.0);
      r.concUnit = o.value("concentration_unit").toString("uM");
      r.u1 = QString("%1_%2").arg(r.containerBarcode, r.wellA01);
      r.u2 = o.value("invenesis_solution_id").toString();
      r.u3 = QString::number(r.conc, 'f', 4);
      r.u4 = QString::number(
          readDouble(exp.value("test_requests").toArray().at(0).toObject(),
                     "dilution_steps", 0.0));
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
  auto produceDaughterPlateMap =
      [&](int di, const QJsonObject &plate,
          const QList<QPair<QString, QString>> &placedStart,
          const QMap<QString, int> &perHitStep) -> FileOut {
    const QString dghtBarcode = QString("Daughter_%1").arg(di + 1);
    const QString projectCode = exp.value("project_code").toString();
    const QJsonObject wells = plate.value("wells").toObject();
    const QJsonObject concs = plate.value("concentrations").toObject();
    // nDilGlob is only used for standards; compound chains use per-compound
    // nDil.
    const int nDilGlob = std::max(1, numberOfDilutionsFromJson(exp));

    auto labelOf = [&](const QString &wn) -> QString {
      return wells.value(wn).toString().trimmed();
    };

    QList<DaughterPlateEntry> rows;

    // Standard wells (layout-driven)
    QSet<QString> stdSet;
    for (auto it = wells.begin(); it != wells.end(); ++it)
      if (it.value().toString().trimmed().compare("Standard",
                                                  Qt::CaseInsensitive) == 0)
        stdSet.insert(normWell(it.key()));

    // split into chains
    QStringList sortedStd = QStringList(stdSet.values());
    std::sort(sortedStd.begin(), sortedStd.end(),
              [](const QString &a, const QString &b) {
                return wellNameToIndex96(a) < wellNameToIndex96(b);
              });
    auto isStd = [&](const QString &w) -> bool {
      return stdSet.contains(normWell(w));
    };
    QSet<QString> visitedStd;
    for (const QString &start : sortedStd) {
      if (visitedStd.contains(start))
        continue;
      const int startIdx = wellNameToIndex96(start);
      const QString prev8 = indexToWellName96(startIdx - 8);
      const QString prev1 = indexToWellName96(startIdx - 1);
      if (isStd(prev8) || isStd(prev1))
        continue;

      int step = 0;
      if (isStd(indexToWellName96(startIdx + 8)))
        step = 8;
      else if (isStd(indexToWellName96(startIdx + 1)))
        step = 1;

      QStringList chain;
      chain << start;
      visitedStd.insert(start);
      if (step != 0) {
        int cur = startIdx;
        while (true) {
          const int nxt = cur + step;
          const QString wn = indexToWellName96(nxt);
          if (!isStd(wn))
            break;
          chain << wn;
          visitedStd.insert(wn);
          cur = nxt;
        }
      }

      for (int i = 0; i < chain.size(); ++i) {
        DaughterPlateEntry r;
        r.containerBarcode = dghtBarcode;
        r.sampleAlias = (i == 0 ? stdName : QString("%1_dil").arg(stdName));
        r.wellA01 = toA01(chain.at(i));
        r.volumeUL = stdVolMother;
        r.volumeUnit = "ul";
        r.conc = concs.value(chain.at(i)).toDouble();
        r.concUnit = "uM";
        r.u1 = QString("%1_%2").arg(r.containerBarcode, r.wellA01);
        r.u2 = useMatrixStandard
                   ? selectedStandard.solutionId
                   : stdObj.value("invenesis_solution_ID").toString();
        r.u3 = QString::number(r.conc, 'f', 4);
        r.u4 = QString::number(
            readDouble(exp.value("test_requests").toArray().at(0).toObject(),
                       "dilution_steps", 0.0));
        r.u5 = projectCode;
        rows.push_back(r);
      }
    }

    // Compound chains.
    // placedStart contains only chain-start wells (not dilution wells), so
    // every entry here is guaranteed to have a valid perHitStep entry. Chain
    // length comes from the compound's own perPlan.nDil, not the global value.
    QSet<QString> seenStart;
    for (const auto &p : placedStart) {
      const QString &name = p.first;
      const QString dst = normWell(p.second);
      if (seenStart.contains(dst))
        continue;
      seenStart.insert(dst);

      const auto &plan =
          perPlan.value(name); // valid: placedStart only has known compounds
      const int nDilC = std::max(1, plan.nDil);
      const int step = perHitStep.value(dst, 0);

      DaughterPlateEntry r0;
      r0.containerBarcode = dghtBarcode;
      r0.sampleAlias = name;
      r0.wellA01 = toA01(dst);
      r0.volumeUL = plan.volMother; // per-compound volume, not global
      r0.volumeUnit = "ul";
      r0.conc = concs.value(dst).toDouble();
      r0.concUnit = "uM";
      r0.u1 = QString("%1_%2").arg(r0.containerBarcode, r0.wellA01);
      r0.u2 = cmpIndex.value(name).barcode;
      r0.u3 = QString::number(r0.conc, 'f', 4);
      r0.u4 = QString::number(
          readDouble(exp.value("test_requests").toArray().at(0).toObject(),
                     "dilution_steps", 0.0));
      r0.u5 = projectCode;
      rows.push_back(r0);

      if (step == 0)
        continue; // single well, no dilution chain

      const QString lab = wells.value(dst).toString().trimmed();
      int cur = wellNameToIndex96(dst);
      for (int s = 1; s < nDilC; ++s) {
        const int nxt = cur + step;
        if (nxt < 1 || nxt > 96)
          break;
        const QString wn = indexToWellName96(nxt);
        if (wells.value(wn).toString().trimmed() != lab)
          break;

        DaughterPlateEntry rd;
        rd.containerBarcode = dghtBarcode;
        rd.sampleAlias = QString("%1_dil%2").arg(name).arg(s);
        rd.wellA01 = toA01(wn);
        rd.volumeUL = plan.volMother;
        rd.volumeUnit = "ul";
        rd.conc = concs.value(wn).toDouble();
        rd.concUnit = "uM";
        rd.u1 = QString("%1_%2").arg(rd.containerBarcode, rd.wellA01);
        rd.u2 = r0.u2;
        rd.u3 = QString::number(rd.conc, 'f', 4);
        rd.u4 = r0.u4;
        rd.u5 = r0.u5;
        rows.push_back(rd);

        cur = nxt;
      }
    }

    // DMSO controls: use the dedicated DMSO control rule (not volMother).
    for (auto it = wells.begin(); it != wells.end(); ++it) {
      if (it.value().toString().trimmed().compare("DMSO",
                                                  Qt::CaseInsensitive) == 0) {
        DaughterPlateEntry rc;
        rc.containerBarcode = dghtBarcode;
        rc.sampleAlias = "DMSO";
        rc.wellA01 = toA01(normWell(it.key()));
        rc.volumeUL = dmsoCtrlVol;
        rc.volumeUnit = "ul";
        rc.conc = 100.0;
        rc.concUnit = "%";
        rc.u1 = QString("%1_%2").arg(rc.containerBarcode, rc.wellA01);
        rc.u2 = "DMSO";
        rc.u3 = "100";
        rc.u4 = QString::number(
            readDouble(exp.value("test_requests").toArray().at(0).toObject(),
                       "dilution_steps", 0.0));
        rc.u5 = projectCode;
        rows.push_back(rc);
      }
    }

    FileOut fcsv;
    fcsv.relativePath =
        QString("PlateMapHitLW/%1.csv").arg(QString("Daughter_%1").arg(di + 1));
    fcsv.lines = renderPlateMapCSV(rows);
    fcsv.isAux = true;
    return fcsv;
  };

  // ---- Process each daughter plate ----
  for (int di = 0; di < plates.size(); ++di) {
    const QJsonObject plate = plates.at(di).toObject();
    const QJsonObject wells = plate.value("wells").toObject();
    const QString dghtLabel =
        QString("Daughter[%1]")
            .arg(QString("%1").arg(di + 1, 3, 10, QChar('0')));
    const QString dghtBarcodeStr = QString("Daughter_%1").arg(di + 1);
    const int nDilGlob = std::max(1, numberOfDilutionsFromJson(exp));

    QMap<QString, QString> normWellsMap;
    for (auto it = wells.begin(); it != wells.end(); ++it) {
      normWellsMap.insert(normWell(it.key()), it.value().toString().trimmed());
    }

    // Collect hits (compounds)
    struct Hit {
      QString product;
      QString dstWell;
      QString srcBarcode;
      QString srcWell;
    };
    QVector<Hit> hits;
    for (auto it = wells.begin(); it != wells.end(); ++it) {
      const QString dstWell = normWell(it.key());
      const QString who = it.value().toString().trimmed();
      if (who.isEmpty() || who.compare("DMSO", Qt::CaseInsensitive) == 0 ||
          who.compare("Standard", Qt::CaseInsensitive) == 0)
        continue;
      if (!cmpIndex.contains(who))
        continue;
      const auto src = cmpIndex.value(who);
      hits.push_back(Hit{who, dstWell, src.barcode, src.well});
    }

    // Group hits by matrix barcode
    QMap<QString, QVector<Hit>> byMatrix;
    for (const auto &h : hits)
      byMatrix[h.srcBarcode].push_back(h);

    // Build standard chains from layout
    const auto stdChains = buildStandardChainsFromLayout(wells);

    // ------------------ SAME-LABEL chain discovery for compounds
    // ------------------
    QSet<QString> startWells, diluteWells, controlDmsoWells;
    QMap<QString, int> perHitStep;
    QMap<QString, QString>
        startWell2Product; // normalized start well -> compound name

    auto labelOf = [&](const QString &wn) -> QString {
      return normWellsMap.value(normWell(wn));
    };

    QVector<Hit> hitsSorted = hits;
    std::sort(
        hitsSorted.begin(), hitsSorted.end(), [](const Hit &a, const Hit &b) {
          return wellNameToIndex96(a.dstWell) < wellNameToIndex96(b.dstWell);
        });

    QSet<QString> visited;
    for (const auto &h : hitsSorted) {
      const QString start = normWell(h.dstWell);
      if (visited.contains(start))
        continue;

      const QString lab = labelOf(start);
      const int startIdx = wellNameToIndex96(start);
      if (startIdx < 1)
        continue;

      const QString plus8 = indexToWellName96(startIdx + 8);
      const QString plus1 = indexToWellName96(startIdx + 1);
      const bool canAcross = (!plus8.isEmpty() && labelOf(plus8) == lab);
      const bool canDown = (!plus1.isEmpty() && labelOf(plus1) == lab);

      int step = 0;
      if (canAcross)
        step = 8;
      else if (canDown)
        step = 1;

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
        for (int s = 1; s < nDilGlob; ++s) {
          const int nxt = cur + step;
          if (nxt < 1 || nxt > 96)
            break;
          const QString wn = indexToWellName96(nxt);
          if (labelOf(wn) != lab)
            break;
          diluteWells.insert(wn);
          visited.insert(wn);
          cur = nxt;
        }
      }
    }

    // DMSO controls
    for (auto it = wells.begin(); it != wells.end(); ++it)
      if (it.value().toString().trimmed().compare("DMSO",
                                                  Qt::CaseInsensitive) == 0)
        controlDmsoWells.insert(normWell(it.key()));

    // Standards start/dilute wells (from chains)
    QSet<QString> stdStartWells, stdDiluteWells;
    for (const auto &chain : stdChains) {
      if (chain.isEmpty())
        continue;
      stdStartWells.insert(chain.first());
      for (int i = 1; i < chain.size(); ++i)
        stdDiluteWells.insert(chain.at(i));
    }

    // ---- 1) Reagent_distrib.gwl : ROW-WISE single aspirate (S;19, direction
    // toggle) ----
    {
      FileOut fo;
      fo.relativePath = QString("dght_%1/Reagent_distrib.gwl").arg(di);

      QStringList L;
      L << "C;Reagent distribution (DMSO) by row — ONE aspirate, many "
           "dispenses per row";
      L << "B;";
      L << QString("S;%1").arg(sValueReagentAndDilution);
      L << "C;Direction toggle via dmso_direction (default LTR)";

      const bool rtl = dmsoDispenseRightToLeft(exp); // false => LTR

      // Build per-row map: row -> (destPos -> per-well volume)
      // Use rounded per-dispense volumes so A; total equals sum(D;)
      QMap<int, QMap<int, double>> row2pos2vol;

      auto addVolAt = [&](int destIdx, double v) {
        const double vv = roundUp01(v);
        if (vv <= 0.0)
          return;
        row2pos2vol[rowFromIndex96(destIdx)][destIdx] += vv;
      };

      // Standards per their own plan
      for (const auto &chain : stdChains) {
        if (chain.isEmpty())
          continue;
        addVolAt(wellNameToIndex96(chain.first()), stdDmsoStart);
        for (int i = 1; i < chain.size(); ++i)
          addVolAt(wellNameToIndex96(chain.at(i)), stdDmsoDilute);
      }

      // Compounds: per-chain using perPlan of THAT product
      {
        QList<QString> starts = startWells.values();
        for (const auto &start : starts) {
          const QString prod = labelOf(start);
          const auto plan = perPlan.value(prod, makeDefaultPlan());
          const int step = perHitStep.value(start, 0);
          const int nDilC = std::max(1, plan.nDil);

          int cur = wellNameToIndex96(start);
          addVolAt(cur, plan.dmsoStart); // start

          if (step != 0) {
            for (int s = 1; s < nDilC; ++s) {
              const int nxt = cur + step;
              if (nxt < 1 || nxt > 96)
                break;
              const QString wn = indexToWellName96(nxt);
              if (labelOf(wn) != prod)
                break;
              addVolAt(nxt, plan.dmsoDilute);
              cur = nxt;
            }
          }
        }
      }

      // DMSO controls: dedicated rule independent of compound plans.
      // max(15 µL, vol_from_daughter × 3 × 1.1) — see dmsoCtrlVol computation
      // at the top of generate().
      for (int idx : namesToIndices(controlDmsoWells))
        addVolAt(idx, dmsoCtrlVol);

      if (optimizeGwl) {
        // Group by exact volume required to output precise R; commands.
        // Starting wells that need 0ul (or different volumes) will be naturally
        // omitted from the list for the dilution volume, and thus appended to
        // ExcludeDestWell.
        QMap<double, QList<int>> vol2wells;
        for (int r = 0; r < 8; ++r) {
          for (auto it = row2pos2vol[r].cbegin(); it != row2pos2vol[r].cend();
               ++it) {
            vol2wells[roundUp01(it.value())].push_back(it.key());
          }
        }

        for (auto it = vol2wells.cbegin(); it != vol2wells.cend(); ++it) {
          const double v = it.key();
          if (v <= 0.0)
            continue;

          QList<int> wells = it.value();
          std::sort(wells.begin(), wells.end());

          int minW = wells.first();
          int maxW = wells.last();

          QString rCmd = QString("R;100ml_Higher;;;1;1;%1;;;%2;%3;%4;DMSO "
                                 "Contact Dry Multi Invenesis;999;100;0")
                             .arg(dghtLabel)
                             .arg(minW)
                             .arg(maxW)
                             .arg(QString::number(v, 'f', 1));

          QSet<int> wellSet(wells.begin(), wells.end());
          for (int w = minW; w <= maxW; ++w) {
            if (!wellSet.contains(w)) {
              rCmd += QString(";%1").arg(w);
            }
          }
          L << rCmd;
        }
      } else {
        // Emit per row
        for (int r = 0; r < 8; ++r) {
          if (!row2pos2vol.contains(r) || row2pos2vol[r].isEmpty())
            continue;

          // sorted list of (destIdx, roundedVol) in column order
          QList<QPair<int, double>> posVols;
          posVols.reserve(row2pos2vol[r].size());
          for (auto it = row2pos2vol[r].cbegin(); it != row2pos2vol[r].cend();
               ++it)
            posVols.push_back({it.key(), roundUp01(it.value())});

          std::sort(
              posVols.begin(), posVols.end(),
              [&](const QPair<int, double> &a, const QPair<int, double> &b) {
                const int ca = colFromIndex96(a.first);
                const int cb = colFromIndex96(b.first);
                return rtl ? (ca > cb) : (ca < cb);
              });

          // Split if total would exceed the tip capacity limit.
          // If a single well volume exceeds the tip capacity, we must split it
          // into multiple dispenses so the Tecan does not throw an error.
          QList<QPair<int, double>> splitPosVols;
          for (const auto &pv : posVols) {
            double remaining = pv.second;
            while (remaining > tipCapacityUL) {
              splitPosVols.push_back({pv.first, tipCapacityUL});
              remaining -= tipCapacityUL;
            }
            if (remaining > 0.0) {
              splitPosVols.push_back({pv.first, remaining});
            }
          }

          QList<QPair<int, double>> chunk;
          double chunkSum = 0.0;

          auto flushChunk = [&]() {
            if (chunk.isEmpty())
              return;
            appendAThenManyD_Vary(
                L, QStringLiteral("100ml_Higher"), 1, dghtLabel, chunk,
                QStringLiteral("DMSO Contact Dry Multi Invenesis"));
            chunk.clear();
            chunkSum = 0.0;
            L << "W;"; // close this aspirate
          };

          for (const auto &pv : splitPosVols) {
            const double v = pv.second; // already rounded by pre-split logic
            if (chunkSum + v > tipCapacityUL && !chunk.isEmpty()) {
              flushChunk();
            }
            chunk.push_back(pv);
            chunkSum += v;
          }
          flushChunk(); // last chunk
        }
      }

      L << "B;";
      fo.lines = L;
      outs.push_back(std::move(fo));
    }

    // ---- 2) Matrix compound placement files (first also seeds Standard start)
    // ----
    {
      int mIdx = 0;
      for (auto it = byMatrix.cbegin(); it != byMatrix.cend(); ++it, ++mIdx) {
        const QString matrixBarcode = it.key();
        const QString matrixLabel =
            QString("Matrix[%1]")
                .arg(QString("%1").arg(mIdx + 1, 3, 10, QChar('0')));
        const QVector<Hit> &mhits = it.value();

        FileOut fo;
        fo.relativePath = QString("dght_%1/%2.gwl").arg(di).arg(matrixBarcode);

        QStringList L;
        L << QString("C;Place compounds from %1 (barcode %2)")
                 .arg(matrixLabel)
                 .arg(matrixBarcode);
        L << "B;";
        L << "S;7";

        // Seed Standard into start well(s) ONLY in first matrix file — one-shot
        // A/D/W
        if (mIdx == 0 && !stdChains.isEmpty() && !stdBarcode.isEmpty() &&
            stdSrcPos >= 1) {
          L << QString("C;Standard %1 seeded in start well(s); no serial "
                       "dilution here")
                   .arg(stdName);
          const double volStartStandard =
              roundUp01(std::max(0.0, stdStartTotalVol - stdDmsoStart));
          if (volStartStandard > 1e-6) {
            for (const auto &chain : stdChains) {
              if (chain.isEmpty())
                continue;
              const int startPos = wellNameToIndex96(chain.first());
              appendADFluentOneShot(L, standardMatrixLabel, stdSrcPos,
                                    dghtLabel, startPos, volStartStandard,
                                    "DMSO Matrix");
              // Audit: standard seeding
              SeedAuditRow ar;
              ar.daughterBarcode = dghtBarcodeStr;
              ar.analyte = (stdName.isEmpty() ? "Standard" : stdName);
              ar.matrixBarcode = stdBarcode;
              ar.matrixWell = stdSrcWell;
              ar.startWell = chain.first();
              ar.seedVolumeUL = volStartStandard;
              ar.notes = "standard";
              seedAudit.push_back(ar);
            }
          }
        }

        // Compounds: seed ONLY the starting wells (one-shot per placement,
        // PER-COMPOUND volume)
        {
          QVector<Hit> startSeeds;
          startSeeds.reserve(mhits.size());
          for (const auto &h : mhits) {
            if (startWells.contains(normWell(h.dstWell)))
              startSeeds.push_back(h);
          }

          std::sort(startSeeds.begin(), startSeeds.end(),
                    [](const Hit &a, const Hit &b) {
                      return wellNameToIndex96(a.dstWell) <
                             wellNameToIndex96(b.dstWell);
                    });

          for (const auto &h : startSeeds) {
            const auto plan = perPlan.value(h.product, makeDefaultPlan());
            const double volCompound =
                roundUp01(std::max(0.0, plan.startTotalVol - plan.dmsoStart));
            if (volCompound <= 0.0)
              continue;

            const int srcPos = wellNameToIndex96(h.srcWell);
            const int dstPos = wellNameToIndex96(h.dstWell);
            appendADFluentOneShot(L, matrixLabel, srcPos, dghtLabel, dstPos,
                                  volCompound, "DMSO Matrix");

            // Audit: compound seeding
            SeedAuditRow ar;
            ar.daughterBarcode = dghtBarcodeStr;
            ar.analyte = h.product;
            ar.matrixBarcode = h.srcBarcode;
            ar.matrixWell = h.srcWell;
            ar.startWell = h.dstWell;
            ar.seedVolumeUL = volCompound;
            ar.notes = "compound";
            seedAudit.push_back(ar);
          }
        }

        L << "B;";
        fo.lines = L;
        outs.push_back(std::move(fo));
      }
    }

    // ---- 3) serial_dilution.gwl (standards first; then compounds; one tip per
    // chain) ----
    {
      FileOut fo;
      fo.relativePath = QString("dght_%1/serial_dilution.gwl").arg(di);

      QStringList L;
      L << "C;Serial dilutions — standards first, then compounds; one tip per "
           "chain (W; between chains)";
      L << "B;";
      L << "S;7"; // 50ul tips for serial dilution

      auto labelOf = [&](const QString &wn) -> QString {
        return normWellsMap.value(normWell(wn));
      };

      auto emitChain = [&](const QList<int> &pos, double volUL) {
        if (pos.size() < 2 || roundNearest01(volUL) <= 0.0)
          return;
        for (int i = 0; i + 1 < pos.size(); ++i) {
          appendADFluent(L, dghtLabel, pos[i], dghtLabel, pos[i + 1], volUL,
                         "Serial dilution");
        }
        L << "W;";
      };

      // ---------------- Standards first (sorted by start index)
      // ----------------
      if (stdTransferVol > 1e-6 && !stdChains.isEmpty()) {
        QVector<QStringList> stdSorted = stdChains;
        std::sort(stdSorted.begin(), stdSorted.end(),
                  [&](const QStringList &a, const QStringList &b) {
                    return wellNameToIndex96(a.first()) <
                           wellNameToIndex96(b.first());
                  });

        for (const auto &chain : stdSorted) {
          QList<int> pos;
          for (const auto &wn : chain)
            pos.push_back(wellNameToIndex96(wn));
          emitChain(pos, stdTransferVol);

          // Audit: standard dilution steps
          for (int i = 0; i + 1 < pos.size(); ++i) {
            DilutionAuditRow dr;
            dr.daughterBarcode = dghtBarcodeStr;
            dr.analyte = (stdName.isEmpty() ? "Standard" : stdName);
            dr.srcWell = indexToWellName96(pos[i]);
            dr.dstWell = indexToWellName96(pos[i + 1]);
            dr.transferUL = stdTransferVol;
            dr.notes = "standard";
            dilutionAudit.push_back(dr);
          }
        }
      }

      // ---------------- Compounds (sorted by the first A; index), PER-COMPOUND
      // plan ----------------
      {
        struct CChain {
          int startIdx;
          QList<int> pos;
        };
        QVector<CChain> chains;

        // collect chains with per-compound length
        QList<QString> starts = startWells.values();
        for (const auto &start : starts) {
          const int step = perHitStep.value(start, 0);
          if (step == 0)
            continue; // no dilution chain

          const QString prod = labelOf(start);
          const auto plan = perPlan.value(prod, makeDefaultPlan());
          const int nDilC = std::max(1, plan.nDil);

          QList<int> pos;
          int cur = wellNameToIndex96(start);
          pos.push_back(cur);

          for (int s = 1; s < nDilC; ++s) {
            const int nxt = cur + step;
            if (nxt < 1 || nxt > 96)
              break;
            const QString wn = indexToWellName96(nxt);
            if (labelOf(wn) != prod)
              break;
            pos.push_back(nxt);
            cur = nxt;
          }

          if (pos.size() >= 2) {
            CChain c;
            c.startIdx = wellNameToIndex96(start);
            c.pos = std::move(pos);
            chains.push_back(std::move(c));
          }
        }

        // sort by the first aspirate index (strict ascending)
        std::sort(chains.begin(), chains.end(),
                  [](const CChain &a, const CChain &b) {
                    return a.startIdx < b.startIdx;
                  });

        // emit with per-compound transfer volume + audit
        for (const auto &c : chains) {
          const QString startWellName = indexToWellName96(c.startIdx);
          const QString analyteName = startWell2Product.value(
              startWellName, QString("Compound_%1").arg(startWellName));
          const auto plan = perPlan.value(analyteName, makeDefaultPlan());

          emitChain(c.pos, plan.transferVol);

          for (int i = 0; i + 1 < c.pos.size(); ++i) {
            DilutionAuditRow dr;
            dr.daughterBarcode = dghtBarcodeStr;
            dr.analyte = analyteName;
            dr.srcWell = indexToWellName96(c.pos[i]);
            dr.dstWell = indexToWellName96(c.pos[i + 1]);
            dr.transferUL = plan.transferVol;
            dr.notes = "compound";
            dilutionAudit.push_back(dr);
          }
        }
      }

      L << "B;";
      fo.lines = L;
      outs.push_back(std::move(fo));
    }

    // Generate plate maps.
    // Pass only chain-start wells to produceDaughterPlateMap.
    // Previously ALL hits were passed, so dilution wells appeared as orphaned
    // seed rows in the CSV (perHitStep returned 0 for them, so no chain was
    // emitted but a standalone compound row was still written).
    if (di == 0) {
      produceMatrixPlateMaps(outs);
    }
    {
      QList<QPair<QString, QString>> placedStart;
      for (const auto &sw : startWells)
        placedStart.append({startWell2Product.value(sw), normWell(sw)});
      outs.push_back(
          produceDaughterPlateMap(di, plate, placedStart, perHitStep));
    }
  }

  // ---- Process each test plate to generate Platemaps ----
  const QJsonArray testPlates = exp.value("test_plates").toArray();
  for (int ti = 0; ti < testPlates.size(); ++ti) {
    const QJsonObject plate = testPlates.at(ti).toObject();
    const QString testBarcode = QString("Test_%1").arg(ti + 1);
    const QString projectCode = exp.value("project_code").toString();
    const QJsonObject wells = plate.value("wells").toObject();
    const QJsonObject concs = plate.value("concentrations").toObject();

    QList<DaughterPlateEntry> rows;
    for (auto it = wells.begin(); it != wells.end(); ++it) {
      const QString wn = normWell(it.key());
      const QString cmpName = it.value().toString().trimmed();
      if (cmpName.isEmpty())
        continue;

      QString baseCmp = cmpName;
      int idx = baseCmp.indexOf(" (");
      if (idx > 0)
        baseCmp = baseCmp.left(idx);

      DaughterPlateEntry r;
      r.containerBarcode = testBarcode;
      r.sampleAlias = cmpName;
      r.wellA01 = toA01(wn);

      // Look up final volume
      double volFinal = 0.0;
      if (baseCmp.compare("DMSO", Qt::CaseInsensitive) == 0) {
        volFinal = vpe.volFinal; // default vpe
      } else {
        VolumePlanEntry tempVpe;
        QString tempErr;
        double cStock = cmpIndex.contains(baseCmp)
                            ? perPlan.value(baseCmp).stockConc
                            : stockMicroM;
        if (m_engine.loadVolumePlan(catalogue, testId, cStock,
                                  globalStartConcMicroM, &tempVpe, &tempErr, overrideMinVol)) {
          volFinal = tempVpe.volFinal;
        } else {
          volFinal = vpe.volFinal;
        }
      }

      r.volumeUL = volFinal;
      r.volumeUnit = "ul";
      r.conc = concs.value(it.key()).toDouble();
      r.concUnit = "uM";
      r.u1 = QString("%1_%2").arg(r.containerBarcode, r.wellA01);

      if (cmpIndex.contains(baseCmp)) {
        r.u2 = cmpIndex.value(baseCmp).barcode;
      } else if (baseCmp.startsWith("Standard", Qt::CaseInsensitive) &&
                 useMatrixStandard) {
        r.u2 = selectedStandard.solutionId;
      } else if (baseCmp.startsWith("Standard", Qt::CaseInsensitive)) {
        r.u2 = stdObj.value("invenesis_solution_ID").toString();
      } else if (baseCmp.compare("DMSO", Qt::CaseInsensitive) == 0) {
        r.u2 = "DMSO";
      } else {
        r.u2 = "";
      }

      r.u3 = QString::number(r.conc, 'f', 4);
      r.u4 = QString::number(
          readDouble(exp.value("test_requests").toArray().at(0).toObject(),
                     "dilution_steps", 0.0));
      r.u5 = projectCode;

      if (baseCmp.compare("DMSO", Qt::CaseInsensitive) == 0) {
        r.conc = 100.0;
        r.concUnit = "%";
        r.u3 = "100";
      }

      rows.push_back(r);
    }

    FileOut fcsv;
    fcsv.relativePath = QString("PlateMapHitLW/%1.csv").arg(testBarcode);
    fcsv.lines = renderPlateMapCSV(rows);
    fcsv.isAux = true;
    outs.push_back(std::move(fcsv));
  }

  // ---- Export experiment JSON alongside GWLs ----
  {
    FileOut fjson;
    fjson.relativePath = QString("Audit/experiment.json");
    const QString jsonPretty =
        QString::fromUtf8(QJsonDocument(exp).toJson(QJsonDocument::Indented));
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

bool FluentDriver::generateAux(const QJsonObject &,
                                              QVector<FileOut> &,
                                              QString *) const {
  return true;
}

// ============================
