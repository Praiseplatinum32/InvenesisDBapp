#ifndef GENERATEGWLDIALOG_H
#define GENERATEGWLDIALOG_H
#pragma once
#include <QDialog>

class QCheckBox;
class QPushButton;
class QComboBox;
class QDoubleSpinBox;

class GenerateGwlDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GenerateGwlDialog(QWidget *parent = nullptr);
    bool useFluent() const;
    bool optimizeGwl() const;
    QString selectedTipSize() const;
    double overrideMinVolume() const;

private:
    QCheckBox *fluentCheck_{nullptr};
    QCheckBox *optimizeCheck_{nullptr};
    QComboBox *tipSizeCombo_{nullptr};
    QCheckBox *overrideMinVolCheck_{nullptr};
    QDoubleSpinBox *minVolSpin_{nullptr};
    QPushButton *ok_{nullptr};
    QPushButton *cancel_{nullptr};
};


#endif // GENERATEGWLDIALOG_H
