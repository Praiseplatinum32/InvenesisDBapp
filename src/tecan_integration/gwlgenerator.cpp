#include "gwlgenerator.h"
#include "../services/DilutionEngine.h"
#include "../hardware/EvoDriver.h"
#include "../hardware/FluentDriver.h"
#include <QDir>
#include <QFile>
#include <QTextStream>

GWLGenerator::GWLGenerator() : m_engine(std::make_unique<DilutionEngine>()) {}

GWLGenerator::GWLGenerator(double df, const QString &tid, double sc, Instrument instr)
    : dilutionFactor_(df), testId_(tid), stockConc_(sc), instrument_(instr),
      m_engine(std::make_unique<DilutionEngine>())
{
    if (instr == Instrument::FLUENT1080) {
        m_driver = std::make_unique<FluentDriver>(*m_engine);
    } else {
        m_driver = std::make_unique<EvoDriver>(*m_engine);
    }
}

GWLGenerator::~GWLGenerator() = default;

bool GWLGenerator::generate(const QJsonObject &experimentJson, QVector<FileOut> &outs, QString *err) const {
    if (!m_driver) return false;
    return m_driver->generate(experimentJson, outs, err);
}

bool GWLGenerator::generateAuxiliary(const QJsonObject &experimentJson, QVector<FileOut> &outs, QString *err) const {
    if (!m_driver) return false;
    return m_driver->generateAux(experimentJson, outs, err);
}

bool GWLGenerator::saveMany(const QString &rootDir, const QVector<FileOut> &outs, QString *err) {
    QDir d(rootDir);
    if (!d.exists() && !d.mkpath(".")) {
        if (err) *err = "Could not create root directory.";
        return false;
    }
    for (const auto &out : outs) {
        QFile f(d.absoluteFilePath(out.relativePath));
        QFileInfo fi(f);
        if (!fi.absoluteDir().exists() && !fi.absoluteDir().mkpath(".")) {
            if (err) *err = "Could not create sub-directory.";
            return false;
        }
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (err) *err = "Could not open file for writing.";
            return false;
        }
        QTextStream ts(&f);
        for (const QString &line : out.lines) {
            ts << line << "\n";
        }
    }
    return true;
}
