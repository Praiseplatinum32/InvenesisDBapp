#ifndef GWLGENERATOR_H
#define GWLGENERATOR_H

#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

class GWLGenerator
{
public:
    /**
     * @param dilutionFactor    the numeric factor (e.g. 3.16)
     * @param testId            the assay ID (e.g. "INV-T-005")
     * @param stockConcMicroM   the stock concentration in µM (e.g. 10000.0 for 10 mM)
     */
    GWLGenerator(double dilutionFactor,
                 const QString &testId,
                 double         stockConcMicroM);

    QStringList generateTransferCommands(const QJsonObject &exp) const;

private:
    const double      dilutionFactor_;
    const QString     testId_;
    const double      stockConcMicroM_;
    const QJsonObject volumeMap_;       // loaded from volumeMap.json

    struct Labware {
        QString label;
        QString rackID;
        QString type;
        int     site;
    };

    // your fixed labware definitions:
    const Labware stdMatrixLab_  = { "MPMatrixSTD",   "", "Matrix05Tube", 3 };
    const Labware cmpMatrixLab_  = { "MPMatrix",      "", "Matrix05Tube", 1 };
    const Labware daughterLab_   = { "MPDaughter",    "", "96daughterInv",1 };
    const Labware troughPreAspi_ = { "TroughDMSOPreAspi","","Trough 200ml",2 };
    const Labware troughDilDMSO_ = { "TroughdilDMSO","",    "Trough 200ml",3 };
    const Labware lavageLab_     = { "Lavage",         "",   "Trough 200ml",1 };

    // Helpers
    QJsonArray pickVolumeRules(const QString &testId,
                               double         stockConcMicroM) const;
    int        wellToIndex(const QString &well) const;
    QString    recA(const Labware &lab, int pos,
                 double vol, const QString &lc,
                 const QString &tubeID = QString()) const;
    QString    recD(const Labware &lab, int pos,
                 double vol, const QString &lc,
                 const QString &tubeID = QString()) const;
    QString    recW(int scheme = 1)                                    const;
    QString    recC(const QString &text)                               const;

    QStringList makeStandardHitPick(const QJsonObject &stdObj,
                                    int plateNum)                     const;
    QStringList makeCompoundHitPick(const QJsonObject &cmpObj,
                                    int plateNum)                     const;
    QStringList makeSerialDilution(const QJsonObject &plateObj,
                                   const QJsonObject &cmpObj,
                                   int                 plateNum)    const;
    QJsonObject loadVolumeMap()                                      const;
};

#endif // GWLGENERATOR_H
