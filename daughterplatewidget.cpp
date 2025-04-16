#include "daughterplatewidget.h"
#include <QRandomGenerator>

DaughterPlateWidget::DaughterPlateWidget(int plateNumber, QWidget *parent)
    : QWidget(parent),
    plateNumber(plateNumber),
    dilutionSteps(1)
{
    auto mainLayout = new QVBoxLayout(this);

    // Plate title
    auto title = new QLabel(QString("Daughter Plate %1").arg(plateNumber), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(title);

    // Plate grid
    plateLayout = new QGridLayout();
    mainLayout->addLayout(plateLayout);

    setupEmptyPlate();

    setAcceptDrops(false);  // initially disabled
}

// Setup a clean 96-well plate grid
void DaughterPlateWidget::setupEmptyPlate()
{
    QStringList rows = {"A","B","C","D","E","F","G","H"};
    for (int i = 0; i < rows.size(); ++i) {
        for (int j = 1; j <= 12; ++j) {
            QString wellName = rows[i] + QString::number(j);
            auto label = new QLabel(wellName, this);
            label->setFixedSize(40,40);
            label->setFrameStyle(QFrame::Box);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet("background-color: black; border: 1px black;");
            plateLayout->addWidget(label, i, j - 1);
            wellLabels.insert(wellName, label);
        }
    }
}

// Populate the plate with compounds, colors, and dilution fading
void DaughterPlateWidget::populatePlate(const QMap<QString, QStringList>& compoundWells,
                                        const QMap<QString, QColor>& compoundColors,
                                        int dilutionSteps)
{
    this->dilutionSteps = dilutionSteps;

    QFont font;
    font.setPointSize(7); // Smaller font for readability

    for (auto it = compoundWells.constBegin(); it != compoundWells.constEnd(); ++it) {
        const QString& compound = it.key();
        const QStringList& wells = it.value();
        QColor baseColor = compoundColors[compound];

        for (int i = 0; i < wells.size(); ++i) {
            QString well = wells[i];
            if (!wellLabels.contains(well))
                continue;

            auto label = wellLabels[well];
            QString displayText = compound;
            if (compound.length() > 10 && compound.contains("-")) {
                displayText.replace("-", "-\n"); // Insert line break at the dash
            }
            label->setText(displayText);
            label->setFont(font);
            label->setToolTip(compound);

            QColor fadedColor;
            if (compound == "DMSO") {
                fadedColor = Qt::darkGray;
            } else {
                qreal fadeFactor = 1.0 - (static_cast<qreal>(i) / dilutionSteps);
                fadedColor = baseColor.lighter(100 + static_cast<int>((1 - fadeFactor) * 30));
            }

            label->setStyleSheet(QString("background-color: %1; border: 1px solid black;")
                                     .arg(fadedColor.name()));
            label->setProperty("compound", compound);
        }
    }
}


// Clear only the compounds, preserving standards and DMSO
void DaughterPlateWidget::clearCompounds()
{
    QFont font;
    font.setPointSize(7);
    for (auto it = wellLabels.begin(); it != wellLabels.end(); ++it) {
        QLabel* label = it.value();
        QString wellName = it.key();
        QString compound = label->property("compound").toString();

        if (compound != "Standard" && compound != "DMSO") {
            label->setFixedSize(40,40);
            label->setFrameStyle(QFrame::Box);
            label->setAlignment(Qt::AlignCenter);
            label->setFont(font);
            label->setText(wellName);
            label->setStyleSheet("background-color: black; border: 1px solid black;");
            label->setProperty("compound", "");
            wellLabels.insert(wellName, label);
        }
    }

    // ✅ Ensure drag-and-drop still works
    setAcceptDrops(true);
}


// Enable drag-and-drop support for adding compounds
void DaughterPlateWidget::enableCompoundDragDrop(int dilutionSteps)
{
    this->dilutionSteps = dilutionSteps;
    setAcceptDrops(true);
}

// Handle drag enter event to accept drops
void DaughterPlateWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasText())
        event->acceptProposedAction();
}

void DaughterPlateWidget::clearDropPreview()
{
    foreach(const QString& well, previewWells) {
        QLabel* label = wellLabels[well];
        QString currentCompound = label->property("compound").toString();
        if (currentCompound.isEmpty()) {
            label->setStyleSheet("background-color:  black; border: 1px black;");
            label->setText(well);  // Reset well label
        }
    }
    previewWells.clear();
    previewCompound.clear();
}


void DaughterPlateWidget::showDropPreview(const QString& compoundName, const QString& startWell)
{
    clearDropPreview();  // Always clear previous highlights

    previewWells.clear();
    previewCompound = compoundName;

    QStringList rows = {"A","B","C","D","E","F","G","H"};
    QString startRow = startWell.left(1);
    int startCol = startWell.mid(1).toInt();
    int rowIdx = rows.indexOf(startRow);
    if (rowIdx == -1)
        return;

    bool conflict = false;

    for (int i = 0; i < dilutionSteps; ++i) {
        if (rowIdx + i >= rows.size()) {
            conflict = true;
            break;
        }

        QString well = rows[rowIdx + i] + QString::number(startCol);

        if (!wellLabels.contains(well) || !wellLabels[well]->property("compound").toString().isEmpty()) {
            conflict = true;
            break;
        }

        previewWells << well;
    }

    foreach(const QString& well, previewWells) {
        QLabel* label = wellLabels[well];
        if (conflict)
            label->setStyleSheet("background-color: #ffaaaa; border: 2px dashed red;");
        else
            label->setStyleSheet("background-color: #d0f0ff; border: 2px dashed blue;");
    }
}



// Handle dropping compounds onto the plate
void DaughterPlateWidget::dropEvent(QDropEvent* event)
{
    QString compoundName = event->mimeData()->text();
    QString displayText = compoundName;
    if (compoundName.length() > 10 && compoundName.contains("-")) {
        displayText.replace("-", "-\n"); // Insert line break at the dash
    }

    QLabel* targetLabel = qobject_cast<QLabel*>(childAt(event->position().toPoint()));
    if (!targetLabel) return;

    QString startWell = targetLabel->text();
    if (startWell.isEmpty()) return;

    char startRow = startWell.at(0).toLatin1();
    int startCol = startWell.mid(1).toInt();

    QStringList targetWells;
    for (int i = 0; i < dilutionSteps; ++i) {
        int col = startCol + i;
        if (col > 12) return;

        QString nextWell = QString("%1%2").arg(startRow).arg(col);
        if (wellLabels[nextWell]->property("compound").toString().isEmpty())
            targetWells << nextWell;
        else
            return; // Collision detected
    }

    // Updated random generator (modern C++)
    QColor color = QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 220);

    QFont font;
    font.setPointSize(7);

    for (int i = 0; i < targetWells.size(); ++i) {
        QLabel* label = wellLabels[targetWells[i]];
        label->setText(displayText);
        label->setFont(font);

        qreal fadeFactor = 1.0 - (static_cast<qreal>(i) / dilutionSteps);
        QColor fadedColor = color.lighter(100 + static_cast<int>((1 - fadeFactor) * 80));

        label->setStyleSheet(QString("background-color: %1; border: 1px solid black;")
                                 .arg(fadedColor.name()));
        label->setProperty("compound", compoundName);
    }

    event->acceptProposedAction();
}

void DaughterPlateWidget::dragMoveEvent(QDragMoveEvent* event)
{
    for (auto it = wellLabels.begin(); it != wellLabels.end(); ++it) {
        QLabel* label = it.value();
        QString compound = label->property("compound").toString();
        if (compound.isEmpty()) {
            label->setStyleSheet("background-color: black; border: 1px black;");
        } else if (compound == "Standard") {
            label->setStyleSheet("background-color: #007acc; border: 1px solid black; color: white;");
        } else if (compound == "DMSO") {
            label->setStyleSheet("background-color: darkgray; border: 1px solid black;");
        }
    }

    QLabel* targetLabel = qobject_cast<QLabel*>(childAt(event->position().toPoint()));
    if (!targetLabel)
        return;

    QString startWell = targetLabel->text();
    if (startWell.isEmpty())
        return;

    QStringList rows = {"A","B","C","D","E","F","G","H"};
    QString rowLetter = startWell.left(1);
    int rowIdx = rows.indexOf(rowLetter);
    int col = startWell.mid(1).toInt();

    if (rowIdx == -1 || col < 1 || col > 12)
        return;

    bool isOverflow = (col + dilutionSteps - 1 > 12);

    for (int i = 0; i < dilutionSteps; ++i) {
        int currentCol = col + i;
        QString previewWell = rowLetter + QString::number(currentCol);

        if (!wellLabels.contains(previewWell))
            continue;

        QLabel* label = wellLabels[previewWell];
        QString compoundInWell = label->property("compound").toString();

        if (compoundInWell.isEmpty()) {
            if (isOverflow)
                label->setStyleSheet("background-color: #ffcccc; border: 2px dashed red;");  // ❌ overflow
            else
                label->setStyleSheet("background-color: #ccccff; border: 1px dashed #444;");  // ✅ preview
        }
    }

    event->acceptProposedAction();
}



void DaughterPlateWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event);
    clearDropPreview();
}

QJsonObject DaughterPlateWidget::toJson() const
{
    QJsonObject json;
    for (auto it = wellLabels.constBegin(); it != wellLabels.constEnd(); ++it) {
        const QString& well = it.key();
        const QLabel* label = it.value();
        QString compound = label->property("compound").toString();
        if (!compound.isEmpty()) {
            json[well] = compound;
        }
    }
    return json;
}


void DaughterPlateWidget::fromJson(const QJsonObject& json, int dilutionSteps)
{
    this->dilutionSteps = dilutionSteps;

    QMap<QString, QStringList> compoundWells;

    // ✅ Replace foreach with iterator to avoid temporary container
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        const QString& well = it.key();
        const QString& compound = it.value().toString();
        compoundWells[compound].append(well);
    }

    // ✅ Avoid calling keys() inside foreach
    QMap<QString, QColor> compoundColors;
    int hue = 0;
    int hueStep = 360 / (compoundWells.size() + 1);

    for (auto it = compoundWells.constBegin(); it != compoundWells.constEnd(); ++it) {
        const QString& compound = it.key();

        if (compound == "Standard")
            compoundColors[compound] = QColor(0, 122, 204);
        else if (compound == "DMSO")
            compoundColors[compound] = Qt::darkGray;
        else {
            compoundColors[compound] = QColor::fromHsv(hue % 360, 200, 220);
            hue += hueStep;
        }
    }

    populatePlate(compoundWells, compoundColors, dilutionSteps);
}


void DaughterPlateWidget::setStandardInfo(const QString& name, const QString& notes)
{
    if (!standardLabel) {
        standardLabel = new QLabel(this);
        standardLabel->setAlignment(Qt::AlignCenter);
        standardLabel->setStyleSheet("font-style: italic; color: #444;");
        if (auto vLayout = qobject_cast<QVBoxLayout*>(layout())) {
            vLayout->insertWidget(1, standardLabel);
        }
    }

    standardLabel->setText("Standard: " + name);
    standardLabel->setToolTip(notes);
}

