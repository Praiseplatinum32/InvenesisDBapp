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
    static bool parseA01(const QString& a01, int& row1, int& col1);  // from "A01" -> (1,1)
    static PlateWidget::WellType roleFromString(const QString& s);

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

    // Helpers for CSV formatting/parsing
    static QString toA01(int row1Based, int col1Based);                 // "A01".."P24" etc.

    static QString roleToString(PlateWidget::WellType t);               // Sample/Standard/DMSO/Void


private:
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

#endif // PLATEMAPDIALOG_H
