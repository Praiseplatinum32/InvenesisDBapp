#include "generategwldialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDoubleSpinBox>

GenerateGwlDialog::GenerateGwlDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Generate GWL");
    auto *layout = new QVBoxLayout(this);

    auto *lbl = new QLabel("Select instrument for GWL generation:", this);
    layout->addWidget(lbl);

    fluentCheck_ = new QCheckBox("Use Tecan Fluent 1080 (disposable tips)", this);
    fluentCheck_->setChecked(true); // default Fluent
    layout->addWidget(fluentCheck_);

    optimizeCheck_ = new QCheckBox("Optimize GWL commands (R/T)", this);
    optimizeCheck_->setChecked(false); // default off to allow comparison
    layout->addWidget(optimizeCheck_);
    connect(fluentCheck_, &QCheckBox::toggled, optimizeCheck_, &QWidget::setEnabled);
    optimizeCheck_->setEnabled(fluentCheck_->isChecked());

    auto *tipLayout = new QHBoxLayout();
    auto *tipLbl = new QLabel("Tip size for reagent/dilution:", this);
    tipSizeCombo_ = new QComboBox(this);
    tipSizeCombo_->addItems({"200ul", "350ul", "1000ul"});
    tipSizeCombo_->setCurrentText("350ul"); // Default
    tipLayout->addWidget(tipLbl);
    tipLayout->addWidget(tipSizeCombo_);
    tipLayout->addStretch(1);
    layout->addLayout(tipLayout);

    connect(fluentCheck_, &QCheckBox::toggled, tipSizeCombo_, &QWidget::setEnabled);
    tipSizeCombo_->setEnabled(fluentCheck_->isChecked());

    auto *volLayout = new QHBoxLayout();
    overrideMinVolCheck_ = new QCheckBox("Override minimum well volume (ul):", this);
    overrideMinVolCheck_->setChecked(false);
    minVolSpin_ = new QDoubleSpinBox(this);
    minVolSpin_->setDecimals(1);
    minVolSpin_->setRange(1.0, 1000.0);
    minVolSpin_->setValue(30.0);
    minVolSpin_->setEnabled(false);
    volLayout->addWidget(overrideMinVolCheck_);
    volLayout->addWidget(minVolSpin_);
    volLayout->addStretch(1);
    layout->addLayout(volLayout);

    connect(overrideMinVolCheck_, &QCheckBox::toggled, minVolSpin_, &QWidget::setEnabled);

    auto *btns = new QHBoxLayout();
    ok_ = new QPushButton("Generate", this);
    cancel_ = new QPushButton("Cancel", this);
    btns->addStretch(1);
    btns->addWidget(ok_);
    btns->addWidget(cancel_);
    layout->addLayout(btns);

    connect(ok_, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel_, &QPushButton::clicked, this, &QDialog::reject);
}

bool GenerateGwlDialog::useFluent() const
{
    return fluentCheck_ && fluentCheck_->isChecked();
}

bool GenerateGwlDialog::optimizeGwl() const
{
    return optimizeCheck_ && optimizeCheck_->isChecked();
}

QString GenerateGwlDialog::selectedTipSize() const
{
    if (tipSizeCombo_) {
        return tipSizeCombo_->currentText();
    }
    return "350ul";
}

double GenerateGwlDialog::overrideMinVolume() const
{
    if (overrideMinVolCheck_ && overrideMinVolCheck_->isChecked() && minVolSpin_) {
        return minVolSpin_->value();
    }
    return -1.0;
}
