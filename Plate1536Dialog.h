#ifndef PLATE1536DIALOG_H
#define PLATE1536DIALOG_H

#pragma once

#include <QDialog>
#include <QButtonGroup>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include "PlateWidget.h"

class Plate1536Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Plate1536Dialog(QWidget* parent = nullptr);

private slots:
    void onSelectionChanged(int id);
    void onSampleChanged(int index);
    void onDilutionChanged(int step);
    void export1536();
    void load1536();

private:
    void writeCSV(const QString& defaultName,
                  PlateWidget* widget,
                  int totalWells);

    PlateWidget*  plate1536;
    QButtonGroup* modeGroup;
    QComboBox*    sampleCombo;
    QSpinBox*     dilutionSpin;
    QPushButton*  exportBtn;
    QPushButton*  loadBtn;
    QPushButton*  clearBtn;
    QPushButton*  undoBtn;
};

#endif // PLATE1536DIALOG_H
