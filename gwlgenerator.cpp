#include "gwlgenerator.h"
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <algorithm>

// — ctor loads the map and remembers testId + stockConc
GWLGenerator::GWLGenerator(double dilutionFactor,
                           const QString &testId,
                           double         stockConcMicroM)
    : dilutionFactor_{dilutionFactor}
    , testId_{testId}
    , stockConcMicroM_{stockConcMicroM}
    , volumeMap_{ loadVolumeMap() }
{}

// — load the JSON resource containing your volumeMap
QJsonObject GWLGenerator::loadVolumeMap() const {
    QFile f(":/standardjson/jsonfile/volumeMap.json");
    if (!f.open(QIODevice::ReadOnly))
        return {};
    auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

// — pick the bucket whose key (as number) is closest to stockConcMicroM
QJsonArray
GWLGenerator::pickVolumeRules(const QString &testId,
                              double         stockConcMicroM) const
{
    if (!volumeMap_.contains(testId)) return {};

    QJsonObject buckets = volumeMap_.value(testId).toObject();
    double bestDiff = std::numeric_limits<double>::infinity();
    QString bestKey;

    for (const QString &k : buckets.keys()) {
        bool ok;
        double c = k.toDouble(&ok);
        if (!ok) continue;
        double d = qAbs(c - stockConcMicroM);
        if (d < bestDiff) {
            bestDiff = d;
            bestKey  = k;
        }
    }
    if (bestKey.isEmpty()) return {};

    return buckets.value(bestKey).toArray();
}

// — utility: A1=1, B1=2…H1=8, A2=9…H12=96
int GWLGenerator::wellToIndex(const QString &well) const {
    if (well.size() < 2) return 0;
    int col = well.mid(1).toInt();
    int row = well[0].unicode() - 'A' + 1;  // 'A'→1 … 'H'→8
    return (col - 1)*8 + row;
}

// — 7-field aspirate / dispense records
QString GWLGenerator::recA(const Labware &lab, int pos, double vol,
                           const QString &lc, const QString &tubeID) const
{
    return QString("A;%1;%2;%3;%4;%5;%6;%7")
    .arg(lab.label)
        .arg(lab.rackID)
        .arg(lab.type)
        .arg(pos)
        .arg(tubeID)
        .arg(QString::number(vol,'f',2))
        .arg(lc);
}

QString GWLGenerator::recD(const Labware &lab, int pos, double vol,
                           const QString &lc, const QString &tubeID) const
{
    return QString("D;%1;%2;%3;%4;%5;%6;%7")
    .arg(lab.label)
        .arg(lab.rackID)
        .arg(lab.type)
        .arg(pos)
        .arg(tubeID)
        .arg(QString::number(vol,'f',2))
        .arg(lc);
}

QString GWLGenerator::recW(int scheme) const {
    return scheme == 1 ? "W;" : QString("W%1;").arg(scheme);
}

QString GWLGenerator::recC(const QString &text) const {
    return "C;" + text;
}

// — standard hit-pick
QStringList
GWLGenerator::makeStandardHitPick(const QJsonObject &stdObj,
                                  int /*plateNum*/) const
{
    QStringList cmds;
    QString bc   = stdObj.value("Containerbarcode").toString();
    QString well = stdObj.value("Containerposition").toString();
    int     pos  = wellToIndex(well);

    cmds << recC("**************  Hit-Pick STD  **************")
         << recA(troughPreAspi_, 1,    50.0, "DMSO_pre_aspiration");

    {
        Labware lw = stdMatrixLab_;
        lw.rackID  = bc;
        cmds << recA(lw, pos, 30.0, "P00DMSO_copy_Matrix");
    }

    cmds << recD(daughterLab_, wellToIndex("A1"), 30.0, "DMSONOMixHitList")
         << recW();

    return cmds;
}

// — hit-pick compounds
QStringList
GWLGenerator::makeCompoundHitPick(const QJsonObject &cmpObj,
                                  int /*plateNum*/) const
{
    QStringList cmds;
    QString bc   = cmpObj.value("container_id").toString();
    QString well = cmpObj.value("well_id").toString();
    int     src  = wellToIndex(well);
    int     col  = well.mid(1).toInt();
    QString dWell = QString("A%1").arg(col);
    int     dst  = wellToIndex(dWell);

    cmds << recA(troughPreAspi_, 1, 50.0, "DMSO_pre_aspiration")
         << recA(troughDilDMSO_, 1, 15.0, "DMSO");

    {
        Labware lw = cmpMatrixLab_;
        lw.rackID  = bc;
        cmds << recA(lw, src, 10.0, "P00DMSO_copy_Matrix");
    }

    cmds << recD(daughterLab_, dst, 25.0, "DMSONOMixHitList")
         << recW();

    return cmds;
}

// — serial dilution, driven by volumeMap rules
QStringList
GWLGenerator::makeSerialDilution(const QJsonObject &plateObj,
                                 const QJsonObject &cmpObj,
                                 int /*plateNum*/) const
{
    QStringList cmds;

    // 1) build sorted path of wells for this compound
    QStringList path;
    QString product = cmpObj.value("product_name").toString();
    auto wellsObj   = plateObj.value("wells").toObject();

    for (const QString &w : wellsObj.keys())
        if (wellsObj.value(w).toString() == product)
            path << w;

    std::sort(path.begin(), path.end(),
              [this](const QString &a, const QString &b){
                  return wellToIndex(a) < wellToIndex(b);
              });

    // 2) pull the array of rules for this test+stock
    //    stockConcMicroM_ was passed in ctor
    QJsonArray rules = pickVolumeRules(testId_, stockConcMicroM_);

    // 3) for each step i→i+1, grab rule[i] or last rule if >count
    for (int i = 0; i + 1 < path.size(); ++i) {
        int   srcPos = wellToIndex(path[i]);
        int   dstPos = wellToIndex(path[i+1]);
        int   idx    = qMin(i, rules.count() - 1);
        auto rule = rules.at(idx).toObject();

        double volMother  = rule.value("VolMother").toDouble();
        double volDiluent = rule.value("DMSO").toDouble();
        double totalVol   = volMother + volDiluent;

        cmds << recA(troughPreAspi_, 1,               50.0,           "DMSO_pre_aspiration")
             << recA(troughDilDMSO_, 1,               volDiluent,     "DMSO")
             << recA(daughterLab_,     srcPos,        volMother,      "PinvDMSO")
             << recD(daughterLab_,     dstPos,        totalVol,       "DMSOMixHitList")
             << recW();
    }

    return cmds;
}

// — top‐level entry (no changes except ctor signature & passing testId/stockConc)
QStringList
GWLGenerator::generateTransferCommands(const QJsonObject &exp) const
{
    QStringList gwl;

    // unwrap
    auto stdObj    = exp.value("standard").toObject();
    auto plates    = exp.value("daughter_plates").toArray();
    auto comps     = exp.value("compounds").toArray();

    // 1) standard
    for (auto pval : plates) {
        auto plateObj = pval.toObject();
        gwl += makeStandardHitPick(stdObj, plateObj.value("plate_number").toInt());

        // 2) hit-pick compounds (only their own matrix)
        for (auto cval : comps) {
            auto cmpObj = cval.toObject();
            if (cmpObj.value("container_id").toString()
                != stdObj.value("Containerbarcode").toString())
                gwl += makeCompoundHitPick(cmpObj, plateObj.value("plate_number").toInt());
        }

        // 3) dilutions
        gwl << recC("**************  Hit-Pick Dilutions  **************");
        for (auto cval : comps) {
            auto cmpObj = cval.toObject();
            if (cmpObj.value("container_id").toString()
                != stdObj.value("Containerbarcode").toString())
                gwl += makeSerialDilution(plateObj, cmpObj, plateObj.value("plate_number").toInt());
        }
    }

    // 4) final extra‐wash
    gwl << recC("**********  Lavage supplementaire (scheme 2)  **********");
    for (int i = 1; i <= 8; ++i)
        gwl << recA(lavageLab_, i, 50.0, "LavageDMSO")
            << recW(2);

    return gwl;
}
