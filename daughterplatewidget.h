#ifndef DAUGHTERPLATEWIDGET_H
#define DAUGHTERPLATEWIDGET_H

#include <QWidget>
#include <QMap>
#include <QStringList>
#include <QGridLayout>
#include <QLabel>
#include <QColor>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QJsonObject>

class DaughterPlateWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DaughterPlateWidget(int plateNumber, QWidget *parent = nullptr);

    void populatePlate(const QMap<QString, QStringList>& compoundWells,
                       const QMap<QString, QColor>& compoundColors,
                       int dilutionSteps);

    void clearCompounds();
    void enableCompoundDragDrop(int dilutionSteps);
    QJsonObject toJson() const;

    void fromJson(const QJsonObject& json, int dilutionSteps);

    void setStandardInfo(const QString& name, const QString& notes);



protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;

private:
    void setupEmptyPlate();
    void showDropPreview(const QString& compoundName, const QString& startWell);
    void clearDropPreview();



    int plateNumber;
    int dilutionSteps;
    QGridLayout* plateLayout;
    QMap<QString, QLabel*> wellLabels;
    QStringList previewWells;  // currently highlighted wells
    QString previewCompound;   // name of the compound being dragged

    QLabel* standardLabel = nullptr;  // 🔹 Display standard name

};

#endif // DAUGHTERPLATEWIDGET_H
