#include "PlateMapDialog.h"
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
#include <QMap>

// ============================ Helpers =====================================

QString PlateMapDialog::toA01(int row1, int col1)
{
    const QChar rowCh = QChar('A' + (row1 - 1));
    return QString("%1%2").arg(rowCh).arg(col1, 2, 10, QChar('0'));
}

bool PlateMapDialog::parseA01(const QString& a01, int& row1, int& col1)
{
    if (a01.isEmpty()) return false;
    const QString s = a01.trimmed().toUpper();
    if (s.size() < 2) return false;

    const QChar r = s.at(0);
    bool ok = false;
    const int c = s.mid(1).toInt(&ok);
    if (!ok) return false;

    row1 = r.unicode() - QChar('A').unicode() + 1;
    col1 = c;
    return (row1 >= 1 && col1 >= 1);
}

QString PlateMapDialog::roleToString(PlateWidget::WellType t)
{
    switch (t) {
    case PlateWidget::Sample:   return "sample";
    case PlateWidget::Standard: return "standard";
    case PlateWidget::DMSO:     return "placebo";
    case PlateWidget::None:
    default:                    return "Void";
    }
}

PlateWidget::WellType PlateMapDialog::roleFromString(const QString& s)
{
    const QString t = s.trimmed().toLower();
    if (t == "sample")   return PlateWidget::Sample;
    if (t == "standard") return PlateWidget::Standard;
    if (t == "dmso")     return PlateWidget::DMSO;
    if (t == "void" || t.isEmpty()) return PlateWidget::None;

    // Backward-compatibility fallbacks for odd tokens
    if (t == "samples" || t == "sample_id") return PlateWidget::Sample;
    return PlateWidget::None;
}

// ============================ UI Setup ====================================

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
    connect(modeGroup, &QButtonGroup::idClicked,
            this,       &PlateMapDialog::onSelectionChanged);

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
    connect(open1536Btn, &QPushButton::clicked,
            this,        &PlateMapDialog::open1536Dialog);

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

// ============================ Slots =======================================

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

// ============================ CSV Export ==================================
// New schema header:
// layoutWell,layoutRow,layoutCol,layoutRole,layoutCompoundInPlate,layoutDilInPlate
// Always output every well (Void for empty).

void PlateMapDialog::writeCSV(const QString& defaultName,
                              PlateWidget* widget,
                              int /*totalWells*/)
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "Save CSV", defaultName, "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;

    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot write file");
        return;
    }

    const int rows = widget->rows();
    const int cols = widget->cols();
    const auto layout = widget->layout(); // QVector<PlateWidget::WellData>

    QTextStream ts(&out);
    ts << ";;;;;;\n";
    ts << "layoutWell,layoutRow,layoutCol,layoutRole,layoutCompoundInPlate,layoutDilInPlate\n";

    for (int r0 = 0; r0 < rows; ++r0) {
        for (int c0 = 0; c0 < cols; ++c0) {
            const int idx  = r0 * cols + c0;
            const int row1 = r0 + 1;
            const int col1 = c0 + 1;

            PlateWidget::WellData wd; // defaults to None/Void
            if (idx >= 0 && idx < layout.size()) wd = layout[idx];

            const QString role = roleToString(wd.type);

            QString sampleId, dilStep;
            switch (wd.type) {
            case PlateWidget::Sample:
            case PlateWidget::Standard:   // <-- export Standard like Sample
                sampleId = QString::number(wd.sampleId);
                dilStep  = QString::number(wd.dilutionStep);
                break;
            case PlateWidget::DMSO:       // 0,0
            case PlateWidget::None:       // Void -> 0,0
                sampleId = "0";
                dilStep  = "0";
                break;
            }

            ts << toA01(row1, col1) << ','
               << row1 << ','
               << col1 << ','
               << role << ','
               << sampleId << ','
               << dilStep << '\n';
        }
    }
}




void PlateMapDialog::export384() { writeCSV("layout_384.csv", plate384, 384); }
void PlateMapDialog::export96()  { writeCSV("layout_96.csv",  plate96,   96); }

// ============================ CSV Import ==================================
//
// New schema (preferred):
// layoutWell,layoutRow,layoutCol,layoutRole,layoutCompoundInPlate,layoutDilInPlate
//
// Old schema (fallback):
// <WellToken>,<ROLE>[,<SampleId>,<DilStep>]
//
// The loaders try the new schema first; if it doesn't match, they fall back.

namespace {
QVector<PlateWidget::WellData>
readPlateCSV_NewSchema(QTextStream& ts, int rows, int cols)
{
    QVector<PlateWidget::WellData> data(rows * cols);
    const QString header = ts.readLine();
    if (header.isNull()) return {};

    const QStringList headCols = header.split(',', Qt::KeepEmptyParts);
    QMap<QString,int> h;
    for (int i=0;i<headCols.size();++i)
        h[headCols[i].trimmed().toLower()] = i;

    const bool hasNew =
        h.contains("layoutwell") &&
        h.contains("layoutrow") &&
        h.contains("layoutcol") &&
        h.contains("layoutrole");
    if (!hasNew) return {};

    auto colAt = [&](const QString& key, const QStringList& parts)->QString {
        int ci = h.value(key, -1);
        return (ci >= 0 && ci < parts.size()) ? parts[ci] : QString();
    };

    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;

        const QStringList parts = line.split(',', Qt::KeepEmptyParts);

        int row1 = colAt("layoutrow", parts).toInt();
        int col1 = colAt("layoutcol", parts).toInt();
        if (row1 <= 0 || col1 <= 0) {
            int r=0,c=0;
            PlateMapDialog::parseA01(colAt("layoutwell", parts), r, c);
            row1 = r; col1 = c;
        }
        const int r0 = row1 - 1, c0 = col1 - 1;
        if (r0 < 0 || r0 >= rows || c0 < 0 || c0 >= cols) continue;

        PlateWidget::WellData wd;
        wd.type = PlateMapDialog::roleFromString(colAt("layoutrole", parts));

        if (wd.type == PlateWidget::Sample || wd.type == PlateWidget::Standard) {
            wd.sampleId     = colAt("layoutcompoundinplate", parts).toInt();
            wd.dilutionStep = colAt("layoutdilinplate", parts).toInt();
        } else if (wd.type == PlateWidget::DMSO || wd.type == PlateWidget::None) {
            // Enforce zeros for DMSO/Void
            wd.sampleId     = 0;
            wd.dilutionStep = 0;
        }

        data[r0 * cols + c0] = wd;
    }
    return data;
}
}



// Old schema fallback (caller must have already skipped the old header)
QVector<PlateWidget::WellData>
readPlateCSV_OldSchema(QTextStream& ts, int rows, int cols)
{
    QVector<PlateWidget::WellData> data(rows * cols);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty()) continue;

        const QStringList parts = line.split(',', Qt::KeepEmptyParts);
        if (parts.size() < 2) continue;

        // parts[0] like "A1" or "A01"
        QString wellTok = parts[0].trimmed();
        if (wellTok.size() == 2 && wellTok[1].isDigit()) {
            // normalize A1 -> A01
            wellTok = wellTok[0] + QString("%1").arg(wellTok.mid(1).toInt(), 2, 10, QChar('0'));
        }

        int row1=0, col1=0;
        if (!PlateMapDialog::parseA01(wellTok, row1, col1)) continue;
        const int r0 = row1 - 1, c0 = col1 - 1;
        if (r0 < 0 || r0 >= rows || c0 < 0 || c0 >= cols) continue;

        PlateWidget::WellData wd;
        const QString t = parts[1].trimmed().toUpper();
        if (t == "SAMPLE" && parts.size() >= 4) {
            wd.type         = PlateWidget::Sample;
            wd.sampleId     = parts[2].toInt();
            wd.dilutionStep = parts[3].toInt();
        } else if (t == "DMSO") {
            wd.type = PlateWidget::DMSO;
        } else if (t == "STANDARD") {
            wd.type = PlateWidget::Standard;
        } else {
            wd.type = PlateWidget::None;
        }

        data[r0 * cols + c0] = wd;
    }
    return data;
}
 // anonymous namespace

void PlateMapDialog::load384()
{
    QString file = QFileDialog::getOpenFileName(
        this, "Open CSV", {}, "CSV Files (*.csv)");
    if (file.isEmpty()) return;

    QFile in(file);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file");
        return;
    }

    QTextStream ts(&in);
    const qint64 pos0 = in.pos();

    QVector<PlateWidget::WellData> data =
        readPlateCSV_NewSchema(ts, plate384->rows(), plate384->cols());

    if (data.isEmpty()) {
        // Fallback to old schema: rewind and skip the old header line
        in.seek(pos0);
        ts.seek(0);
        ts.readLine(); // skip old header line "<file>,<n>,user_layout"
        data = readPlateCSV_OldSchema(ts, plate384->rows(), plate384->cols());
    }

    plate384->loadLayout(data);
}

void PlateMapDialog::load96()
{
    QString file = QFileDialog::getOpenFileName(
        this, "Open CSV", {}, "CSV Files (*.csv)");
    if (file.isEmpty()) return;

    QFile in(file);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file");
        return;
    }

    QTextStream ts(&in);
    const qint64 pos0 = in.pos();

    QVector<PlateWidget::WellData> data =
        readPlateCSV_NewSchema(ts, plate96->rows(), plate96->cols());

    if (data.isEmpty()) {
        in.seek(pos0);
        ts.seek(0);
        ts.readLine(); // skip old header line
        data = readPlateCSV_OldSchema(ts, plate96->rows(), plate96->cols());
    }

    plate96->loadLayout(data);
}

void PlateMapDialog::open1536Dialog()
{
    Plate1536Dialog dlg(this);
    dlg.exec();
}
