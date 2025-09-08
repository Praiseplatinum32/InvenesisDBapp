#pragma once
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QVector>
#include <memory>

class GWLGenerator
{
public:
    enum class Instrument { EVO150, FLUENT1080 };

    struct Hit {
        QString srcBarcode;
        int     srcPos = 0;   // 1..96 (matrix tube index)
        QString dstWell;      // e.g. "B3"
        int     dstIdx = 0;   // 1..96 (Fluent position)
    };

    struct CompoundSrc {
        QString barcode;
        int     position = 0; // 1..96
    };

    struct FileOut {
        QString     relativePath; // path under an output root (folders like dght_0/…)
        QStringList lines;
        bool        isAux = false;
    };

    struct VolumePlanEntry {
        double volMother  = 0.0; // µL in each daughter well
        double dmso       = 0.0; // µL DMSO in starting well
        double volDght    = 0.0; // not used by Fluent generator here
        double volFinal   = 0.0; // not used by Fluent generator here
        double finalConc  = 0.0; // not used by Fluent generator here
        double concMother = 0.0; // not used by Fluent generator here
    };

    GWLGenerator();
    GWLGenerator(double dilutionFactor,
                 const QString &testId,
                 double stockConcMicroM,
                 Instrument instrument = Instrument::FLUENT1080);
    ~GWLGenerator();

    bool generate(const QJsonObject &root, QVector<FileOut> &outs, QString *err) const;
    bool generateAuxiliary(const QJsonObject &root, QVector<FileOut> &outs, QString *err) const;

    // Write all FileOuts under a chosen folder.
    static bool saveMany(const QString &rootDir, const QVector<FileOut> &outs, QString *err);

    // Shared helpers used by backends
    QMap<QString, CompoundSrc> buildCompoundIndex(const QJsonArray &compounds) const;
    QVector<Hit> collectHitsFromDaughterLayout(const QJsonObject &daughter,
                                               const QMap<QString, CompoundSrc> &cmpIdx) const;
    QMap<QString, QVector<Hit>> groupHitsByMatrix(const QVector<Hit> &hits) const;

    bool loadVolumePlan(const QString &testId, double stockConcMicroM,
                        VolumePlanEntry *out, QString *err) const;

    static int  tubePosFromWell(const QString &well);   // "D12" -> 1..96 (matrix)
    static int  wellToIndex96(const QString &well);     // "A1"  -> 1..96 (Fluent)
    static bool isStandardLabel(const QString &s);
    static bool isDMSOLabel(const QString &s);

private:
    class Backend {
    protected:
        explicit Backend(const GWLGenerator &outer) : outer_(outer) {}
        const GWLGenerator &outer_;
    public:
        virtual ~Backend() = default;
        virtual bool generate(const QJsonObject &root,
                              QVector<FileOut> &outs,
                              QString *err) const = 0;
        virtual bool generateAux(const QJsonObject &root,
                                 QVector<FileOut> &outs,
                                 QString *err) const = 0;
    };

    class Evo150Backend : public Backend {
    public:
        explicit Evo150Backend(const GWLGenerator &outer) : Backend(outer) {}
        bool generate(const QJsonObject &root, QVector<FileOut> &outs, QString *err) const override;
        bool generateAux(const QJsonObject &root, QVector<FileOut> &outs, QString *err) const override;
    private:
        void emitStandardAndDmso(QStringList &lines,
                                 const QJsonObject &root,
                                 const QJsonObject &daughter) const;
        void emitHitPicks(QStringList &lines,
                          const QString &matrixBarcode,
                          const QVector<Hit> &hits,
                          const QJsonObject &root,
                          const QJsonObject &daughter) const;
        void emitSerialDilutions(QStringList &lines,
                                 const QJsonObject &root,
                                 const QJsonObject &daughter) const;
    };

    class FluentBackend : public Backend {
    public:
        explicit FluentBackend(const GWLGenerator &outer) : Backend(outer) {}
        bool generate(const QJsonObject &root, QVector<FileOut> &outs, QString *err) const override;
        bool generateAux(const QJsonObject &root, QVector<FileOut> &outs, QString *err) const override;
    };

private:
    double     dilutionFactor_ = 3.16;           // e.g. 3.16
    QString    testId_;                           // e.g. "INV-T-005"
    double     stockConc_ = 0.0;                  // µM
    Instrument instrument_ = Instrument::FLUENT1080;
    std::unique_ptr<Backend> backend_;
};
