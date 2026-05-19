#include "DilutionEngine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <cmath>

#include "gwl_helpers.h"
#include "../tecan_integration/gwlgenerator.h"

using FileOut = GWLGenerator::FileOut;
using StandardSource = GWLGenerator::StandardSource;
using VolumePlanEntry = GWLGenerator::VolumePlanEntry;
using CompoundSrc = GWLGenerator::CompoundSrc;
using Hit = GWLGenerator::Hit;
using Instrument = GWLGenerator::Instrument;
using namespace GWLHelpers;


bool DilutionEngine::loadStandardsMatrix(QVector<StandardSource> &standards,
                                       QString *err) const {
  QFile f(":/data/resources/data/standards_matrix.json");
  if (!f.open(QIODevice::ReadOnly)) {
    if (err)
      *err = "Cannot open standards_matrix.json";
    return false;
  }

  const auto doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isArray()) {
    if (err)
      *err = "standards_matrix.json is not an array";
    return false;
  }

  const auto arr = doc.array();
  for (const auto &val : arr) {
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
      if (!ok)
        src.concentration = 0.0;
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

StandardSource DilutionEngine::selectBestStandard(
    const QString &standardName, double targetConc,
    const QVector<StandardSource> &available) const {
  // Scoring rules (lower is better):
  //   - Stock >= target: score = ratio (prefer closest above target, up to
  //   kPenaltyThreshold x)
  //   - Score > kPenaltyThreshold: apply kOverConcPenalty to discourage very
  //   dilute stocks
  //   - Stock < target: score = kUnderstockPenalty + ratio (hard preference for
  //   sufficient stock)
  static constexpr double kPenaltyThreshold =
      100.0; // ratio above which stock is "too concentrated"
  static constexpr double kOverConcPenalty =
      0.1; // dampening factor for over-concentrated stocks
  static constexpr double kUnderstockPenalty =
      1000.0; // strong penalty when stock < target

  StandardSource best;
  double bestScore = 1e300;

  for (const auto &src : available) {
    if (src.sampleAlias.compare(standardName, Qt::CaseInsensitive) != 0)
      continue;
    if (src.concentration <= 0.0)
      continue;

    double score = 0.0;
    if (src.concentration >= targetConc) {
      score = src.concentration / targetConc;
      if (score > kPenaltyThreshold)
        score =
            kPenaltyThreshold + (score - kPenaltyThreshold) * kOverConcPenalty;
    } else {
      score = kUnderstockPenalty + (targetConc / src.concentration);
    }

    if (score < bestScore) {
      bestScore = score;
      best = src;
    }
  }

  return best;
}

// The standard must be 10 x more concentrated than the highest starting
// concentration in the test so that the first serial dilution step lands at the
// correct value.
static constexpr double kStandardConcFactor = 10.0;

// ========================== Fluent backend ================================

QMap<QString, CompoundSrc> DilutionEngine::buildCompoundIndex(const QJsonArray &compounds) const {
  QMap<QString, CompoundSrc> idx;
  for (const auto &vv : compounds) {
    const auto o = vv.toObject();
    const QString n = o.value("product_name").toString().trimmed();
    const QString bc = o.value("container_id").toString().trimmed();
    const QString w = o.value("well_id").toString().trimmed();
    if (n.isEmpty() || bc.isEmpty() || w.isEmpty())
      continue;
    CompoundSrc s;
    s.barcode = bc;
    s.position = tubePosFromWell(w);
    if (s.position > 0)
      idx.insert(n, s);
  }
  return idx;
}

QVector<Hit> DilutionEngine::collectHitsFromDaughterLayout(
    const QJsonObject &daughter,
    const QMap<QString, CompoundSrc> &cmpIdx) const {
  QVector<Hit> hits;
  const QJsonObject wells = daughter.value("wells").toObject();

  for (const QString &dstWell : wells.keys()) {
    const QString who = wells.value(dstWell).toString().trimmed();
    if (who.isEmpty() || isStandardLabel(who) || isDMSOLabel(who))
      continue;
    if (!cmpIdx.contains(who))
      continue;

    Hit h;
    h.dstWell = dstWell;
    h.dstIdx = wellToIndex96(dstWell);
    h.srcBarcode = cmpIdx[who].barcode;
    h.srcPos = cmpIdx[who].position;
    hits.push_back(h);
  }
  std::sort(hits.begin(), hits.end(),
            [](const Hit &a, const Hit &b) { return a.dstIdx < b.dstIdx; });
  return hits;
}

QMap<QString, QVector<Hit>>
DilutionEngine::groupHitsByMatrix(const QVector<Hit> &hits) const {
  QMap<QString, QVector<Hit>> g;
  for (const auto &h : hits)
    g[h.srcBarcode].push_back(h);
  return g;
}

bool DilutionEngine::loadVolumePlan(const QJsonObject &catalogue,
                                  const QString &testId, double stockConcMicroM,
                                  double startingConcInTestMicroM,
                                  VolumePlanEntry *out, QString *err, double overrideMinVol) const {
  // -----------------------------------------------------------------------
  // Uses the pre-loaded invenesis_catalogue (passed in by the caller) to find
  // the test plate transfer parameters, then:
  //   1. Back-calculates the required concentration in the daughter plate.
  //   2. Applies volume rules to determine volMother and dmso.
  //
  // Volume rules:
  //   - Minimum compound volume pipettable by the Tecan: 1 µL
  //   - Default total well volume in daughter plate: kDefaultWellVolumeUL (15
  //   µL)
  //   - If vol_from_daughter * kSafetyMultiplier > kDefaultWellVolumeUL:
  //       target = vol_from_daughter * kSafetyMultiplier * kSafetyHeadroom
  //   - If the 1 µL minimum forces total > target, scale total up accordingly
  // -----------------------------------------------------------------------
  static constexpr double kDefaultWellVolumeUL =
      15.0; // µL, default daughter plate well volume
  static constexpr double kSafetyMultiplier =
      3.0; // safety factor on vol_from_daughter
  static constexpr double kSafetyHeadroom =
      1.1; // 10 % extra above the safety multiple
  static constexpr double kMinCompoundVolumeUL =
      1.0; // minimum pipettable compound volume

  if (catalogue.isEmpty()) {
    if (err)
      *err = "Catalogue not loaded — call loadVolumePlan after loading "
             "invenesis_catalogue.json";
    return false;
  }

  const auto entry = catalogue.value(testId).toObject();
  if (entry.isEmpty()) {
    if (err)
      *err = QString("Test '%1' not found in catalogue").arg(testId);
    return false;
  }

  const double volFromDaughter =
      entry.value("vol_from_daughter_\u00b5l").toDouble();
  const bool isTarsal = entry.value("is_tarsal").toBool();

  if (volFromDaughter <= 0.0) {
    if (err)
      *err =
          QString(
              "vol_from_daughter is 0 for test '%1' — cannot compute volumes")
              .arg(testId);
    return false;
  }
  if (stockConcMicroM <= 0.0) {
    if (err)
      *err = "Stock concentration is 0 — cannot compute volumes";
    return false;
  }
  if (startingConcInTestMicroM <= 0.0) {
    if (err)
      *err =
          "Starting concentration in test plate is 0 — cannot compute volumes";
    return false;
  }

  // --- Step 1: back-calculate required concentration in daughter plate ---
  double concDaughter_µM = 0.0;
  double volFinal = 0.0;

  if (!isTarsal) {
    const double totalWellVol =
        entry.value("total_well_vol_\u00b5l").toDouble();
    if (totalWellVol <= 0.0) {
      if (err)
        *err = QString("total_well_vol is 0 for test '%1'").arg(testId);
      return false;
    }
    // concDaughter [µM] = startingConcInTest [µM] * totalWellVol /
    // volFromDaughter
    concDaughter_µM = startingConcInTestMicroM * totalWellVol / volFromDaughter;
    volFinal = totalWellVol;
  } else {
    // Tarsal: client orders in µmol/m².
    // concDaughter [mM] = target [µmol/m²] * well_area [m²] / (volFromDaughter
    // [µL] * 1e-3) concDaughter [µM] = concDaughter [mM] * 1000
    const double wellArea = entry.value("well_area_m2").toDouble();
    if (wellArea <= 0.0) {
      if (err)
        *err = QString("well_area_m2 is 0 for tarsal test '%1'").arg(testId);
      return false;
    }
    const double concDaughter_mM =
        startingConcInTestMicroM * wellArea / (volFromDaughter * 1e-3);
    concDaughter_µM = concDaughter_mM * 1000.0;
    volFinal = 0.0; // not a liquid-volume concept for tarsal
  }

  // --- Step 2: determine target total volume in daughter plate well ---
  double defaultWellVol = overrideMinVol > 0.0 ? overrideMinVol : kDefaultWellVolumeUL;
  double targetTotal = defaultWellVol;
  if (volFromDaughter * kSafetyMultiplier > defaultWellVol)
    targetTotal = volFromDaughter * kSafetyMultiplier * kSafetyHeadroom;

  // vol_compound / targetTotal = concDaughter / stockConc
  double volCompound = targetTotal * concDaughter_µM / stockConcMicroM;

  if (volCompound < kMinCompoundVolumeUL) {
    // Scale total up until vol_compound reaches the minimum, keeping
    // targetTotal as a lower bound so we never produce a smaller well volume
    // than the default.
    const double minTotalFor1ul = stockConcMicroM / concDaughter_µM;
    targetTotal = std::max(targetTotal, minTotalFor1ul);
    volCompound = targetTotal * concDaughter_µM / stockConcMicroM;
  }

  const double volDmso = targetTotal - volCompound;

  VolumePlanEntry vp;
  vp.volMother = targetTotal;
  vp.dmso = volDmso;
  vp.volDght = volFromDaughter;
  vp.volFinal = volFinal;
  vp.finalConc = startingConcInTestMicroM;
  vp.concMother = concDaughter_µM;

  qDebug() << "[VolumePlan]" << testId
           << "startConc=" << startingConcInTestMicroM << "uM"
           << "stock=" << stockConcMicroM << "uM"
           << "concDaughter=" << concDaughter_µM << "uM"
           << "totalWell=" << targetTotal << "uL"
           << "compound=" << volCompound << "uL"
           << "DMSO=" << volDmso << "uL";

  if (out)
    *out = vp;
  return true;
}

int DilutionEngine::tubePosFromWell(const QString &well) {
  if (well.size() < 2)
    return 0;
  const QChar rowCh = well.at(0).toUpper();
  bool ok = false;
  int col = well.mid(1).toInt(&ok);
  if (!ok || col < 1 || col > 12)
    return 0;
  int row = rowCh.unicode() - QChar('A').unicode();
  if (row < 0 || row > 7)
    return 0;
  return row * 12 + col; // 1..96
}

int DilutionEngine::wellToIndex96(const QString &well) {
  if (well.size() < 2)
    return -1;
  const QChar rowCh = well.at(0).toUpper();
  bool ok = false;
  int col = well.mid(1).toInt(&ok);
  if (!ok || col < 1 || col > 12)
    return -1;
  int row = rowCh.unicode() - QChar('A').unicode();
  if (row < 0 || row > 7)
    return -1;
  return (col - 1) * 8 + row + 1; // 1..96
}

bool DilutionEngine::isStandardLabel(const QString &s) {
  const QString t = s.trimmed().toLower();
  return (t == "standard" || t == "std");
}

bool DilutionEngine::isDMSOLabel(const QString &s) {
  const QString t = s.trimmed().toLower();
  return (t == "dmso");
}

