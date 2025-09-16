#ifndef INVENESIS_MATRIXPLATEWIDGET_H
#define INVENESIS_MATRIXPLATEWIDGET_H
/**
 * @file matrixplatewidget.h
 * @brief Simple 96‑well “matrix” (source) plate with a barcode/title on top.
 *
 * Refactor April 2025 – same API, fixed‑size grid so the title always
 * stays exactly above the wells when the scroll‑area is resized.
 */

#include <QWidget>
#include <QMap>
#include <QSet>

QT_BEGIN_NAMESPACE
class QLabel;
class QGridLayout;
QT_END_NAMESPACE

class MatrixPlateWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MatrixPlateWidget)

public:
    explicit MatrixPlateWidget(const QString &containerId,
                               QWidget *parent = nullptr);

    void setOccupiedWells(const QSet<QString> &wells);

    [[nodiscard]] QString      getContainerId()   const { return containerId_; }
    [[nodiscard]] QSet<QString> getOccupiedWells() const { return occupied_; }

private:
    void setupPlateGrid();

private:                                    /* constants */
    static constexpr int  kCols       = 12;
    static constexpr int  kRows       = 8;
    static constexpr int  kWellPx     = 40;   // well width/height
    static constexpr int  kGapPx      = 1;    // spacing in the grid

private:                                    /* state */
    const QString          containerId_;
    QGridLayout           *grid_        = nullptr;
    QMap<QString,QLabel*>  wells_;          // "A01" → label*
    QSet<QString>          occupied_;
};

#endif // INVENESIS_MATRIXPLATEWIDGET_H
