#ifndef INVENESIS_DAUGHTERPLATEWIDGET_H
#define INVENESIS_DAUGHTERPLATEWIDGET_H
/**
 * @file  daughterplatewidget.h
 * @brief Interactive 96‑well daughter‑plate widget (drag‑and‑drop compounds).
 *
 * 2025‑04 refactor – same public API, improved style & fixed‑spacing layout.
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

    using CompoundMap = QMap<QString, QStringList>;
    using ColorMap    = QMap<QString, QColor>;

    void populatePlate(const CompoundMap &compoundWells,
                       const ColorMap    &compoundColors,
                       int                dilutionSteps);

    void clearCompounds();
    void enableCompoundDragDrop(int dilutionSteps);

    [[nodiscard]] QJsonObject toJson() const;
    void fromJson(const QJsonObject &json, int dilutionSteps);

    /** Display a standard name + tooltip underneath the title. */
    void setStandardInfo(const QString &name, const QString &notes);

protected:                                /* Qt D‑n‑D overrides */
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:                                 /* helpers */
    void setupEmptyPlate();
    void showDropPreview(const QString &compoundName,
                         const QString &startWell);
    void clearDropPreview();

private:                                 /* constants */
    static constexpr int   kColumns     = 12;
    static constexpr int   kWellSizePx  = 40;
    static const QStringList kRows;

private:                                 /* state */
    const int              plateNumber_;
    int                    dilutionSteps_ = 1;
    QGridLayout           *plateLayout_   = nullptr;
    QMap<QString,QLabel*>  wellLabels_;           // well‑ID ➜ label*
    QStringList            previewWells_;         // wells highlighted during drag
    QString                previewCompound_;
    QLabel                *standardLabel_ = nullptr;
};

#endif // INVENESIS_DAUGHTERPLATEWIDGET_H
