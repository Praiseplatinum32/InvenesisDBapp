#ifndef INVENESIS_TECANVIEWMODEL_H
#define INVENESIS_TECANVIEWMODEL_H

#include <QObject>
#include <QJsonObject>
#include <memory>
#include "services/ExperimentManager.h"
#include "services/ExperimentJsonSerializer.h"

class TecanViewModel : public QObject {
    Q_OBJECT

public:
    explicit TecanViewModel(QObject *parent = nullptr);
    ~TecanViewModel() override;

    ExperimentJsonSerializer::DaughterPlateType getDaughterPlateType() const;
    void setDaughterPlateType(ExperimentJsonSerializer::DaughterPlateType type);

    QJsonObject getQcPlatesJson() const;
    void setQcPlatesJson(const QJsonObject &json);

    QJsonObject getLastSavedExperimentJson() const;
    void setLastSavedExperimentJson(const QJsonObject &json);

    ExperimentManager* getExperimentManager() const;

signals:
    void plateTypeChanged();
    void qcPlatesChanged();

private:
    std::unique_ptr<ExperimentManager> m_experimentManager;
    ExperimentJsonSerializer::DaughterPlateType m_daughterPlateType;
    QJsonObject m_qcPlatesJson;
    QJsonObject m_lastSavedExperimentJson;
};

#endif // INVENESIS_TECANVIEWMODEL_H
