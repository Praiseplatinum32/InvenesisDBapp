#ifndef GWLGENERATOR_H
#define GWLGENERATOR_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMap>

class QWidget;

/**
 * @brief Generates EVOware‑compatible GWL transfer scripts for an
 *        Invenesis daughter‑plate experiment.
 *
 * 2025‑05 refactor:
 *   • chooses mother/DMSO volumes from volumeMap.json
 *   • builds serial‑dilution chains
 *   • groups up to 8 compounds per aspirate/dispense cycle
 */
class GWLGenerator
{
public:
    explicit GWLGenerator(double dilutionFactor = 3.16);

    QStringList generateTransferCommands(const QJsonObject &experiment) const;

    static bool saveToFile(const QStringList &lines,
                           const QString     &defaultName,
                           QWidget           *parent = nullptr);

private:
    /* ---- mapping helpers ---- */
    QJsonObject loadVolumeMap() const;
    QJsonObject pickVolumeRule(const QString      &testId,
                               double              stockConc,
                               const QJsonObject  &map,
                               QStringList        *warnings) const;

    QMap<QString,QJsonObject> buildCompoundIndex(const QJsonArray &compounds) const;
    QMap<QString,QString>     buildTestIndex(const QJsonArray &testRequests) const;

    /* ---- command generators ---- */
    QStringList makeFirstDilution(const QString &compound,
                                  const QString &dstWell,
                                  int            plateNumber,
                                  const QJsonObject &cmp,
                                  const QJsonObject &rule) const;

    QStringList makeSerialDilution(const QString &rowLetter,
                                   int            startCol,
                                   int            plateNumber,
                                   int            steps,
                                   double         firstVol) const;

    QStringList makeEightChannelBlock(const QStringList &singleTipCmds) const;

private:
    const double dilutionFactor_;          // default 3.16
    const double totalWellVol_ = 50.0;     // daughter‑well working volume
};

#endif // GWLGENERATOR_H
