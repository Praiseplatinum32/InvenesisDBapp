#pragma once

#include <QJsonObject>
#include <QVector>
#include <QString>
#include "../tecan_integration/gwlgenerator.h" // For FileOut struct

class ILiquidHandlerDriver {
public:
    virtual ~ILiquidHandlerDriver() = default;
    
    // Core GWL/hardware script generation
    virtual bool generate(const QJsonObject& experimentJson, QVector<GWLGenerator::FileOut>& outs, QString* err) const = 0;
    
    // Auxiliary files generation (maps, reports)
    virtual bool generateAux(const QJsonObject& experimentJson, QVector<GWLGenerator::FileOut>& outs, QString* err) const = 0;
};
