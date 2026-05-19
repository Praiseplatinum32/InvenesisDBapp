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

/* static */ const QStringList DaughterPlateWidget::kRows96 =
    {"A","B","C","D","E","F","G","H"};
/* static */ const QStringList DaughterPlateWidget::kRows384 =
    {"A","B","C","D","E","F","G","H",
     "I","J","K","L","M","N","O","P"};

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

    // Default format: 96-well
    format_ = Plate96;
    setupEmptyPlate();

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);          // no stretch
    adjustSize();

    setAcceptDrops(false);
}

/* ======================================================================== */
/*                           plate initialisation                           */
/* ======================================================================== */

const QStringList &DaughterPlateWidget::rowLabels() const
{
    return (format_ == Plate384) ? kRows384 : kRows96;
}

int DaughterPlateWidget::columnCount() const
{
    return (format_ == Plate384) ? kColumns384 : kColumns96;
}

int DaughterPlateWidget::getWellSize() const
{
    return (format_ == Plate384) ? 20 : 40;
}

void DaughterPlateWidget::setupEmptyPlate()
{
    // Clear any previous content
    clearDropPreview();
    qDeleteAll(wellLabels_);
    wellLabels_.clear();

    if (!plateLayout_)
        return;

    while (QLayoutItem *item = plateLayout_->takeAt(0)) {
        if (auto *w = item->widget())
            w->deleteLater();
        delete item;
    }

    const QStringList &rows = rowLabels();
    const int cols          = columnCount();

    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 1; c <= cols; ++c) {
            const QString wellId = rows[r] + QString::number(c).rightJustified(2, '0');
            auto *lbl = new QLabel(wellId, this);
            const int sz = getWellSize();
            lbl->setFixedSize(sz, sz);
            lbl->setFrameStyle(QFrame::Box);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet(QStringLiteral(
                "background-color:black; border:1px solid black;"));

            plateLayout_->addWidget(lbl, r, c-1);
            wellLabels_.insert(wellId, lbl);
        }
    }
}

/* ======================================================================== */
/*                        format / geometry control                         */
/* ======================================================================== */

void DaughterPlateWidget::setPlateFormat(PlateFormat fmt)
{
    if (fmt == format_)
        return;

    format_ = fmt;
    setupEmptyPlate();
    adjustSize();
}

/* ======================================================================== */
/*                          public-facing helpers                           */
/* ======================================================================== */
void DaughterPlateWidget::populatePlate(const CompoundMap &compoundWells,
                                        const ColorMap    &compoundColors,
                                        int                dilutionSteps)
{
    dilutionSteps_ = dilutionSteps;

    QFont font; font.setPointSize(format_ == Plate384 ? 5 : 7);

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
                const qreal fade = 1.0 - (static_cast<qreal>(i) / qMax(1, dilutionSteps_));
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
    QFont font; font.setPointSize(format_ == Plate384 ? 5 : 7);

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
    const int   maxCols   = columnCount();

    QStringList targetWells;
    for (int i = 0; i < dilutionSteps_; ++i) {
        const int col = startCol + i;
        if (col > maxCols) return;

        const QString well = rowLetter + QString::number(col).rightJustified(2, '0');
        if (!wellLabels_.contains(well)) return;
        if (!wellLabels_[well]->property("compound").toString().isEmpty())
            return;
        targetWells << well;
    }

    const QColor base = QColor::fromHsv(
        QRandomGenerator::global()->bounded(360), 200, 220);

    QFont font; font.setPointSize(format_ == Plate384 ? 5 : 7);
    const QString dispText = compound.length() > 10 && compound.contains('-')
                                 ? QString(compound).replace('-', "-\n")
                                 : compound;

    for (int i = 0; i < targetWells.size(); ++i)
    {
        auto *lbl = wellLabels_[targetWells[i]];
        lbl->setText(dispText);
        lbl->setFont(font);

        const qreal fade = 1.0 - static_cast<qreal>(i) / qMax(1, dilutionSteps_);
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
        auto *lbl = wellLabels_.value(well, nullptr);
        if (!lbl) continue;

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

    const QStringList &rows = rowLabels();
    const int cols          = columnCount();

    const QString rowStr = startWell.left(1);
    const int rowIdx     = rows.indexOf(rowStr);
    const int startCol   = startWell.mid(1).toInt();
    if (rowIdx == -1) return;

    bool conflict = false;
    for (int i = 0; i < dilutionSteps_; ++i) {
        const int col = startCol + i;
        const QString well = rows[rowIdx] + QString::number(col).rightJustified(2, '0');

        if (col > cols ||
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
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        QString well = it.key();
        if (well.size() >= 2) {
            well = well.at(0).toUpper() + QString::number(well.mid(1).toInt()).rightJustified(2, '0');
        }
        cmpdWells[it.value().toString()].append(well);
    }

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
