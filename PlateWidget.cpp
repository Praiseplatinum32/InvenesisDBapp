#include "PlateWidget.h"

PlateWidget::PlateWidget(int rows, int cols, QWidget* parent)
    : QWidget(parent)
    , m_rows(rows)
    , m_cols(cols)
    , m_cellSize(30)
    , m_labelMargin(20)
    , m_layout(rows * cols)
    , m_currentType(None)
    , m_currentSample(1)
    , m_currentDilutionStep(1)
    , m_rubberBand(new QRubberBand(QRubberBand::Rectangle, this))
    , m_selecting(false)
{
    setMinimumSize(m_labelMargin + m_cols * m_cellSize + 1,
                   m_labelMargin + m_rows * m_cellSize + 1);
    setMouseTracking(true);
}

void PlateWidget::saveState()
{
    m_undoStack.push(m_layout);
    if (m_undoStack.size() > 50)
        m_undoStack.remove(0);
}

void PlateWidget::undo()
{
    if (m_undoStack.isEmpty()) return;
    m_layout = m_undoStack.pop();
    update();
    emit layoutChanged();
}

void PlateWidget::clearLayout()
{
    saveState();
    for (auto& w : m_layout) w = WellData();
    update(); emit layoutChanged();
}

void PlateWidget::loadLayout(const QVector<WellData>& layout)
{
    if (layout.size() != m_rows * m_cols) return;
    saveState();
    m_layout = layout;
    update(); emit layoutChanged();
}

QVector<PlateWidget::WellData> PlateWidget::layout() const
{
    return m_layout;
}

void PlateWidget::setCurrentWellType(PlateWidget::WellType type)
{
    m_currentType = type;
}

void PlateWidget::setCurrentSample(int id)
{
    m_currentSample = qBound(1, id, m_rows * m_cols);
}

void PlateWidget::setCurrentDilutionStep(int step)
{
    m_currentDilutionStep = qBound(1, step, m_cols);
}

QRect PlateWidget::cellRect(int row, int col) const
{
    int x = m_labelMargin + col * m_cellSize;
    int y = m_labelMargin + row * m_cellSize;
    return QRect(x + 1, y + 1, m_cellSize - 2, m_cellSize - 2);
}

// Generate a unique hue per sampleId, and adjust brightness per dilution
QColor PlateWidget::sampleColor(int sampleId, int dilutionStep) const
{
    int hue = (sampleId * 137) % 360;
    int sat = 200;
    const int minVal = 55;
    const int maxVal = 255;
    int val = minVal + ((dilutionStep - 1) * (maxVal - minVal)) / (m_cols - 1);
    val = qBound(minVal, val, maxVal);
    return QColor::fromHsv(hue, sat, val);
}

void PlateWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // labels
    for (int c = 0; c < m_cols; ++c) {
        int x = m_labelMargin + c * m_cellSize + m_cellSize / 2;
        p.drawText(x - 5, m_labelMargin / 2 + 6, QString::number(c + 1));
    }
    for (int r = 0; r < m_rows; ++r) {
        int y = m_labelMargin + r * m_cellSize + m_cellSize / 2;
        p.drawText(5, y + 5, QString(QChar('A' + r)));
    }
    // wells
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            QRect rect = cellRect(r, c);
            const WellData& wd = m_layout[r * m_cols + c];
            QColor fill = Qt::white;
            switch (wd.type) {
            case Sample:
                fill = sampleColor(wd.sampleId, wd.dilutionStep);
                break;
            case DMSO:
                fill = QColor(152, 251, 152);
                break;
            case Standard:
                fill = Qt::red;
                break;
            default:
                break;
            }
            p.fillRect(rect, fill);
            p.setPen(Qt::black);
            p.drawRect(rect);
            if (wd.type == Sample) {
                p.drawText(rect, Qt::AlignCenter,
                           QString("S%1 %2").arg(wd.sampleId).arg(wd.dilutionStep));
            }
        }
    }
}

void PlateWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
        m_selecting = true;
        m_dragStart = event->pos();
        m_rubberBand->setGeometry(QRect(m_dragStart, QSize()));
        m_rubberBand->show();
    } else {
        saveState();
        setWellAt(event->pos());
    }
}

void PlateWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_selecting) {
        QRect r(m_dragStart, event->pos());
        m_rubberBand->setGeometry(r.normalized());
    } else if (event->buttons() & Qt::LeftButton) {
        setWellAt(event->pos());
    }
}

void PlateWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_selecting) {
        m_rubberBand->hide();
        saveState();
        applySelectionRect(m_rubberBand->geometry());
        m_selecting = false;
    } else {
        setWellAt(event->pos());
    }
}

void PlateWidget::applySelectionRect(const QRect& rect)
{
    bool warned = false;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            QRect cell = cellRect(r, c);
            int idx = r * m_cols + c;
            if (!rect.intersects(cell)) continue;
            if (m_layout[idx].type != None && m_currentType != None &&
                m_layout[idx].type != m_currentType) {
                if (!warned) {
                    QMessageBox::warning(this, "Overlap Detected",
                                         "Cannot overwrite non-empty wells!");
                    warned = true;
                }
                continue;
            }
            m_layout[idx] = WellData{m_currentType, m_currentSample, m_currentDilutionStep};
        }
    }
    update(); emit layoutChanged();
}

void PlateWidget::setWellAt(const QPoint& pos)
{
    int x = pos.x() - m_labelMargin;
    int y = pos.y() - m_labelMargin;
    if (x < 0 || y < 0) return;
    int col = x / m_cellSize;
    int row = y / m_cellSize;
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return;
    int idx = row * m_cols + col;
    if (m_layout[idx].type != None && m_currentType != None &&
        m_layout[idx].type != m_currentType) {
        QMessageBox::warning(this, "Overlap Detected",
                             "Cannot overwrite non-empty well!");
        return;
    }
    m_layout[idx] = WellData{m_currentType, m_currentSample, m_currentDilutionStep};
    update(); emit layoutChanged();
}
