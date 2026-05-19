#include "TecanViewModel.h"

TecanViewModel::TecanViewModel(QObject *parent)
    : QObject(parent),
      m_experimentManager(std::make_unique<ExperimentManager>(this)),
      m_daughterPlateType(ExperimentJsonSerializer::DaughterPlateType::Plate96)
{
}

TecanViewModel::~TecanViewModel() = default;

ExperimentJsonSerializer::DaughterPlateType TecanViewModel::getDaughterPlateType() const {
    return m_daughterPlateType;
}

void TecanViewModel::setDaughterPlateType(ExperimentJsonSerializer::DaughterPlateType type) {
    if (m_daughterPlateType != type) {
        m_daughterPlateType = type;
        emit plateTypeChanged();
    }
}

QJsonObject TecanViewModel::getQcPlatesJson() const {
    return m_qcPlatesJson;
}

void TecanViewModel::setQcPlatesJson(const QJsonObject &json) {
    m_qcPlatesJson = json;
    emit qcPlatesChanged();
}

QJsonObject TecanViewModel::getLastSavedExperimentJson() const {
    return m_lastSavedExperimentJson;
}

void TecanViewModel::setLastSavedExperimentJson(const QJsonObject &json) {
    m_lastSavedExperimentJson = json;
}

ExperimentManager* TecanViewModel::getExperimentManager() const {
    return m_experimentManager.get();
}
