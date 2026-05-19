#pragma once

#include "ILiquidHandlerDriver.h"
#include "../services/DilutionEngine.h"

class EvoDriver : public ILiquidHandlerDriver {
public:
    explicit EvoDriver(const DilutionEngine& engine);
    ~EvoDriver() override = default;

    bool generate(const QJsonObject& experimentJson, QVector<GWLGenerator::FileOut>& outs, QString* err) const override;
    bool generateAux(const QJsonObject& experimentJson, QVector<GWLGenerator::FileOut>& outs, QString* err) const override;

private:
    const DilutionEngine& m_engine;
};
