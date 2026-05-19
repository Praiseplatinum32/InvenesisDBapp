#ifndef INVENESIS_DAUGHTERPLATEWIDGET_H
#define INVENESIS_DAUGHTERPLATEWIDGET_H
/**
 * @file  daughterplatewidget.h
 * @brief Interactive daughter-plate widget (96- or 384-well, drag-and-drop compounds).
 *
 * 2025-04 refactor – now supports 96 and 384 wells, same public behaviour.
 */

#include <QWidget>
#include <QMap>
#include <QStringList>
#include <QColor>
#include <QJsonObject>

QT_BEGIN_NAMESPACE
class QGridLayout;
class QLabel;
class QVBoxLayout;
class QDragEnterEvent;
class QDropEvent;
class QDragMoveEvent;
QT_END_NAMESPACE

class DaughterPlateWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DaughterPlateWidget)

public:
    explicit DaughterPlateWidget(int plateNumber, QWidget *parent = nullptr);

    /// Plate geometry / format.
    enum PlateFormat {
        Plate96,
        Plate384
    };

    using CompoundMap = QMap<QString, QStringList>;
    using ColorMap    = QMap<QString, QColor>;

    /// Populate wells with compounds + colours, based on the current plate format.
    void populatePlate(const CompoundMap &compoundWells,
                       const ColorMap    &compoundColors,
                       int                dilutionSteps);

    /// Clear all *compound* placements (keep Standard & DMSO).
    void clearCompounds();

    /// Enable drag/drop of compounds (stores dilutionSteps for preview logic).
    void enableCompoundDragDrop(int dilutionSteps);

    /// Set plate format (96 vs 384). This rebuilds the grid and clears labels.
    void setPlateFormat(PlateFormat fmt);

    [[nodiscard]] QJsonObject toJson() const;
    void fromJson(const QJsonObject &json, int dilutionSteps);

    /** Display a standard name + tooltip underneath the title. */
    void setStandardInfo(const QString &name, const QString &notes);

protected:                                /* Qt D-n-D overrides */
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:                                 /* helpers */
    void setupEmptyPlate();
    void showDropPreview(const QString &compoundName,
                         const QString &startWell);
    void clearDropPreview();

    /// Convenience: current row labels (A..H or A..P).
    const QStringList &rowLabels() const;
    /// Convenience: current column count (12 or 24).
    int columnCount() const;
    /// Gets well size dynamically based on plate format
    int getWellSize() const;

private:                                 /* constants */
    static constexpr int   kColumns96    = 12;
    static constexpr int   kColumns384   = 24;
    static const QStringList kRows96;
    static const QStringList kRows384;

private:                                 /* state */
    const int              plateNumber_;
    int                    dilutionSteps_ = 1;
    PlateFormat            format_        = Plate96;
    QGridLayout           *plateLayout_   = nullptr;
    QMap<QString,QLabel*>  wellLabels_;           // well-ID ➜ label*
    QStringList            previewWells_;         // wells highlighted during drag
    QString                previewCompound_;
    QLabel                *standardLabel_ = nullptr;
};

#endif // INVENESIS_DAUGHTERPLATEWIDGET_H
