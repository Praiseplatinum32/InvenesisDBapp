#ifndef MATRIXPLATEWIDGET_H
#define MATRIXPLATEWIDGET_H

#include <QWidget>
#include <QMap>
#include <QLabel>
#include <QSet>

class MatrixPlateWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MatrixPlateWidget(const QString& containerId, QWidget *parent = nullptr);
    void setOccupiedWells(const QSet<QString>& wells);
    QString getContainerId() const { return containerId; }
    QSet<QString> getOccupiedWells() const { return occupiedWells; }

private:
    void setupPlateGrid();
    QString containerId;
    QMap<QString, QLabel*> wellsMap;
    QSet<QString> occupiedWells;  // ✅ Add this line
};

#endif // MATRIXPLATEWIDGET_H
