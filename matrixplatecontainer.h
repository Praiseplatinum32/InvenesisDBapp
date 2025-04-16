#ifndef MATRIXPLATECONTAINER_H
#define MATRIXPLATECONTAINER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMap>
#include <QSet>
#include "matrixplatewidget.h"

class MatrixPlateContainer : public QWidget
{
    Q_OBJECT

public:
    explicit MatrixPlateContainer(QWidget *parent = nullptr);
    void populatePlates(const QMap<QString, QSet<QString>>& plateData);
    void clearPlates();
    QMap<QString, QSet<QString>> getPlateMap() const;

private:
    QVBoxLayout* mainLayout;
    QScrollArea* scrollArea;
    QWidget* containerWidget;
    QVBoxLayout* platesLayout;

    QMap<QString, MatrixPlateWidget*> platesMap;  // Keep track explicitly
};

#endif // MATRIXPLATECONTAINER_H
