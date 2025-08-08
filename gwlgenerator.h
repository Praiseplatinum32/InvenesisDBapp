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

    // Top-level API
    QStringList generateTransferCommands(const QJsonObject &exp) const;

private:
    // Immutable ctor inputs
    const double      dilutionFactor_;
    const QString     testId_;
    const double      stockConcMicroM_;
    const QJsonObject volumeMap_;       // loaded from volumeMap.json

    struct Labware {
        QString label;  // deck label / device name
        QString rackID; // barcode (filled on the fly)
        QString type;   // device type
        int     site;   // deck site (unused in GWL but useful to carry)
    };

    // === Fixed labware definitions (adjust to your deck) ===
    const Labware stdMatrixLab_  = { "MPMatrixSTD",     "", "Matrix05Tube", 3 };
    const Labware cmpMatrixLab_  = { "MPMatrix",        "", "Matrix05Tube", 1 };
    const Labware daughterLab_   = { "MPDaughter",      "", "96daughterInv",1 };
    const Labware troughPreAspi_ = { "TroughDMSOPreAspi","", "Trough 200ml",2 };
    const Labware troughDilDMSO_ = { "TroughdilDMSO",   "", "Trough 200ml",3 };
    const Labware lavageLab_     = { "Lavage",          "", "Trough 200ml",1 };

    // === Planning helpers ===
    QJsonObject loadVolumeMap() const;
    QJsonArray  pickVolumeRules(const QString &testId,
                               double         stockConcMicroM) const;

    // Well helpers (96-well A1..H12)
    int  wellToIndex(const QString &well) const;      // 1..96, 0 if invalid
    bool isValidWell(const QString &well) const;

    // GWL record helpers
    QString recA(const Labware &lab, int pos,
                 double vol, const QString &lc,
                 const QString &tubeID = QString()) const;

    QString recD(const Labware &lab, int pos,
                 double vol, const QString &lc,
                 const QString &tubeID = QString()) const;

    QString recMultiAsp(const Labware &lab,
                        int start, int end,
                        double totalVol,
                        const QString &lc) const;

    QString recMultiDisp(const Labware &lab,
                         int start, int end,
                         double totalVol,
                         const QString &lc) const;

    QString recW(int scheme = 1) const;               // default in header only
    QString recC(const QString &text) const;

    // Block primitive for batched multi-channel transfers
    struct DispenseBlock {
        // 1) pre-asp (DMSO on all tips)
        double  preAspVol      = 0.0;
        QString preAspLC;

        // 2) diluent (per tip)
        Labware diluentLab;
        double  diluentVolPer  = 0.0;
        QString diluentLC;

        // 3) mother (single pos, replicated across tips)
        Labware motherLab;
        int     motherPos      = 0;     // 1..96
        double  motherVolPer   = 0.0;
        QString motherLC;

        // 4) multi-dispense to daughter
        Labware targetLab;
        int     tgtStart       = 0;     // start well index (1..96)
        int     tgtEnd         = 0;     // end   well index (>= start)
        double  mixVolPer      = 0.0;   // per tip (diluent + mother)
        QString mixLC;

        // 5) wash at end
        int     washScheme     = 1;
    };

    void addDispenseBlock(const DispenseBlock &b, QStringList &cmds) const;

    // Per-feature emitters
    QStringList makeStandardHitPick(const QJsonObject &stdObj,
                                    int plateNum) const;

    QStringList makeCompoundHitPick(const QJsonObject &cmpObj,
                                    int plateNum) const;

    QStringList makeSerialDilution(const QJsonObject &plateObj,
                                   const QJsonObject &cmpObj,
                                   int plateNum) const;
};

#endif // GWLGENERATOR_H
