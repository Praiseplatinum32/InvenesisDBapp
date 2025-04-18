#ifndef INVENESIS_MATRIXPLATECONTAINER_H
#define INVENESIS_MATRIXPLATECONTAINER_H

#include <QWidget>
#include <QMap>
#include <QSet>
#include "matrixplatewidget.h"

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QScrollArea;
QT_END_NAMESPACE

class MatrixPlateContainer : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MatrixPlateContainer)

public:
    explicit MatrixPlateContainer(QWidget *parent = nullptr);

    void populatePlates(const QMap<QString,QSet<QString>> &plateData);
    void clearPlates();

    [[nodiscard]] QMap<QString,QSet<QString>> getPlateMap() const;

private:
    QVBoxLayout           *mainLayout_     = nullptr;
    QScrollArea           *scrollArea_     = nullptr;
    QWidget               *platesHost_     = nullptr;
    QVBoxLayout           *platesLayout_   = nullptr;
    QMap<QString,MatrixPlateWidget*> plates_;
};

#endif // INVENESIS_MATRIXPLATECONTAINER_H
