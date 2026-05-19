#pragma once
#include <QString>
#include <QStringList>
#include <QSet>
#include <QList>
#include <QPair>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>

namespace GWLHelpers {
    static constexpr double kStandardConcFactor = 10.0;

    QString normWell(const QString &s);
    QString toA01(const QString &s);
    int wellNameToIndex96(const QString &w);
    QString indexToWellName96(int idx);
    QSet<int> fullPlate96Indices();
    QSet<int> namesToIndices(const QSet<QString> &wells);
    double roundUp01(double v);
    double roundNearest01(double v);
    
    void appendADFluent(QStringList &out, const QString &srcLabel, int srcPos, const QString &dstLabel, int dstPos, double volUL, const QString &liqClass);
    void appendADFluentOneShot(QStringList &out, const QString &srcLabel, int srcPos, const QString &dstLabel, int dstPos, double volUL, const QString &liqClass);
    void appendAThenManyD(QStringList &out, const QString &srcLabel, int srcPos, const QString &dstLabel, const QList<int> &dstPositions, double perWellUL, const QString &liqClass);
    void appendAThenManyD_Vary(QStringList &out, const QString &srcLabel, int srcPos, const QString &dstLabel, const QList<QPair<int, double>> &posVols, const QString &liqClass);
    
    int numberOfDilutionsFromJson(const QJsonObject &exp);
    double readDouble(const QJsonObject &o, const char *key, double def = 0.0);
    QVector<QStringList> buildStandardChainsFromLayout(const QJsonObject &wellsObj);
    
    struct DaughterPlateEntry {
        QString containerBarcode;
        QString sampleAlias;
        QString wellA01;
        double volumeUL = 0.0;
        QString volumeUnit = "ul";
        double conc = 0.0;
        QString concUnit;
        QString u1, u2, u3, u4, u5;
    };
    QStringList renderPlateMapCSV(const QList<DaughterPlateEntry> &rows);
    
    struct SeedAuditRow {
        QString daughterBarcode;
        QString analyte;
        QString matrixBarcode;
        QString matrixWell;
        QString startWell;
        double seedVolumeUL = 0;
        QString notes;
    };
    QStringList renderSeedAuditCSV(const QList<SeedAuditRow> &rows);
    
    struct DilutionAuditRow {
        QString daughterBarcode;
        QString analyte;
        QString srcWell;
        QString dstWell;
        double transferUL = 0;
        QString notes;
    };
    QStringList renderDilutionAuditCSV(const QList<DilutionAuditRow> &rows);
    
    bool dmsoDispenseRightToLeft(const QJsonObject &exp);
    int rowFromIndex96(int idx);
    int colFromIndex96(int idx);
}
