#ifndef GWLGENERATOR_H
#define GWLGENERATOR_H

#include <memory>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>

class GWLGenerator
{
public:
    enum class Instrument {
        EVO150,
        FLUENT1080
    };

    struct FileOut {
        QString relativePath;
        QStringList lines;
        bool isAux = false;
    };

    struct VolumePlanEntry {
        double volMother = 0.0;
        double dmso = 0.0;
        double volDght = 0.0;
        double volFinal = 0.0;
        double finalConc = 0.0;
        double concMother = 0.0;
    };

    struct CompoundSrc {
        QString barcode;
        int position = 0;
    };

    struct Hit {
        QString dstWell;
        int dstIdx = 0;
        QString srcBarcode;
        int srcPos = 0;
    };

    struct StandardSource {
        QString barcode;
        QString well;
        double concentration = 0.0;
        QString concentrationUnit;
        QString sampleAlias;
        QString solutionId;
    };

    GWLGenerator();
    GWLGenerator(double dilutionFactor,
                 const QString &testId,
                 double stockConcMicroM,
                 Instrument instrument = Instrument::EVO150);
    ~GWLGenerator();

    bool generate(const QJsonObject &experimentJson,
                  QVector<FileOut> &outputs,
                  QString *errorMsg = nullptr) const;

    bool generateAuxiliary(const QJsonObject &experimentJson,
                           QVector<FileOut> &outputs,
                           QString *errorMsg = nullptr) const;

    static bool saveMany(const QString &rootDir,
                         const QVector<FileOut> &outputs,
                         QString *errorMsg = nullptr);

    // Helper methods
    bool loadVolumePlan(const QString &testId,
                        double stockConc,
                        VolumePlanEntry *out,
                        QString *err = nullptr) const;

    bool loadStandardsMatrix(QVector<StandardSource>& standards,
                             QString* err = nullptr) const;

    StandardSource selectBestStandard(const QString& standardName,
                                      double targetConc,
                                      const QVector<StandardSource>& available) const;

protected:
    QMap<QString, CompoundSrc> buildCompoundIndex(const QJsonArray &compounds) const;
    QVector<Hit> collectHitsFromDaughterLayout(const QJsonObject &daughter,
                                               const QMap<QString, CompoundSrc> &cmpIdx) const;
    QMap<QString, QVector<Hit>> groupHitsByMatrix(const QVector<Hit> &hits) const;

    static int tubePosFromWell(const QString &well);
    static int wellToIndex96(const QString &well);
    static bool isStandardLabel(const QString &s);
    static bool isDMSOLabel(const QString &s);

private:
    class Backend {
    public:
        Backend(const GWLGenerator &outer) : outer_(outer) {}
        virtual ~Backend() = default;
        virtual bool generate(const QJsonObject &exp,
                              QVector<FileOut> &outs,
                              QString *err) const = 0;
        virtual bool generateAux(const QJsonObject &exp,
                                 QVector<FileOut> &outs,
                                 QString *err) const = 0;
    protected:
        const GWLGenerator &outer_;
    };

    class Evo150Backend : public Backend {
    public:
        using Backend::Backend;
        bool generate(const QJsonObject &exp,
                      QVector<FileOut> &outs,
                      QString *err) const override;
        bool generateAux(const QJsonObject &exp,
                         QVector<FileOut> &outs,
                         QString *err) const override;
    };

    class FluentBackend : public Backend {
    public:
        using Backend::Backend;
        bool generate(const QJsonObject &exp,
                      QVector<FileOut> &outs,
                      QString *err) const override;
        bool generateAux(const QJsonObject &exp,
                         QVector<FileOut> &outs,
                         QString *err) const override;
    };

    double dilutionFactor_ = 3.16;
    QString testId_;
    double stockConc_ = 0.0;
    Instrument instrument_ = Instrument::EVO150;
    std::unique_ptr<Backend> backend_;
};

#endif // GWLGENERATOR_H
