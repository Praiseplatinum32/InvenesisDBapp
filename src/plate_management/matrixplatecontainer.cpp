#include "matrixplatecontainer.h"

// Qt
#include <QScrollArea>
#include <QVBoxLayout>

/* ====================================================================== */
/*                              ctor                                      */
/* ====================================================================== */
MatrixPlateContainer::MatrixPlateContainer(QWidget *parent)
    : QWidget(parent)
{
    mainLayout_   = new QVBoxLayout(this);
    scrollArea_   = new QScrollArea(this);
    platesHost_   = new QWidget(this);
    platesLayout_ = new QVBoxLayout(platesHost_);

    platesLayout_->setAlignment(Qt::AlignTop | Qt::AlignHCenter);  // centred
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setAlignment(Qt::AlignTop | Qt::AlignHCenter);    // centred
    scrollArea_->setWidget(platesHost_);

    mainLayout_->addWidget(scrollArea_);
}

/* ====================================================================== */
/*                      add / remove plates                               */
/* ====================================================================== */
void MatrixPlateContainer::populatePlates(const QMap<QString,QSet<QString>> &data)
{
    clearPlates();

    for (auto it = data.cbegin(); it != data.cend(); ++it)
    {
        auto *plate = new MatrixPlateWidget(it.key(), this);

        /* clean well IDs */
        QSet<QString> wells;
        for (const QString &w : it.value())
            wells.insert(w.trimmed().toUpper());

        plate->setOccupiedWells(wells);
        platesLayout_->addWidget(plate);
        plates_.insert(it.key(), plate);
    }
}

void MatrixPlateContainer::clearPlates()
{
    while (auto *item = platesLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    plates_.clear();
}

/* ====================================================================== */
/*                        gather data back                                */
/* ====================================================================== */
QMap<QString,QSet<QString>> MatrixPlateContainer::getPlateMap() const
{
    QMap<QString,QSet<QString>> out;
    for (auto it = plates_.cbegin(); it != plates_.cend(); ++it)
        out[it.key()] = it.value()->getOccupiedWells();
    return out;
}
