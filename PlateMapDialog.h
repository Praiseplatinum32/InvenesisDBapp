#pragma once

#include <QDialog>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include "PlateWidget.h"

class PlateMapDialog : public QDialog {
    Q_OBJECT
public:
    explicit PlateMapDialog(QWidget* parent = nullptr);

private slots:
    void export384();
    void export96();
    void load384();
    void load96();
    void onSelectionChanged(int id);
    void onSampleChanged(int index);
    void onDilutionChanged(double value);

private:
    PlateWidget* plate384;
    PlateWidget* plate96;
    QButtonGroup* modeGroup;
    QComboBox* sampleCombo;
    QDoubleSpinBox* dilutionSpin;
    QPushButton* export384Btn;
    QPushButton* load384Btn;
    QPushButton* clear384Btn;
    QPushButton* undo384Btn;
    QPushButton* export96Btn;
    QPushButton* load96Btn;
    QPushButton* clear96Btn;
    QPushButton* undo96Btn;

    void writeCSV(const QString& filename, PlateWidget* widget, int wells);
};
