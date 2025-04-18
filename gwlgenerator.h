#ifndef GWLGENERATOR_H
#define GWLGENERATOR_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMap>

class QWidget;

class GWLGenerator
{
public:
    GWLGenerator();

    /* returns full GWL file as list of lines (warnings are prefixed with ;) */
    QStringList generateTransferCommands(const QJsonObject &experimentJson);

    static bool saveToFile(const QStringList &lines,
                           const QString &defaultName,
                           QWidget *parent = nullptr);

private:                                /* helpers */
    QJsonObject loadVolumeMapJson();     // now returns *object* not array
    bool isInvT031Test(const QJsonArray &testRequests) const;

    QJsonObject getVolumeEntry(const QString  &testId,
                               double          stockConc,
                               const QJsonObject &volMap,
                               QStringList    *errors = nullptr) const;

    QMap<QString,QJsonObject> prepareCompoundLookup(const QJsonArray &compounds) const;
    QMap<QString,QString>     prepareTestLookup(const QJsonArray &testReqs) const;

    QStringList generateStandardCommandsINV031(const QString &barcode,
                                               const QString &baseWell,
                                               int            plateNum) const;

    QStringList generateCompoundTransfer(const QString     &compound,
                                         const QString     &targetWell,
                                         int                plateNum,
                                         const QJsonObject &cmpData,
                                         const QJsonObject &volEntry);
};

#endif // GWLGENERATOR_H
