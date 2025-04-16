#ifndef STANDARDLIBRARY_H
#define STANDARDLIBRARY_H

#include <QMap>
#include <QString>
#include <QJsonObject>

struct StandardInfo {
    QString name;
    QString containerBarcode;
    QString well;
    double concentration;
    QString unit;
    QString invenesisSolutionId;
};

class StandardLibrary
{
public:
    static QMap<QString, QList<StandardInfo>> loadFromJson(const QString& filePath);
};

#endif // STANDARDLIBRARY_H
