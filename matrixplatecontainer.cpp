#include "matrixplatecontainer.h"

MatrixPlateContainer::MatrixPlateContainer(QWidget *parent)
    : QWidget(parent)
{
    mainLayout = new QVBoxLayout(this);
    scrollArea = new QScrollArea(this);
    containerWidget = new QWidget(this);
    platesLayout = new QVBoxLayout(containerWidget);
    platesLayout->setAlignment(Qt::AlignTop);

    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(containerWidget);

    mainLayout->addWidget(scrollArea);
}

void MatrixPlateContainer::populatePlates(const QMap<QString, QSet<QString>>& plateData)
{
    clearPlates();

    for (auto it = plateData.constBegin(); it != plateData.constEnd(); ++it) {
        const auto& containerId = it.key();
        auto plate = new MatrixPlateWidget(containerId, this);

        QSet<QString> cleanedWells;
        for (const QString& well : it.value()) {
            cleanedWells.insert(well.trimmed().toUpper());
        }


        plate->setOccupiedWells(cleanedWells);
        plate->update();
        platesLayout->addWidget(plate);
        platesMap.insert(containerId, plate);
    }


    containerWidget->updateGeometry();
    scrollArea->update();
}


void MatrixPlateContainer::clearPlates()
{
    QLayoutItem *child;
    while ((child = platesLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    platesMap.clear();
}


QMap<QString, QSet<QString>> MatrixPlateContainer::getPlateMap() const
{
    QMap<QString, QSet<QString>> map;

    for (int i = 0; i < platesLayout->count(); ++i) {
        auto plate = qobject_cast<MatrixPlateWidget*>(platesLayout->itemAt(i)->widget());
        if (plate) {
            QString containerId = plate->getContainerId();
            QSet<QString> occupied = plate->getOccupiedWells();  // implement getter in MatrixPlateWidget
            map[containerId] = occupied;
        }
    }

    return map;
}
