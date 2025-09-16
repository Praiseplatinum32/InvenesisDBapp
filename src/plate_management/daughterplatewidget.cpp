#include "daughterplatewidget.h"

// Qt
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QRandomGenerator>
#include <QJsonArray>
#include <QJsonValue>

namespace { constexpr int kSpacingPx = 1; }

/* static */ const QStringList DaughterPlateWidget::kRows =
    {"A","B","C","D","E","F","G","H"};

/* ======================================================================== */
/*                               constructor                                */
/* ======================================================================== */
DaughterPlateWidget::DaughterPlateWidget(int plateNumber, QWidget *parent)
    : QWidget(parent),
    plateNumber_{plateNumber}
{
    /* main vertical layout */
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4,4,4,4);
    mainLayout->setSpacing(4);

    auto *title = new QLabel(tr("Daughter Plate %1").arg(plateNumber_), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-weight:bold;"));
    mainLayout->addWidget(title);

    plateLayout_ = new QGridLayout;
    plateLayout_->setSpacing(kSpacingPx);
    plateLayout_->setSizeConstraint(QLayout::SetFixedSize);        // fixed grid
    mainLayout->addLayout(plateLayout_);

    setupEmptyPlate();

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);          // no stretch
    adjustSize();

    setAcceptDrops(false);
}

/* ======================================================================== */
/*                           plate initialisation                           */
/* ======================================================================== */
void DaughterPlateWidget::setupEmptyPlate()
{
    for (int r = 0; r < kRows.size(); ++r)
        for (int c = 1; c <= kColumns; ++c)
        {
            const QString wellId = kRows[r] + QString::number(c);
            auto *lbl = new QLabel(wellId, this);
            lbl->setFixedSize(kWellSizePx, kWellSizePx);
            lbl->setFrameStyle(QFrame::Box);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet(QStringLiteral(
                "background-color:black; border:1px solid black;"));

            plateLayout_->addWidget(lbl, r, c-1);
            wellLabels_.insert(wellId, lbl);
        }
}

/* ======================================================================== */
/*                          public‑facing helpers                           */
/* ======================================================================== */
void DaughterPlateWidget::populatePlate(const CompoundMap &compoundWells,
                                        const ColorMap    &compoundColors,
                                        int                dilutionSteps)
{
    dilutionSteps_ = dilutionSteps;

    QFont font; font.setPointSize(7);

    for (auto it = compoundWells.cbegin(); it != compoundWells.cend(); ++it)
    {
        const QString &compound = it.key();
        const QStringList &wells = it.value();
        const QColor base = compoundColors.value(compound, Qt::gray);

        for (int i = 0; i < wells.size(); ++i)
        {
            const QString &well = wells[i];
            if (!wellLabels_.contains(well)) continue;

            auto *lbl = wellLabels_[well];
            QString text = compound.length() > 10 && compound.contains('-')
                               ? QString(compound).replace('-', "-\n")
                               : compound;

            lbl->setText(text);
            lbl->setFont(font);
            lbl->setToolTip(compound);

            QColor shade;
            if (compound == "DMSO")
                shade = Qt::darkGray;
            else {
                const qreal fade = 1.0 - (static_cast<qreal>(i) / dilutionSteps_);
                shade = base.lighter(100 + static_cast<int>((1 - fade) * 30));
            }
            lbl->setStyleSheet(QStringLiteral(
                                   "background-color:%1; border:1px solid black;").arg(shade.name()));
            lbl->setProperty("compound", compound);
        }
    }
}

void DaughterPlateWidget::clearCompounds()
{
    QFont font; font.setPointSize(7);

    for (auto it = wellLabels_.begin(); it != wellLabels_.end(); ++it)
    {
        auto *lbl = it.value();
        const QString compound = lbl->property("compound").toString();

        if (compound != "Standard" && compound != "DMSO")
        {
            lbl->setText(it.key());
            lbl->setFont(font);
            lbl->setStyleSheet(QStringLiteral(
                "background-color:black; border:1px solid black;"));
            lbl->setProperty("compound", QString());
        }
    }
    setAcceptDrops(true);
}

void DaughterPlateWidget::enableCompoundDragDrop(int dilutionSteps)
{
    dilutionSteps_ = dilutionSteps;
    setAcceptDrops(true);
}

/* ======================================================================== */
/*                               drag/drop                                  */
/* ======================================================================== */
void DaughterPlateWidget::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasText())
        e->acceptProposedAction();
}

void DaughterPlateWidget::dragLeaveEvent(QDragLeaveEvent *e)
{
    Q_UNUSED(e)
    clearDropPreview();
}

void DaughterPlateWidget::dragMoveEvent(QDragMoveEvent *e)
{
    clearDropPreview();

    auto *lbl = qobject_cast<QLabel*>(childAt(e->position().toPoint()));
    if (!lbl) return;

    const QString startWell = lbl->text();
    if (startWell.isEmpty()) return;

    showDropPreview(e->mimeData()->text(), startWell);
    e->acceptProposedAction();
}

void DaughterPlateWidget::dropEvent(QDropEvent *e)
{
    const QString compound = e->mimeData()->text();
    auto *target = qobject_cast<QLabel*>(childAt(e->position().toPoint()));
    if (!target) return;

    const QString startWell = target->text();
    if (startWell.isEmpty()) return;

    const QChar rowLetter = startWell.at(0);
    const int   startCol  = startWell.mid(1).toInt();

    QStringList targetWells;
    for (int i = 0; i < dilutionSteps_; ++i) {
        const int col = startCol + i;
        if (col > kColumns) return;

        const QString well = rowLetter + QString::number(col);
        if (!wellLabels_[well]->property("compound").toString().isEmpty())
            return;
        targetWells << well;
    }

    const QColor base = QColor::fromHsv(
        QRandomGenerator::global()->bounded(360), 200, 220);

    QFont font; font.setPointSize(7);
    const QString dispText = compound.length() > 10 && compound.contains('-')
                                 ? QString(compound).replace('-', "-\n")
                                 : compound;

    for (int i = 0; i < targetWells.size(); ++i)
    {
        auto *lbl = wellLabels_[targetWells[i]];
        lbl->setText(dispText);
        lbl->setFont(font);

        const qreal fade = 1.0 - static_cast<qreal>(i) / dilutionSteps_;
        const QColor shade = base.lighter(100 + static_cast<int>((1 - fade) * 80));
        lbl->setStyleSheet(QStringLiteral(
                               "background-color:%1; border:1px solid black;").arg(shade.name()));
        lbl->setProperty("compound", compound);
    }
    e->acceptProposedAction();
}

/* ---------- preview helpers --------------------------------------------- */
void DaughterPlateWidget::clearDropPreview()
{
    for (const QString &well : std::as_const(previewWells_))
    {
        auto *lbl = wellLabels_[well];
        if (lbl->property("compound").toString().isEmpty())
            lbl->setStyleSheet(QStringLiteral(
                "background-color:black; border:1px solid black;"));
        /*  ← we NO LONGER reset lbl->setText(well) here, preventing
            accidental overwrite of compound names                      */
    }
    previewWells_.clear();
    previewCompound_.clear();
}

void DaughterPlateWidget::showDropPreview(const QString &cmpd,
                                          const QString &startWell)
{
    previewWells_.clear();
    previewCompound_ = cmpd;

    const int rowIdx   = kRows.indexOf(startWell.left(1));
    const int startCol = startWell.mid(1).toInt();
    if (rowIdx == -1) return;

    bool conflict = false;
    for (int i = 0; i < dilutionSteps_; ++i) {
        const int col = startCol + i;
        const QString well = kRows[rowIdx] + QString::number(col);

        if (col > kColumns ||
            !wellLabels_.contains(well) ||
            !wellLabels_[well]->property("compound").toString().isEmpty())
        {
            conflict = true;
            break;
        }
        previewWells_ << well;
    }

    const QString okCss  = "background-color:#d0f0ff; border:2px dashed blue;";
    const QString badCss = "background-color:#ffaaaa; border:2px dashed red;";
    for (const QString &well : std::as_const(previewWells_))
        wellLabels_[well]->setStyleSheet(conflict ? badCss : okCss);
}

/* ======================================================================== */
/*                       JSON serialisation helpers                         */
/* ======================================================================== */
QJsonObject DaughterPlateWidget::toJson() const
{
    QJsonObject json;
    for (auto it = wellLabels_.cbegin(); it != wellLabels_.cend(); ++it)
    {
        const QString compound = it.value()->property("compound").toString();
        if (!compound.isEmpty())
            json[it.key()] = compound;
    }
    return json;
}

void DaughterPlateWidget::fromJson(const QJsonObject &json, int dilutionSteps)
{
    dilutionSteps_ = dilutionSteps;

    CompoundMap cmpdWells;
    for (auto it = json.constBegin(); it != json.constEnd(); ++it)
        cmpdWells[it.value().toString()].append(it.key());

    ColorMap cmpdColors;
    int hue = 0, step = 360 / (cmpdWells.size() + 1);
    for (auto it = cmpdWells.cbegin(); it != cmpdWells.cend(); ++it)
    {
        const QString &cmpd = it.key();
        if (cmpd == "Standard")      cmpdColors[cmpd] = QColor(0,122,204);
        else if (cmpd == "DMSO")     cmpdColors[cmpd] = Qt::darkGray;
        else                         cmpdColors[cmpd] = QColor::fromHsv(hue,200,220);
        hue += step;
    }
    populatePlate(cmpdWells, cmpdColors, dilutionSteps_);
}

/* ======================================================================== */
/*                      standard info under the title                       */
/* ======================================================================== */
void DaughterPlateWidget::setStandardInfo(const QString &name,
                                          const QString &notes)
{
    if (!standardLabel_) {
        standardLabel_ = new QLabel(this);
        standardLabel_->setAlignment(Qt::AlignCenter);
        standardLabel_->setStyleSheet(
            QStringLiteral("font-style:italic; color:#444;"));
        if (auto *v = qobject_cast<QVBoxLayout*>(layout()))
            v->insertWidget(1, standardLabel_);
    }
    standardLabel_->setText(tr("Standard: %1").arg(name));
    standardLabel_->setToolTip(notes);
}
