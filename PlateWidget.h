#ifndef PLATEWIDGET_H
#define PLATEWIDGET_H

#pragma once

#include <QWidget>
#include <QVector>
#include <QStack>
#include <QRubberBand>
#include <QMouseEvent>
#include <QPainter>
#include <QMessageBox>
#include <QColor>

class PlateWidget : public QWidget {
    Q_OBJECT
public:
    enum WellType { None = 0, Sample, DMSO, Standard };
    struct WellData {
        WellType type = None;
        int sampleId = 1;
        int dilutionStep = 1;
    };

    explicit PlateWidget(int rows, int cols, QWidget* parent = nullptr);

    void clearLayout();
    void loadLayout(const QVector<WellData>& layout);
    QVector<WellData> layout() const;

    void setCurrentWellType(WellType type);
    void setCurrentSample(int id);
    void setCurrentDilutionStep(int step);
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
    int m_currentDilutionStep;
    QStack<QVector<WellData>> m_undoStack;
    QRubberBand* m_rubberBand;
    QPoint m_dragStart;
    bool m_selecting;
    bool m_serialSelecting;

    QRect cellRect(int row, int col) const;
    void saveState();
    void applySelectionRect(const QRect& rect);
    void applySerialSelectionRect(const QRect& rect);
    void setWellAt(const QPoint& pos);
    QColor sampleColor(int sampleId, int dilutionStep) const;
};
#endif
