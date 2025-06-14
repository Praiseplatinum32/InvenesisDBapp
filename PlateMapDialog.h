#ifndef PLATEMAPDIALOG_H
#define PLATEMAPDIALOG_H

#pragma once

#include <QDialog>
#include <QButtonGroup>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include "PlateWidget.h"

class PlateMapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlateMapDialog(QWidget* parent = nullptr);

private slots:
    void onSelectionChanged(int id);
    void onSampleChanged(int index);
    void onDilutionChanged(int step);

    void export384();
    void load384();
    void export96();
    void load96();
    void open1536Dialog();

private:
    void writeCSV(const QString& defaultName,
                  PlateWidget* widget,
                  int totalWells);

    PlateWidget* plate384;
    PlateWidget* plate96;

    QButtonGroup* modeGroup;
    QComboBox*    sampleCombo;
    QSpinBox*     dilutionSpin;

    QPushButton* export384Btn;
    QPushButton* load384Btn;
    QPushButton* clear384Btn;
    QPushButton* undo384Btn;

    QPushButton* export96Btn;
    QPushButton* load96Btn;
    QPushButton* clear96Btn;
    QPushButton* undo96Btn;

    QPushButton* open1536Btn;
};



#endif
