#pragma once

#include <QWidget>
#include <QVector>
#include <QStack>
#include <QRubberBand>
#include <QMouseEvent>
#include <QPainter>
#include <QMessageBox>

class PlateWidget : public QWidget {
    Q_OBJECT
public:
    enum WellType { None = 0, Sample, DMSO, Standard };
    struct WellData {
        WellType type = None;
        int sampleId = 1;
        double dilution = 1.0;
    };

    explicit PlateWidget(int rows, int cols, QWidget* parent = nullptr);

    void clearLayout();
    void loadLayout(const QVector<WellData>& layout);
    QVector<WellData> layout() const;

    void setCurrentWellType(WellType type);
    void setCurrentSample(int id);
    void setCurrentDilution(double dil);
    void undo();

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

signals:
    void layoutChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int m_rows;
    int m_cols;
    int m_cellSize;
    int m_labelMargin;
    QVector<WellData> m_layout;
    WellType m_currentType;
    int m_currentSample;
    double m_currentDilution;
    QStack<QVector<WellData>> m_undoStack;
    QRubberBand* m_rubberBand;
    QPoint m_dragStart;
    bool m_selecting;

    QRect cellRect(int row, int col) const;
    void saveState();
    void applySelectionRect(const QRect& rect);
    void setWellAt(const QPoint& pos);
};

