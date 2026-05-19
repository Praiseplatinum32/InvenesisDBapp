#include "EvoDriver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <cmath>

#include "../services/gwl_helpers.h"
#include "../tecan_integration/gwlgenerator.h"

using FileOut = GWLGenerator::FileOut;
using StandardSource = GWLGenerator::StandardSource;
using VolumePlanEntry = GWLGenerator::VolumePlanEntry;
using CompoundSrc = GWLGenerator::CompoundSrc;
using Hit = GWLGenerator::Hit;
using Instrument = GWLGenerator::Instrument;
using namespace GWLHelpers;


EvoDriver::EvoDriver(const DilutionEngine& engine) : m_engine(engine) {}

bool EvoDriver::generate(const QJsonObject &,
                                           QVector<FileOut> &,
                                           QString *err) const {
  if (err)
    *err = "EVO150 backend not implemented yet.";
  return true;
}

bool EvoDriver::generateAux(const QJsonObject &,
                                              QVector<FileOut> &,
                                              QString *) const {
  return true;
}

// ========================== Standards Matrix Loading
// ==========================

