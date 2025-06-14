#include "PlateMapDialog.h"
#include "Plate1536Dialog.h"    // new dialog
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QLabel>
#include <QRadioButton>

PlateMapDialog::PlateMapDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Create Plate Map");

    // Plates
    plate384 = new PlateWidget(16, 24, this);
    plate96  = new PlateWidget( 8, 12, this);

    // Mode buttons
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
            &PlateMapDialog::onSelectionChanged);

    // Sample & dilution controls
    sampleCombo = new QComboBox(this);
    int maxSample384 = plate384->rows() * plate384->cols();
    for (int i = 1; i <= maxSample384; ++i)
        sampleCombo->addItem(QString::number(i), i);
    connect(sampleCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &PlateMapDialog::onSampleChanged);

    dilutionSpin = new QSpinBox(this);
    dilutionSpin->setRange(1, plate384->cols());
    dilutionSpin->setValue(1);
    connect(dilutionSpin,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &PlateMapDialog::onDilutionChanged);

    auto controlsLayout = new QHBoxLayout;
    controlsLayout->addWidget(noneBtn);
    controlsLayout->addWidget(sampleBtn);
    controlsLayout->addWidget(new QLabel("Sample ID:", this));
    controlsLayout->addWidget(sampleCombo);
    controlsLayout->addWidget(new QLabel("Dilution:", this));
    controlsLayout->addWidget(dilutionSpin);
    controlsLayout->addWidget(dmsoBtn);
    controlsLayout->addWidget(stdBtn);

    // 384-well buttons
    export384Btn = new QPushButton("Export 384 CSV", this);
    load384Btn   = new QPushButton("Load 384 CSV", this);
    clear384Btn  = new QPushButton("Clear 384", this);
    undo384Btn   = new QPushButton("Undo 384", this);
    connect(export384Btn, &QPushButton::clicked, this, &PlateMapDialog::export384);
    connect(load384Btn,   &QPushButton::clicked, this, &PlateMapDialog::load384);
    connect(clear384Btn,  &QPushButton::clicked, plate384, &PlateWidget::clearLayout);
    connect(undo384Btn,   &QPushButton::clicked, plate384, &PlateWidget::undo);

    // 96-well buttons
    export96Btn = new QPushButton("Export 96 CSV", this);
    load96Btn   = new QPushButton("Load 96 CSV", this);
    clear96Btn  = new QPushButton("Clear 96", this);
    undo96Btn   = new QPushButton("Undo 96", this);
    connect(export96Btn, &QPushButton::clicked, this, &PlateMapDialog::export96);
    connect(load96Btn,   &QPushButton::clicked, this, &PlateMapDialog::load96);
    connect(clear96Btn,  &QPushButton::clicked, plate96,  &PlateWidget::clearLayout);
    connect(undo96Btn,   &QPushButton::clicked, plate96,  &PlateWidget::undo);

    // 1536-well dialog launcher
    open1536Btn = new QPushButton("1536-well…", this);
    connect(open1536Btn,
            &QPushButton::clicked,
            this,
            &PlateMapDialog::open1536Dialog);

    // Button layout
    auto btnLayout = new QHBoxLayout;
    btnLayout->addWidget(export384Btn);
    btnLayout->addWidget(load384Btn);
    btnLayout->addWidget(clear384Btn);
    btnLayout->addWidget(undo384Btn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(export96Btn);
    btnLayout->addWidget(load96Btn);
    btnLayout->addWidget(clear96Btn);
    btnLayout->addWidget(undo96Btn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(open1536Btn);

    // Plates layout
    auto platesLayout = new QHBoxLayout;
    platesLayout->addWidget(plate384);
    platesLayout->addWidget(plate96);

    // Main layout
    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(controlsLayout);
    mainLayout->addLayout(platesLayout);
    mainLayout->addLayout(btnLayout);
    setLayout(mainLayout);
}

void PlateMapDialog::onSelectionChanged(int id)
{
    PlateWidget::WellType type = static_cast<PlateWidget::WellType>(id);
    plate384->setCurrentWellType(type);
    plate96->setCurrentWellType(type);
}

void PlateMapDialog::onSampleChanged(int index)
{
    int id = sampleCombo->itemData(index).toInt();
    plate384->setCurrentSample(id);
    plate96->setCurrentSample(id);
}

void PlateMapDialog::onDilutionChanged(int step)
{
    plate384->setCurrentDilutionStep(step);
    plate96->setCurrentDilutionStep(step);
}

void PlateMapDialog::writeCSV(const QString& defaultName,
                              PlateWidget* widget,
                              int totalWells)
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "Save CSV", defaultName, "CSV Files (*.csv)"
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
        ts << QChar('A' + row) << (col + 1) << ",";

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

void PlateMapDialog::export384() { writeCSV("layout_384.csv", plate384, 384); }
void PlateMapDialog::load384()
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
    tsIn.readLine();  // skip header

    int cols = plate384->cols();
    int rows = plate384->rows();
    QVector<PlateWidget::WellData> data(rows * cols);

    while (!tsIn.atEnd()) {
        QStringList parts = tsIn.readLine().split(',');
        if (parts.size() < 2) continue;

        int r   = parts[0][0].toLatin1() - 'A';
        int c   = parts[0].mid(1).toInt() - 1;
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
    plate384->loadLayout(data);
}

void PlateMapDialog::export96() { writeCSV("layout_96.csv", plate96, 96); }
void PlateMapDialog::load96()
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

    int cols = plate96->cols();
    int rows = plate96->rows();
    QVector<PlateWidget::WellData> data(rows * cols);

    while (!tsIn.atEnd()) {
        QStringList parts = tsIn.readLine().split(',');
        if (parts.size() < 2) continue;

        int r   = parts[0][0].toLatin1() - 'A';
        int c   = parts[0].mid(1).toInt() - 1;
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
    plate96->loadLayout(data);
}

void PlateMapDialog::open1536Dialog()
{
    Plate1536Dialog dlg(this);
    dlg.exec();
}
