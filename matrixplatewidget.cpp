#include "matrixplatewidget.h"
#include <QGridLayout>
#include <QLabel>
#include <QFrame>

MatrixPlateWidget::MatrixPlateWidget(const QString& containerId, QWidget *parent)
    : QWidget(parent), containerId(containerId)
{
    setupPlateGrid();
}

void MatrixPlateWidget::setupPlateGrid()
{
    auto layout = new QGridLayout(this);
    layout->setSpacing(1);
    layout->setContentsMargins(1, 20, 1, 1); // Increased top margin for better visibility of containerId label

    // Container ID at the top
    auto title = new QLabel(QString("Container: %1").arg(containerId), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-weight:bold; padding-bottom:5px;");
    layout->addWidget(title, 0, 0, 1, 13);

    // Row labels (A-H)
    QStringList rows = {"A","B","C","D","E","F","G","H"};
    for (int i = 0; i < 8; ++i) {
        auto rowLabel = new QLabel(rows[i], this);
        rowLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLabel->setStyleSheet("padding-right:5px;");
        layout->addWidget(rowLabel, i + 2, 0);
    }

    // Column labels (1-12)
    for (int j = 1; j <= 12; ++j) {
        auto colLabel = new QLabel(QString::number(j), this);
        colLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(colLabel, 1, j);
    }

    // Initialize wells explicitly into a map
    for (int i = 0; i < 8; ++i) {
        for (int j = 1; j <= 12; ++j) {
            QString wellName = rows[i] + QString("%1").arg(j, 2, 10, QLatin1Char('0')); // A01 format
            auto well = new QLabel(wellName, this);
            well->setFixedSize(40, 40);
            well->setFrameStyle(QFrame::Panel | QFrame::Sunken);
            well->setAlignment(Qt::AlignCenter);
            well->setStyleSheet("background-color: white;");
            layout->addWidget(well, i + 2, j);
            wellsMap[wellName] = well; // Store in map explicitly
        }
    }
}

void MatrixPlateWidget::setOccupiedWells(const QSet<QString>& wells)
{
    occupiedWells = wells;  // ✅ Store for retrieval

    // Reset all wells first to avoid residual highlights
    for (auto label : std::as_const(wellsMap)) {
        label->setStyleSheet("background-color: white;");
    }

    // Highlight only specified occupied wells
    for(const QString &well : wells) {
        QString formattedWell = well.trimmed().toUpper();
        if(wellsMap.contains(formattedWell)) {
            wellsMap[formattedWell]->setStyleSheet("background-color: lightgreen; font-weight:bold;");
        }
    }
}

