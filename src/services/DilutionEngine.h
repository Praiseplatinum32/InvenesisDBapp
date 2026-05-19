#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QString>
#include <QVector>
#include "../tecan_integration/gwlgenerator.h" // For structs: StandardSource, VolumePlanEntry, CompoundSrc, Hit

class DilutionEngine {
public:
    DilutionEngine() = default;
    ~DilutionEngine() = default;

    // Catalogue / volume plan helpers
    bool loadStandardsMatrix(QVector<GWLGenerator::StandardSource>& standards, QString* err = nullptr) const;
    GWLGenerator::StandardSource selectBestStandard(const QString& standardName, double targetConc, const QVector<GWLGenerator::StandardSource>& available) const;
    bool loadVolumePlan(const QJsonObject& catalogue, const QString& testId, double stockConcMicroM, double startingConcInTestMicroM, GWLGenerator::VolumePlanEntry* out, QString* err = nullptr, double overrideMinVol = -1.0) const;

    // Utility helpers
    QMap<QString, GWLGenerator::CompoundSrc> buildCompoundIndex(const QJsonArray& compounds) const;
    QVector<GWLGenerator::Hit> collectHitsFromDaughterLayout(const QJsonObject& daughter, const QMap<QString, GWLGenerator::CompoundSrc>& cmpIdx) const;
    QMap<QString, QVector<GWLGenerator::Hit>> groupHitsByMatrix(const QVector<GWLGenerator::Hit>& hits) const;

    // Well-ID helpers
    static int tubePosFromWell(const QString& well);
    static int wellToIndex96(const QString& well);
    static bool isStandardLabel(const QString& s);
    static bool isDMSOLabel(const QString& s);
};
