#pragma once

#include <memory>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>

class ILiquidHandlerDriver;
class DilutionEngine;

class GWLGenerator
{
public:
    enum class Instrument { EVO150, FLUENT1080 };

    struct FileOut {
        QString     relativePath;
        QStringList lines;
        bool        isAux = false; 
    };

    struct VolumePlanEntry {
        double volMother  = 0.0;
        double dmso       = 0.0;
        double volDght    = 0.0;
        double volFinal   = 0.0;
        double finalConc  = 0.0;
        double concMother = 0.0;
    };

    struct CompoundSrc {
        QString barcode;
        int     position = 0;
    };

    struct Hit {
        QString dstWell;
        int     dstIdx   = -1;
        QString srcBarcode;
        int     srcPos   = 0;
    };

    struct StandardSource {
        QString barcode;
        QString well;
        QString sampleAlias;
        QString solutionId;
        double  concentration     = 0.0;
        QString concentrationUnit;
    };

    GWLGenerator();

    GWLGenerator(double         dilutionFactor,
                 const QString &testId,
                 double         stockConcMicroM,
                 Instrument     instrument);

    ~GWLGenerator();

    bool generate(const QJsonObject &experimentJson,
                  QVector<FileOut>  &outs,
                  QString           *err = nullptr) const;

    bool generateAuxiliary(const QJsonObject &experimentJson,
                           QVector<FileOut>  &outs,
                           QString           *err = nullptr) const;

    static bool saveMany(const QString        &rootDir,
                         const QVector<FileOut> &outs,
                         QString              *err = nullptr);

private:
    double      dilutionFactor_ = 3.16;
    QString     testId_;
    double      stockConc_  = 0.0;
    Instrument  instrument_ = Instrument::EVO150;

    std::unique_ptr<DilutionEngine> m_engine;
    std::unique_ptr<ILiquidHandlerDriver> m_driver;
};
