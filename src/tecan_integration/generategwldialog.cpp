#include "generategwldialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>

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
