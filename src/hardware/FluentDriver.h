#pragma once

#include "ILiquidHandlerDriver.h"
#include "../services/DilutionEngine.h"

class FluentDriver : public ILiquidHandlerDriver {
public:
    explicit FluentDriver(const DilutionEngine& engine);
    ~FluentDriver() override = default;

    bool generate(const QJsonObject& experimentJson, QVector<GWLGenerator::FileOut>& outs, QString* err) const override;
    bool generateAux(const QJsonObject& experimentJson, QVector<GWLGenerator::FileOut>& outs, QString* err) const override;

private:
    const DilutionEngine& m_engine;
};
