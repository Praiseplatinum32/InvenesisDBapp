#include "Plate1536Dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QLabel>
#include <QRadioButton>
#include <QScrollArea>

Plate1536Dialog::Plate1536Dialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("1536-Well Plate Map");
    resize(800, 600);  // initial size

    // --- Plate widget + scroll area ---
    plate1536 = new PlateWidget(32, 48, this);
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(plate1536);

    // --- Mode buttons ---
    QRadioButton* noneBtn   = new QRadioButton("None", this);
    QRadioButton* sampleBtn = new QRadioButton("Sample", this);
    QRadioButton* dmsoBtn   = new QRadioButton("DMSO", this);
    QRadioButton* stdBtn    = new QRadioButton("Standard", this);
    noneBtn->setChecked(true);

    modeGroup = new QButtonGroup(this);
    modeGroup->addButton(noneBtn,   PlateWidget::None);
    modeGroup->addButton(sampleBtn, PlateWidget::Sample);
    modeGroup->addButton(dmsoBtn,   PlateWidget::DMSO);
    modeGroup->addButton(stdBtn,    PlateWidget::Standard);
    connect(modeGroup,
            &QButtonGroup::idClicked,
            this,
            &Plate1536Dialog::onSelectionChanged);

    // --- Sample ID combo (1–1536) ---
    sampleCombo = new QComboBox(this);
    for (int i = 1; i <= 1536; ++i)
        sampleCombo->addItem(QString::number(i), i);
    connect(sampleCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &Plate1536Dialog::onSampleChanged);

    // --- Dilution spin (1–48) ---
    dilutionSpin = new QSpinBox(this);
    dilutionSpin->setRange(1, 48);
    dilutionSpin->setValue(1);
    connect(dilutionSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &Plate1536Dialog::onDilutionChanged);

    auto ctrlLayout = new QHBoxLayout;
    ctrlLayout->addWidget(noneBtn);
    ctrlLayout->addWidget(sampleBtn);
    ctrlLayout->addWidget(new QLabel("Sample ID:", this));
    ctrlLayout->addWidget(sampleCombo);
    ctrlLayout->addWidget(new QLabel("Dilution:", this));
    ctrlLayout->addWidget(dilutionSpin);
    ctrlLayout->addWidget(dmsoBtn);
    ctrlLayout->addWidget(stdBtn);

    // --- Action buttons ---
    exportBtn = new QPushButton("Export CSV", this);
    loadBtn   = new QPushButton("Load CSV", this);
    clearBtn  = new QPushButton("Clear", this);
    undoBtn   = new QPushButton("Undo", this);
    connect(exportBtn, &QPushButton::clicked, this, &Plate1536Dialog::export1536);
    connect(loadBtn,   &QPushButton::clicked, this, &Plate1536Dialog::load1536);
    connect(clearBtn,  &QPushButton::clicked, plate1536, &PlateWidget::clearLayout);
    connect(undoBtn,   &QPushButton::clicked, plate1536, &PlateWidget::undo);

    auto btnLayout = new QHBoxLayout;
    btnLayout->addWidget(exportBtn);
    btnLayout->addWidget(loadBtn);
    btnLayout->addWidget(clearBtn);
    btnLayout->addWidget(undoBtn);

    // --- Assemble main layout ---
    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(ctrlLayout);
    mainLayout->addWidget(scrollArea);
    mainLayout->addLayout(btnLayout);
    setLayout(mainLayout);
}

void Plate1536Dialog::onSelectionChanged(int id)
{
    PlateWidget::WellType type = static_cast<PlateWidget::WellType>(id);
    plate1536->setCurrentWellType(type);
}

void Plate1536Dialog::onSampleChanged(int index)
{
    int id = sampleCombo->itemData(index).toInt();
    plate1536->setCurrentSample(id);
}

void Plate1536Dialog::onDilutionChanged(int step)
{
    plate1536->setCurrentDilutionStep(step);
}

void Plate1536Dialog::writeCSV(const QString& defaultName,
                               PlateWidget* widget,
                               int totalWells)
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "Save CSV", defaultName, "CSV Files (*.csv>"
        );
    if (filePath.isEmpty()) return;

    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot write file");
        return;
    }

    QTextStream ts(&out);
    ts << QFileInfo(filePath).fileName()
       << "," << totalWells
       << ",user_layout\n";

    const auto layout = widget->layout();
    int cols = widget->cols();
    for (int i = 0; i < layout.size(); ++i) {
        const auto& wd = layout[i];
        if (wd.type == PlateWidget::None) continue;

        int row = i / cols;
        int col = i % cols;
        ts << QString("%1%2")
                  .arg(QChar('A' + (row / 26)))
                  .arg(QChar('A' + (row % 26)))
           << QString("%1").arg(col + 1, 2, 10, QChar('0'))
           << ",";

        switch (wd.type) {
        case PlateWidget::Sample:
            ts << "SAMPLE," << wd.sampleId << "," << wd.dilutionStep;
            break;
        case PlateWidget::DMSO:
            ts << "DMSO";
            break;
        case PlateWidget::Standard:
            ts << "STANDARD";
            break;
        default:
            break;
        }
        ts << "\n";
    }
}

void Plate1536Dialog::export1536() { writeCSV("layout_1536.csv", plate1536, 1536); }

void Plate1536Dialog::load1536()
{
    QString file = QFileDialog::getOpenFileName(
        this, "Open CSV", {}, "CSV Files (*.csv>"
        );
    if (file.isEmpty()) return;

    QFile in(file);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file");
        return;
    }

    QTextStream tsIn(&in);
    tsIn.readLine();

    int cols = plate1536->cols();
    int rows = plate1536->rows();
    QVector<PlateWidget::WellData> data(rows * cols);

    while (!tsIn.atEnd()) {
        QStringList parts = tsIn.readLine().split(',');
        if (parts.size() < 2) continue;

        QString rowStr = parts[0].left(2);
        int first  = rowStr[0].toLatin1() - 'A';
        int second = rowStr[1].toLatin1() - 'A';
        int r = first * 26 + second;
        int c = parts[0].mid(2).toInt() - 1;
        int idx = r * cols + c;
        if (idx < 0 || idx >= data.size()) continue;

        PlateWidget::WellData wd;
        QString t = parts[1].toUpper();
        if (t == "SAMPLE" && parts.size() >= 4) {
            wd.type         = PlateWidget::Sample;
            wd.sampleId     = parts[2].toInt();
            wd.dilutionStep = parts[3].toInt();
        } else if (t == "DMSO") {
            wd.type = PlateWidget::DMSO;
        } else if (t == "STANDARD") {
            wd.type = PlateWidget::Standard;
        }
        data[idx] = wd;
    }
    plate1536->loadLayout(data);
}

