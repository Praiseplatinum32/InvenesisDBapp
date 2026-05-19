#include "gwl_helpers.h"
#include <cmath>

namespace GWLHelpers {


// normalize "A01"/"a1" -> "A1"
QString normWell(const QString &s) {
  if (s.isEmpty())
    return s;
  QString t = s.trimmed().toUpper();
  if (t.size() < 2)
    return t;
  QChar row = t.at(0);
  bool ok = false;
  int col = t.mid(1).toInt(&ok);
  if (!ok)
    return t;
  return QString("%1%2").arg(row).arg(col);
}

// "A1" -> "A01" for CSV
QString toA01(const QString &s) {
  const QString n = normWell(s);
  if (n.size() < 2)
    return n;
  const QChar row = n.at(0);
  bool ok = false;
  int col = n.mid(1).toInt(&ok);
  if (!ok)
    return n;
  return QString("%1%2").arg(row).arg(col, 2, 10, QChar('0'));
}

// A1..H12 <-> 1..96 (column-major: down A..H then next column)
int wellNameToIndex96(const QString &w) {
  const QString n = normWell(w);
  if (n.size() < 2)
    return -1;
  const QChar rowCh = n.at(0);
  bool ok = false;
  int col = n.mid(1).toInt(&ok);
  if (!ok || col < 1 || col > 12)
    return -1;
  int row = rowCh.unicode() - QChar('A').unicode();
  if (row < 0 || row > 7)
    return -1;
  return (col - 1) * 8 + row + 1; // 1..96
}

QString indexToWellName96(int idx) {
  if (idx < 1 || idx > 96)
    return {};
  const int zero = idx - 1;
  const int row = zero % 8;     // 0..7 -> A..H
  const int col = zero / 8 + 1; // 1..12
  const QChar rowCh = QChar('A' + row);
  return QString("%1%2").arg(rowCh).arg(col);
}

QSet<int> fullPlate96Indices() {
  QSet<int> s;
  for (int i = 1; i <= 96; ++i)
    s.insert(i);
  return s;
}

QSet<int> namesToIndices(const QSet<QString> &wells) {
  QSet<int> out;
  for (const auto &w : wells) {
    int idx = wellNameToIndex96(w);
    if (idx >= 1)
      out.insert(idx);
  }
  return out;
}

// Aspirate/dispense volumes are always rounded UP: we must never
// under-aspirate.
double roundUp01(double v) {
  if (v <= 0.0)
    return 0.0;
  return std::ceil(v * 10.0) / 10.0; // e.g. 60.52 -> 60.6
}

// Transfer volumes (serial dilution) are rounded to nearest 0.1 µL.
// Always rounding up would accumulate a systematic upward bias across 12
// dilution steps, shifting the effective dilution factor on every step.
double roundNearest01(double v) {
  if (v <= 0.0)
    return 0.0;
  return std::round(v * 10.0) / 10.0; // e.g. 4.74 -> 4.7, 4.75 -> 4.8
}

// Maximum volume for a single aspirate with 350 µL tips (leave 10 µL
// head-room). This is now dynamically calculated based on tip size.

// For Fluent: Generate A;/D; lines (one A then one D)
void appendADFluent(QStringList &out, const QString &srcLabel,
                           int srcPos, const QString &dstLabel, int dstPos,
                           double volUL, const QString &liqClass) {
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
void appendADFluentOneShot(QStringList &out, const QString &srcLabel,
                                  int srcPos, const QString &dstLabel,
                                  int dstPos, double volUL,
                                  const QString &liqClass) {
  appendADFluent(out, srcLabel, srcPos, dstLabel, dstPos, volUL, liqClass);
  out << "W;";
}

// One aspirate, many dispenses (for DMSO multi-dispense)
void appendAThenManyD(QStringList &out, const QString &srcLabel,
                             int srcPos, const QString &dstLabel,
                             const QList<int> &dstPositions, double perWellUL,
                             const QString &liqClass) {
  if (dstPositions.isEmpty())
    return;
  const double vPer = roundUp01(perWellUL);
  if (vPer <= 0)
    return;

  const double vTotal = roundUp01(vPer * dstPositions.size());
  const QString totStr = QString::number(vTotal, 'f', 1);
  const QString perStr = QString::number(vPer, 'f', 1);

  out << QString("A;%1;;;%2;;%3;%4")
             .arg(srcLabel)
             .arg(srcPos)
             .arg(totStr)
             .arg(liqClass);

  for (int pos : dstPositions) {
    out << QString("D;%1;;;%2;;%3;%4")
               .arg(dstLabel)
               .arg(pos)
               .arg(perStr)
               .arg(liqClass);
  }
}

// number_of_dilutions default 3
int numberOfDilutionsFromJson(const QJsonObject &exp) {
  const auto arr = exp.value("test_requests").toArray();
  if (arr.isEmpty())
    return 3;
  const auto tr0 = arr.at(0).toObject();
  bool ok = false;
  int n = tr0.value("number_of_dilutions").toString().toInt(&ok);
  if (!ok)
    n = tr0.value("number_of_dilutions").toInt();
  return (n > 0 ? n : 3);
}

// pull double (supports string)
double readDouble(const QJsonObject &o, const char *key,
                         double defVal) {
    QJsonValue v = o.value(key);
    if (v.isUndefined() || v.isNull()) return defVal;
    return v.isString() ? v.toString().toDouble() : v.toVariant().toDouble();
}

// Build standard chains from layout (contiguous "Standard" along +8 or +1)
QVector<QStringList>
buildStandardChainsFromLayout(const QJsonObject &wellsObj) {
  QSet<QString> stdSet;
  for (auto it = wellsObj.begin(); it != wellsObj.end(); ++it) {
    if (it.value().toString().trimmed().compare("Standard",
                                                Qt::CaseInsensitive) == 0)
      stdSet.insert(normWell(it.key()));
  }
  if (stdSet.isEmpty())
    return {};

  QStringList sorted = QStringList(stdSet.values());
  std::sort(sorted.begin(), sorted.end(),
            [](const QString &a, const QString &b) {
              return wellNameToIndex96(a) < wellNameToIndex96(b);
            });

  auto isStd = [&](const QString &w) -> bool {
    return stdSet.contains(normWell(w));
  };

  QSet<QString> visited;
  QVector<QStringList> chains;

  for (const QString &start : sorted) {
    if (visited.contains(start))
      continue;

    const int startIdx = wellNameToIndex96(start);
    const QString prev8 = indexToWellName96(startIdx - 8);
    const QString prev1 = indexToWellName96(startIdx - 1);
    if (isStd(prev8) || isStd(prev1))
      continue; // not a chain start

    int step = 0;
    if (isStd(indexToWellName96(startIdx + 8)))
      step = 8;
    else if (isStd(indexToWellName96(startIdx + 1)))
      step = 1;

    QStringList chain;
    chain << start;
    visited.insert(start);
    if (step != 0) {
      int cur = startIdx;
      while (true) {
        const int nxt = cur + step;
        const QString wn = indexToWellName96(nxt);
        if (!isStd(wn))
          break;
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



QStringList renderPlateMapCSV(const QList<DaughterPlateEntry> &rows) {
  QStringList L;
  L << "Containerbarcode,Samplealias,Containerposition,Volume,VolumeUnit,"
       "Concentration,ConcentrationUnit,UserdefValue1,UserdefValue2,"
       "UserdefValue3,UserdefValue4,UserdefValue5";
  for (const auto &r : rows) {
    const QString volStr = QString::number(roundUp01(r.volumeUL), 'f', 1);
    L << QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12")
             .arg(r.containerBarcode, r.sampleAlias, r.wellA01, volStr,
                  r.volumeUnit, QString::number(r.conc, 'f', 4), r.concUnit,
                  r.u1, r.u2, r.u3, r.u4, r.u5);
  }
  return L;
}

// ---------------------- Audit CSV rows & renderers ------------------------





QStringList renderSeedAuditCSV(const QList<SeedAuditRow> &rows) {
  QStringList L;
  L << "Daughter,Analyte,MatrixBarcode,MatrixWell,StartWell,SeedVolume_uL,"
       "Notes";
  for (const auto &r : rows) {
    L << QString("%1,%2,%3,%4,%5,%6,%7")
             .arg(r.daughterBarcode, r.analyte, r.matrixBarcode,
                  toA01(r.matrixWell), toA01(r.startWell),
                  QString::number(roundUp01(r.seedVolumeUL), 'f', 1), r.notes);
  }
  return L;
}

QStringList renderDilutionAuditCSV(const QList<DilutionAuditRow> &rows) {
  QStringList L;
  L << "Daughter,Analyte,From,To,Transfer_uL,Notes";
  for (const auto &r : rows) {
    L << QString("%1,%2,%3,%4,%5,%6")
             .arg(r.daughterBarcode, r.analyte, toA01(r.srcWell),
                  toA01(r.dstWell),
                  QString::number(roundUp01(r.transferUL), 'f', 1), r.notes);
  }
  return L;
}

// -------- Direction parsing (LTR default) --------
bool dmsoDispenseRightToLeft(const QJsonObject &exp) {
  const QString s = exp.value("dmso_direction").toString().trimmed().toLower();
  if (s == "ltr" || s == "left-to-right" || s == "left" || s == "0")
    return false;
  if (s == "rtl" || s == "right-to-left" || s == "right" || s == "1")
    return true;
  // Default: LTR
  return false;
}

// Row/Col from 1..96 index
int rowFromIndex96(int idx) {
  return (idx - 1) % 8;
} // 0..7 (A..H)
int colFromIndex96(int idx) { return (idx - 1) / 8 + 1; } // 1..12

// One aspirate, many dispenses (varying per-dispense volumes)
void appendAThenManyD_Vary(
    QStringList &out, const QString &srcLabel, int srcPos,
    const QString &dstLabel,
    const QList<QPair<int, double>> &posVols, // [(destIdx, volUL_rounded)]
    const QString &liqClass) {
  double total = 0.0; // already-rounded sum
  for (const auto &pv : posVols)
    total += pv.second;

  const QString vStr = QString::number(roundUp01(total), 'f', 1);
  out << QString("A;%1;;;%2;;%3;%4")
             .arg(srcLabel)
             .arg(srcPos)
             .arg(vStr)
             .arg(liqClass);

  for (const auto &pv : posVols) {
    const QString dVol = QString::number(pv.second, 'f', 1);
    out << QString("D;%1;;;%2;;%3;%4")
               .arg(dstLabel)
               .arg(pv.first)
               .arg(dVol)
               .arg(liqClass);
  }
}


} // namespace GWLHelpers
