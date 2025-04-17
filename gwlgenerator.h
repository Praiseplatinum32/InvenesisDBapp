#ifndef GWLGENERATOR_H
#define GWLGENERATOR_H

#include <QString>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

class QWidget;

class GWLGenerator
{
public:
    GWLGenerator();

    QStringList generateTransferCommands(const QJsonObject& experimentJson);

    static bool saveToFile(const QStringList& lines, const QString& defaultName, QWidget* parent = nullptr);

private:
    QJsonArray loadVolumeMapJson();
    bool isInvT031Test(const QJsonArray& testRequests);

    QJsonObject getVolumeEntry(const QString& testId, double stockConc, const QJsonArray& volumeMap, QStringList* errors = nullptr);
    QMap<QString, QJsonObject> prepareCompoundLookup(const QJsonArray& compounds);
    QMap<QString, QString> prepareTestLookup(const QJsonArray& testRequests);

    QStringList generateStandardCommandsINV031(const QString& barcode, const QString& baseWell, int plateNumber);
    QStringList generateCompoundTransfer(const QString& compound, const QString& targetWell, int plateNumber,
                                         const QJsonObject& cmpData, const QJsonObject& volumeEntry);
};

#endif // GWLGENERATOR_H
