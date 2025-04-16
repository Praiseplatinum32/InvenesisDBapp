#ifndef GWLGENERATOR_H
#define GWLGENERATOR_H

#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

class GWLGenerator
{
public:
    GWLGenerator();
    static QStringList generateTransferCommands(const QJsonObject& experimentJson);

private:
    static QStringList generateStandardCommandsINV031(const QString& barcode, const QString& baseWell, int plateNumber);
    static QMap<QString, QJsonObject> prepareCompoundLookup(const QJsonArray& compounds);
    static QMap<QString, QString> prepareTestLookup(const QJsonArray& testRequests);
    static QJsonArray loadVolumeMapJson();
    static QJsonObject getVolumeEntry(const QString& testId, double stockConc, const QJsonArray& volumeMap);
    static bool isInvT031Test(const QJsonArray& testRequests);
    static QStringList generateCompoundTransfer(const QString& compound, const QString& targetWell, int plateNumber,
                                                const QJsonObject& cmpData, const QJsonObject& volumeEntry);
};

#endif // GWLGENERATOR_H
