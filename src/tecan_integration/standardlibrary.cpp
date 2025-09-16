#include "standardlibrary.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

QMap<QString, QList<StandardInfo>> StandardLibrary::loadFromJson(const QString& filePath)
{
    QMap<QString, QList<StandardInfo>> standards;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open standards file:" << filePath;
        return standards;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        qWarning() << "Invalid standards file format.";
        return standards;
    }

    QJsonArray array = doc.array();
    for (const QJsonValue& value : array) {
        QJsonObject obj = value.toObject();
        StandardInfo info;
        info.name = obj["Samplealias"].toString();
        info.containerBarcode = obj["Containerbarcode"].toString();
        info.well = obj["Containerposition"].toString();
        info.concentration = obj["Concentration"].toDouble();
        info.unit = obj["ConcentrationUnit"].toString();
        info.invenesisSolutionId = obj["invenesis_solution_ID"].toString();

        standards[info.name].append(info);
    }

    return standards;
}
