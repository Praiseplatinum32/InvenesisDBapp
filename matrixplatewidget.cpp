#include "matrixplatewidget.h"

// Qt
#include <QGridLayout>
#include <QLabel>
#include <QFrame>

/* ======================================================================= */
/*                                ctor                                     */
/* ======================================================================= */
MatrixPlateWidget::MatrixPlateWidget(const QString &containerId, QWidget *parent)
    : QWidget(parent),
    containerId_{containerId}
{
    setupPlateGrid();

    /* make the widget exactly as wide as the grid so the title centres
       above the wells *and* the whole plate can be centred in the scroll‑area */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    adjustSize();
}

/* ======================================================================= */
/*                          grid construction                              */
/* ======================================================================= */
void MatrixPlateWidget::setupPlateGrid()
{
    grid_ = new QGridLayout(this);
    grid_->setSpacing(kGapPx);
    grid_->setContentsMargins(1, 4, 1, 1);
    grid_->setSizeConstraint(QLayout::SetFixedSize);    // **key line**

    /* --- plate title / barcode ------------------------------------------ */
    auto *title = new QLabel(tr("Container: %1").arg(containerId_), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-weight:bold;"));
    grid_->addWidget(title, 0, 0, 1, kCols + 1);        // span all columns

    /* --- row / column headers ------------------------------------------- */
    const QStringList rowLetters = {"A","B","C","D","E","F","G","H"};
    for (int r = 0; r < kRows; ++r) {
        auto *rowLbl = new QLabel(rowLetters[r], this);
        rowLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLbl->setStyleSheet(QStringLiteral("padding-right:3px;"));
        grid_->addWidget(rowLbl, r + 2, 0);             // +2 → leave row 1 free
    }
    for (int c = 1; c <= kCols; ++c) {
        auto *colLbl = new QLabel(QString::number(c), this);
        colLbl->setAlignment(Qt::AlignCenter);
        grid_->addWidget(colLbl, 1, c);                 // column header row
    }

    /* --- wells ----------------------------------------------------------- */
    for (int r = 0; r < kRows; ++r)
        for (int c = 1; c <= kCols; ++c)
        {
            const QString wellId =
                rowLetters[r] + QString("%1").arg(c,2,10,QLatin1Char('0')); // A01
            auto *lbl = new QLabel(wellId, this);
            lbl->setFixedSize(kWellPx, kWellPx);
            lbl->setFrameStyle(QFrame::Panel | QFrame::Sunken);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet(QStringLiteral("background-color:white;"));
            grid_->addWidget(lbl, r + 2, c);
            wells_.insert(wellId, lbl);
        }
}

/* ======================================================================= */
/*                          public setters                                 */
/* ======================================================================= */
void MatrixPlateWidget::setOccupiedWells(const QSet<QString> &wells)
{
    occupied_ = wells;

    /* reset all wells */
    for (auto *lbl : std::as_const(wells_))
        lbl->setStyleSheet(QStringLiteral("background-color:white;"));

    const QString css = QStringLiteral("background-color:lightgreen; font-weight:bold;");
    for (const QString &idRaw : std::as_const(occupied_)) {
        const QString id = idRaw.trimmed().toUpper();
        if (wells_.contains(id))
            wells_[id]->setStyleSheet(css);
    }
}

