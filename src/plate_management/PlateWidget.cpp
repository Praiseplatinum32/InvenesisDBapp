#include "PlateWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMessageBox>
#include <QRubberBand>
#include <QDebug>

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
    , m_serialSelecting(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_Hover);
    setMinimumSize(
        m_labelMargin + m_cols * m_cellSize + 1,
        m_labelMargin + m_rows * m_cellSize + 1
        );
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
    if (m_undoStack.isEmpty())
        return;
    m_layout = m_undoStack.pop();
    update();
    emit layoutChanged();
}

void PlateWidget::clearLayout()
{
    saveState();
    for (auto &wd : m_layout)
        wd = WellData(); // default: None, id=0, dil=0
    update();
    emit layoutChanged();
}

void PlateWidget::loadLayout(const QVector<WellData> &layout)
{
    if (layout.size() != m_rows * m_cols)
        return;
    saveState();
    m_layout = layout;
    update();
    emit layoutChanged();
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

QColor PlateWidget::sampleColor(int sampleId, int dilutionStep) const
{
    // Unique-ish hue per sample ID
    int hue = (sampleId * 137) % 360;
    int sat = 200;
    const int dark = 55;
    const int bright = 255;
    int val = dark + ((dilutionStep - 1) * (bright - dark)) / qMax(1, (m_cols - 1));
    val = qBound(dark, val, bright);
    return QColor::fromHsv(hue, sat, val);
}

void PlateWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw row labels (single-letter for ≤26 rows, else two-letter)
    for (int r = 0; r < m_rows; ++r) {
        QString rowLabel;
        if (m_rows <= 26) {
            rowLabel = QChar('A' + r);
        } else {
            int first  = r / 26;
            int second = r % 26;
            rowLabel = QString("%1%2")
                           .arg(QChar('A' + first))
                           .arg(QChar('A' + second));
        }
        QRect lr(0,
                 m_labelMargin + r * m_cellSize,
                 m_labelMargin,
                 m_cellSize);
        p.drawText(lr, Qt::AlignCenter, rowLabel);
    }

    // Draw column labels (zero-padded if >9)
    for (int c = 0; c < m_cols; ++c) {
        QString colLabel = (m_cols > 9)
        ? QString("%1").arg(c + 1, 2, 10, QChar('0'))
        : QString::number(c + 1);

        QRect lc(m_labelMargin + c * m_cellSize,
                 0,
                 m_cellSize,
                 m_labelMargin);
        p.drawText(lc, Qt::AlignCenter, colLabel);
    }

    // Draw wells
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            int idx = r * m_cols + c;
            QRect rect = cellRect(r, c);
            const WellData &wd = m_layout[idx];

            QColor fill = Qt::white;
            switch (wd.type) {
            case Sample:
                fill = sampleColor(wd.sampleId, wd.dilutionStep);
                break;
            case DMSO:
                fill = QColor(152, 251, 152); // pale green
                break;
            case Standard:
                fill = Qt::red;
                break;
            case None:
            default:
                break;
            }
            p.fillRect(rect, fill);
            p.setPen(Qt::black);
            p.drawRect(rect);

            // Text for Sample and Standard (now both render ID & dilution)
            if (wd.type == Sample || wd.type == Standard) {
                QFont f = p.font();
                f.setPointSize(qMax(6, m_cellSize / 5));
                p.setFont(f);
                const QString prefix = (wd.type == Sample) ? "S" : "Std";
                p.drawText(rect, Qt::AlignCenter,
                           QString("%1%2\n%3").arg(prefix).arg(wd.sampleId).arg(wd.dilutionStep));
            }
            // DMSO/None: no text (id/dil are 0,0 by definition)
        }
    }
}

void PlateWidget::mousePressEvent(QMouseEvent* event)
{
    Qt::KeyboardModifiers mods = event->modifiers();
    bool shift = mods.testFlag(Qt::ShiftModifier);
    bool ctrl  = mods.testFlag(Qt::ControlModifier);

    // 1) Serial-dilution rectangle:
    //    Keep original behavior (Ctrl+Shift in Sample mode).
    //    If you want this for Standard too, change the condition below to:
    //    (m_currentType == Sample || m_currentType == Standard)
    if (shift && ctrl && (m_currentType == Sample || m_currentType == Standard)) {
        saveState();
        m_serialSelecting = true;
        m_selecting = false;
        m_dragStart = event->pos();
        m_rubberBand->setGeometry(QRect(m_dragStart, QSize()));
        m_rubberBand->show();
    }
    // 2) Normal rectangle selection: Shift in any non-None mode
    else if (shift && m_currentType != None) {
        saveState();
        m_selecting = true;
        m_serialSelecting = false;
        m_dragStart = event->pos();
        m_rubberBand->setGeometry(QRect(m_dragStart, QSize()));
        m_rubberBand->show();
    }
    // 3) Single-well click (all modes)
    else {
        saveState();
        setWellAt(event->pos());
    }
}

void PlateWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_selecting || m_serialSelecting) {
        QRect r(m_dragStart, event->pos());
        m_rubberBand->setGeometry(r.normalized());
    }
    else if (event->buttons() & Qt::LeftButton) {
        setWellAt(event->pos());
    }
}

void PlateWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_serialSelecting) {
        m_rubberBand->hide();
        applySerialSelectionRect(m_rubberBand->geometry());
        m_serialSelecting = false;
    }
    else if (m_selecting) {
        m_rubberBand->hide();
        applySelectionRect(m_rubberBand->geometry());
        m_selecting = false;
    }
    else {
        setWellAt(event->pos());
    }
}

void PlateWidget::applySelectionRect(const QRect& rect)
{
    bool warned = false;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            int idx = r * m_cols + c;
            QRect cell = cellRect(r, c);
            if (!rect.intersects(cell))
                continue;
            if (m_layout[idx].type != None
                && m_currentType != None
                && m_layout[idx].type != m_currentType)
            {
                if (!warned) {
                    QMessageBox::warning(
                        this,
                        "Overlap",
                        "Cannot overwrite non-empty wells!"
                        );
                    warned = true;
                }
                continue;
            }

            // Normalize data: DMSO and None must be (0,0). Sample/Standard keep selected id/dil.
            int id  = (m_currentType == DMSO || m_currentType == None) ? 0 : m_currentSample;
            int dil = (m_currentType == DMSO || m_currentType == None) ? 0 : m_currentDilutionStep;

            m_layout[idx] = WellData{ m_currentType, id, dil };
        }
    }
    update();
    emit layoutChanged();
}

void PlateWidget::applySerialSelectionRect(const QRect& rect)
{
    int r0 = (rect.top() - m_labelMargin) / m_cellSize;
    int c0 = (rect.left() - m_labelMargin) / m_cellSize;
    int r1 = (rect.bottom() - m_labelMargin) / m_cellSize;
    int c1 = (rect.right() - m_labelMargin) / m_cellSize;
    r0 = qBound(0, r0, m_rows - 1);
    r1 = qBound(0, r1, m_rows - 1);
    c0 = qBound(0, c0, m_cols - 1);
    c1 = qBound(0, c1, m_cols - 1);
    if (r1 < r0) std::swap(r0, r1);
    if (c1 < c0) std::swap(c0, c1);

    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            int idx = r * m_cols + c;

            // For serial selection we follow existing logic:
            //  - Sample: id increments down rows, dil increments across columns.
            //  - Others: keep normalized rules (DMSO/None -> 0,0; Standard behaves like Sample).
            int sample = m_currentSample + (r - r0);
            int dil    = m_currentDilutionStep + (c - c0);
            sample = qBound(1, sample, m_rows * m_cols);
            dil    = qBound(1, dil, m_cols);

            WellType t = m_currentType;

            if (t == DMSO || t == None) {
                sample = 0;
                dil    = 0;
            }
            // If you prefer Standard NOT to increment ID across rows, uncomment next line:
            // if (t == Standard) sample = m_currentSample;

            m_layout[idx] = WellData{ t, sample, dil };
        }
    }
    update();
    emit layoutChanged();
}

void PlateWidget::setWellAt(const QPoint& pos)
{
    int x = pos.x() - m_labelMargin;
    int y = pos.y() - m_labelMargin;
    if (x < 0 || y < 0)
        return;
    int c = x / m_cellSize;
    int r = y / m_cellSize;
    if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
        return;

    int idx = r * m_cols + c;
    if (m_layout[idx].type != None
        && m_currentType != None
        && m_layout[idx].type != m_currentType)
    {
        QMessageBox::warning(
            this,
            "Overlap",
            "Cannot overwrite non-empty well!"
            );
        return;
    }

    // Normalize when writing:
    int id  = (m_currentType == DMSO || m_currentType == None) ? 0 : m_currentSample;
    int dil = (m_currentType == DMSO || m_currentType == None) ? 0 : m_currentDilutionStep;

    m_layout[idx] = WellData{ m_currentType, id, dil };
    update();
    emit layoutChanged();
}



