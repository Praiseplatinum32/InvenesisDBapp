#include "PlateMapDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

PlateMapDialog::PlateMapDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Create Plate Map");

    plate384 = new PlateWidget(16, 24, this);
    plate96  = new PlateWidget(8, 12, this);

    QRadioButton* noneBtn   = new QRadioButton("None", this);
    QRadioButton* sampleBtn = new QRadioButton("Sample", this);
    QRadioButton* dmsoBtn   = new QRadioButton("DMSO", this);
    QRadioButton* stdBtn    = new QRadioButton("Standard", this);
    noneBtn->setChecked(true);

    modeGroup = new QButtonGroup(this);
    modeGroup->addButton(noneBtn, PlateWidget::None);
    modeGroup->addButton(sampleBtn, PlateWidget::Sample);
    modeGroup->addButton(dmsoBtn, PlateWidget::DMSO);
    modeGroup->addButton(stdBtn, PlateWidget::Standard);
    connect(modeGroup, &QButtonGroup::idClicked,
            this, &PlateMapDialog::onSelectionChanged);

    sampleCombo = new QComboBox(this);
    for (int i = 1; i <= 10; ++i)
        sampleCombo->addItem(QString::number(i), i);
    connect(sampleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlateMapDialog::onSampleChanged);

    dilutionSpin = new QDoubleSpinBox(this);
    dilutionSpin->setRange(0.01, 100.0);
    dilutionSpin->setDecimals(3);
    dilutionSpin->setSingleStep(0.1);
    dilutionSpin->setValue(1.0);
    connect(dilutionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlateMapDialog::onDilutionChanged);

    QHBoxLayout* controlsLayout = new QHBoxLayout;
    controlsLayout->addWidget(noneBtn);
    controlsLayout->addWidget(sampleBtn);
    controlsLayout->addWidget(new QLabel("Sample ID:"));
    controlsLayout->addWidget(sampleCombo);
    controlsLayout->addWidget(new QLabel("Dilution:"));
    controlsLayout->addWidget(dilutionSpin);
    controlsLayout->addWidget(dmsoBtn);
    controlsLayout->addWidget(stdBtn);

    export384Btn = new QPushButton("Export 384 CSV");
    load384Btn   = new QPushButton("Load 384 CSV");
    clear384Btn  = new QPushButton("Clear 384");
    undo384Btn   = new QPushButton("Undo 384");
    connect(export384Btn, &QPushButton::clicked, this, &PlateMapDialog::export384);
    connect(load384Btn,   &QPushButton::clicked, this, &PlateMapDialog::load384);
    connect(clear384Btn,  &QPushButton::clicked, plate384, &PlateWidget::clearLayout);
    connect(undo384Btn,   &QPushButton::clicked, plate384, &PlateWidget::undo);

    export96Btn = new QPushButton("Export 96 CSV");
    load96Btn   = new QPushButton("Load 96 CSV");
    clear96Btn  = new QPushButton("Clear 96");
    undo96Btn   = new QPushButton("Undo 96");
    connect(export96Btn, &QPushButton::clicked, this, &PlateMapDialog::export96);
    connect(load96Btn,   &QPushButton::clicked, this, &PlateMapDialog::load96);
    connect(clear96Btn,  &QPushButton::clicked, plate96, &PlateWidget::clearLayout);
    connect(undo96Btn,   &QPushButton::clicked, plate96, &PlateWidget::undo);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addWidget(export384Btn);
    btnLayout->addWidget(load384Btn);
    btnLayout->addWidget(clear384Btn);
    btnLayout->addWidget(undo384Btn);
    btnLayout->addSpacing(20);
    btnLayout->addWidget(export96Btn);
    btnLayout->addWidget(load96Btn);
    btnLayout->addWidget(clear96Btn);
    btnLayout->addWidget(undo96Btn);

    QHBoxLayout* platesLayout = new QHBoxLayout;
    platesLayout->addWidget(plate384);
    platesLayout->addWidget(plate96);

    QVBoxLayout* mainLayout = new QVBoxLayout;
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

void PlateMapDialog::onDilutionChanged(double value)
{
    plate384->setCurrentDilution(value);
    plate96->setCurrentDilution(value);
}

void PlateMapDialog::writeCSV(const QString& filename, PlateWidget* widget, int wells)
{
    QString filePath = QFileDialog::getSaveFileName(this, "Save CSV", filename, "CSV Files (*.csv)");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot write file");
        return;
    }

    QTextStream ts(&file);
    ts << QFileInfo(filePath).fileName() << "," << wells << ",user_layout\n";

    auto layout = widget->layout();
    int cols = widget->cols();
    for (int i = 0; i < layout.size(); ++i) {
        const auto& wd = layout[i];
        if (wd.type == PlateWidget::None)
            continue;
        int row = i / cols;
        int col = i % cols;
        ts << QChar('A' + row) << (col + 1) << ",";
        if (wd.type == PlateWidget::Sample) {
            ts << "SAMPLE," << wd.sampleId << "," << wd.dilution;
        } else if (wd.type == PlateWidget::DMSO) {
            ts << "DMSO";
        } else /* Standard */ {
            ts << "STANDARD";
        }
        ts << "\n";
    }
}

void PlateMapDialog::export384() { writeCSV("layout_384.csv", plate384, 384); }
void PlateMapDialog::export96()  { writeCSV("layout_96.csv", plate96, 96); }

void PlateMapDialog::load384()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open CSV", {}, "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file");
        return;
    }
    QTextStream ts(&file);
    ts.readLine(); // skip header

    int cols = plate384->cols();
    int rows = plate384->rows();
    QVector<PlateWidget::WellData> data(rows * cols);

    while (!ts.atEnd()) {
        QStringList parts = ts.readLine().split(',');
        if (parts.size() < 2) continue;
        int r = parts[0][0].toLatin1() - 'A';
        int c = parts[0].mid(1).toInt() - 1;
        int idx = r * cols + c;
        if (idx < 0 || idx >= data.size()) continue;

        PlateWidget::WellData wd;
        QString t = parts[1].toUpper();
        if (t == "SAMPLE" && parts.size() >= 4) {
            wd.type = PlateWidget::Sample;
            wd.sampleId = parts[2].toInt();
            wd.dilution = parts[3].toDouble();
        } else if (t == "DMSO") {
            wd.type = PlateWidget::DMSO;
        } else if (t == "STANDARD") {
            wd.type = PlateWidget::Standard;
        }
        data[idx] = wd;
    }
    plate384->loadLayout(data);
}

void PlateMapDialog::load96()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open CSV", {}, "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file");
        return;
    }
    QTextStream ts(&file);
    ts.readLine();

    int cols = plate96->cols();
    int rows = plate96->rows();
    QVector<PlateWidget::WellData> data(rows * cols);

    while (!ts.atEnd()) {
        QStringList parts = ts.readLine().split(',');
        if (parts.size() < 2) continue;
        int r = parts[0][0].toLatin1() - 'A';
        int c = parts[0].mid(1).toInt() - 1;
        int idx = r * cols + c;
        if (idx < 0 || idx >= data.size()) continue;

        PlateWidget::WellData wd;
        QString t = parts[1].toUpper();
        if (t == "SAMPLE" && parts.size() >= 4) {
            wd.type = PlateWidget::Sample;
            wd.sampleId = parts[2].toInt();
            wd.dilution = parts[3].toDouble();
        } else if (t == "DMSO") {
            wd.type = PlateWidget::DMSO;
        } else if (t == "STANDARD") {
            wd.type = PlateWidget::Standard;
        }
        data[idx] = wd;
    }
    plate96->loadLayout(data);
}
