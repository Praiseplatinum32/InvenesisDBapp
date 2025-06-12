#include "tecanwindow.h"
#include "PlateMapDialog.h"
#include "ui_tecanwindow.h"

// Qt
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardItemModel>
#include <QInputDialog>
#include <QMessageBox>
#include <QColor>
#include <QRandomGenerator>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>        // std::sort
#include <QJsonDocument>
#include <QVariant>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QDebug>
#include <QSet>

// Project
#include "daughterplatewidget.h"
#include "standardselectiondialog.h"
#include "loadexperimentdialog.h"
#include "gwlgenerator.h"

/* ============================================================
 *  Canonical JSON  – deterministic order for objects & arrays
 *  (place once in tecanwindow.cpp, after #includes)
 * ============================================================ */
#include <algorithm>
#include <QJsonDocument>

/* ----- helper: convert ANY QJsonValue to stable compact bytes ----- */
static QByteArray jBytes(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Object:
        return QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact);
    case QJsonValue::Array:
        return QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact);
    case QJsonValue::String:
        return v.toString().toUtf8();
    case QJsonValue::Double:
        return QByteArray::number(v.toDouble(), 'g', 16);
    case QJsonValue::Bool:
        return v.toBool() ? "true" : "false";
    default:                         /* Null / Undefined */
        return "null";
    }
}

/* forward decl */
static QJsonValue canonJson(const QJsonValue &v);

/* ----- key‑sorted object -------------------------------------------- */
static QJsonObject canonObject(const QJsonObject &in)
{
    QJsonObject out;
    QStringList keys = in.keys();
    std::sort(keys.begin(), keys.end(),
              [](const QString &a, const QString &b){
                  return a.localeAwareCompare(b) < 0;
              });
    for (const QString &k : keys)
        out.insert(k, canonJson(in.value(k)));
    return out;
}

/* ----- array sorted by canonical byte string ------------------------ */
static QJsonArray canonArray(const QJsonArray &in)
{
    QList<QJsonValue> lst;
    lst.reserve(in.size());
    for (const QJsonValue &v : in)
        lst.append(canonJson(v));

    std::sort(lst.begin(), lst.end(),
              [](const QJsonValue &a, const QJsonValue &b){
                  return jBytes(a) < jBytes(b);
              });

    QJsonArray out;
    for (const QJsonValue &v : std::as_const(lst))
        out.append(v);
    return out;
}

/* ----- dispatch ----------------------------------------------------- */
static QJsonValue canonJson(const QJsonValue &v)
{
    if (v.isObject()) return canonObject(v.toObject());
    if (v.isArray())  return canonArray(v.toArray());
    /* treat empty string and null as equivalent */
    if (v.isString() && v.toString().trimmed().isEmpty()) return QJsonValue();
    return v;              // primitive number/bool/null or undefined
}

/* public helpers ----------------------------------------------------- */
static QJsonObject canonicalise(const QJsonObject &obj)
{
    return canonObject(obj);
}

/* deep equality, ignoring order & numeric‑string mismatch */
static bool jsonEqual(const QJsonValue &a, const QJsonValue &b)
{
    const QJsonValue ca = canonJson(a), cb = canonJson(b);

    if (ca.type() != cb.type()) {
        /* tolerate number <-> string if content matches */
        if ((ca.isDouble() && cb.isString()) ||
            (ca.isString() && cb.isDouble()))
            return ca.toString() == cb.toString();
        return false;
    }
    return jBytes(ca) == jBytes(cb);
}





using SqlModelUPtr = std::unique_ptr<QSqlQueryModel>;



namespace                       /* ====== file‑local helpers / constants ====== */
{
    static constexpr int  kMaxColumns    = 12;
    static const QStringList kPlateRows  = {"A","B","C","D","E","F","G","H"};


} // unnamed namespace

/* ======= static QMessageBox wrappers (header declared) ======= */
void TecanWindow::showInfo(QWidget *parent, const QString &title,
                           const QString &msg)
{ QMessageBox::information(parent, title, msg); }

void TecanWindow::showWarning(QWidget *parent, const QString &title,
                              const QString &msg)
{ QMessageBox::warning(parent, title, msg); }

void TecanWindow::showError(QWidget *parent, const QString &title,
                            const QString &msg)
{ QMessageBox::critical(parent, title, msg); }

/* ========================================================================== */
/*                              ctor / dtor                                   */
/* ========================================================================== */
TecanWindow::TecanWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::TecanWindow),
      testRequestModel(std::make_unique<QSqlQueryModel>()),
      compoundQueryModel(std::make_unique<QSqlQueryModel>())
{
    ui->setupUi(this);
    showMaximized();
    setCentralWidget(ui->splitter_3);
    setWindowTitle(QStringLiteral("Tecan Automation Interface"));

    ui->testRequestTableView->setModel(testRequestModel.get());
    ui->compoundQueryTableView->setModel(compoundQueryModel.get());

    ui->compoundQueryTableView->setDragEnabled(true);
    ui->compoundQueryTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->compoundQueryTableView->setDragDropMode(QAbstractItemView::DragOnly);

    /* --- plate visual containers --- */
    matrixPlateContainer = new MatrixPlateContainer(this);
    ui->plateDisplayScrollArea->setWidget(matrixPlateContainer);

    daughterPlatesContainerWidget = new QWidget(this);
    daughterPlatesLayout = new QVBoxLayout(daughterPlatesContainerWidget);
    daughterPlatesLayout->setAlignment(Qt::AlignTop);
    ui->daughterPlateScrollArea->setWidgetResizable(true);
    ui->daughterPlateScrollArea->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    ui->daughterPlateScrollArea->setWidget(daughterPlatesContainerWidget);
}

TecanWindow::~TecanWindow()                                          = default;

/* ========================================================================== */
/*                         test‑request / solution logic                      */
/* ========================================================================== */
void TecanWindow::loadTestRequests(const QStringList &requestIDs)
{
    if (requestIDs.isEmpty()) {
        showInfo(this, tr("No Selection"), tr("No test requests selected."));
        return;
    }

    const QString placeholders = '\'' + requestIDs.join("','") + '\'';
    const QString queryStr = QStringLiteral(
          "SELECT * FROM test_requests WHERE request_id IN (%1)").arg(placeholders);

    testRequestModel->setQuery(queryStr);
    if (testRequestModel->lastError().isValid()) {
        showError(this, tr("Query Error"), testRequestModel->lastError().text());
        return;
    }

    ui->testRequestTableView->resizeColumnsToContents();
    querySolutionsFromTestRequests();
}

void TecanWindow::querySolutionsFromTestRequests()
{
    QSet<QString> compoundNames;
    for (int row = 0, rows = testRequestModel->rowCount(); row < rows; ++row)
        compoundNames.insert(
            testRequestModel->record(row).value("compound_name").toString());

    querySolutions(compoundNames);
}

void TecanWindow::querySolutions(const QSet<QString> &compoundNames)
{
    QList<int> selectedSolutionIds;
    QSqlQuery   query;

    for (const QString &compound : compoundNames)
    {
        query.prepare(R"(
            SELECT solution_id, product_name, invenesis_solution_id, weight, weight_unit,
                   concentration, concentration_unit, container_id, well_id, matrix_tube_id
            FROM   solutions
            WHERE  product_name = :compound)");
        query.bindValue(":compound", compound);

        if (!query.exec()) {
            showError(this, tr("Query Error"), query.lastError().text());
            continue;
        }

        QList<QVariantMap> solutionsFound;
        while (query.next())
        {
            QVariantMap sol;
            const QSqlRecord rec = query.record();
            for (int i = 0; i < rec.count(); ++i)
                sol[rec.fieldName(i)] = rec.value(i);
            solutionsFound.append(sol);
        }

        /* ----- handle #matches per compound ----- */
        if (solutionsFound.size() == 1)
            selectedSolutionIds << solutionsFound.first()["solution_id"].toInt();
        else if (solutionsFound.size() > 1) {
            const int id = resolveCompoundDuplicates(compound, solutionsFound);
            if (id != -1) selectedSolutionIds << id;
        } else {
            showWarning(this, tr("No Solution Found"),
                        tr("No solution found for compound '%1'.").arg(compound));
        }
    }

    populateCompoundTable(selectedSolutionIds);
}

int TecanWindow::resolveCompoundDuplicates(const QString &compoundName,
                                           const QList<QVariantMap> &duplicates)
{
    QStringList items;
    QMap<QString,int> itemToId;

    for (const auto &sol : duplicates)
    {
        const QString desc = QStringLiteral(
            "Solution ID: %1 | Container: %2 | Well: %3 | Conc.: %4 %5")
            .arg(sol["invenesis_solution_id"].toString(),
                 sol["container_id"].toString(),
                 sol["well_id"].toString(),
                 sol["concentration"].toString(),
                 sol["concentration_unit"].toString());

        items << desc;
        itemToId[desc] = sol["solution_id"].toInt();
    }

    bool ok = false;
    const QString choice = QInputDialog::getItem(
        this, tr("Select Solution for %1").arg(compoundName),
        tr("Multiple solutions found. Please select one:"),
        items, 0, false, &ok);

    return ok && itemToId.contains(choice) ? itemToId.value(choice) : -1;
}

void TecanWindow::populateCompoundTable(const QList<int> &solutionIds)
{
    if (solutionIds.isEmpty()) {
        showInfo(this, tr("No Solutions"),
                 tr("No solutions selected to display."));
        return;
    }

    QStringList idPlaceholders;
    for (int id : solutionIds) idPlaceholders << QString::number(id);

    const QString queryStr = QStringLiteral(R"(
        SELECT product_name, invenesis_solution_id, weight, weight_unit,
               concentration, concentration_unit, container_id, well_id, matrix_tube_id
        FROM   solutions
        WHERE  solution_id IN (%1))").arg(idPlaceholders.join(','));

    compoundQueryModel->setQuery(queryStr);
    if (compoundQueryModel->lastError().isValid()) {
        showError(this, tr("Query Error"),
                  compoundQueryModel->lastError().text());
        return;
    }
    ui->compoundQueryTableView->resizeColumnsToContents();

    /* ---------- update visual matrix plate ---------- */
    QMap<QString,QSet<QString>> plateData;
    for (int i = 0, rows = compoundQueryModel->rowCount(); i < rows; ++i) {
        const QSqlRecord rec = compoundQueryModel->record(i);
        plateData[rec.value("container_id").toString()]
                .insert(rec.value("well_id").toString());
    }
    matrixPlateContainer->populatePlates(plateData);

    /* ---------- prepare daughter plates ---------- */
    QStringList compounds;
    for (int i = 0, rows = compoundQueryModel->rowCount(); i < rows; ++i)
        compounds.removeDuplicates(),
        compounds << compoundQueryModel->record(i)
                      .value("product_name").toString();

    const int  dilutionSteps =
        testRequestModel->record(0).value("number_of_dilutions").toInt();
    const QString testType =
        testRequestModel->record(0).value("requested_tests").toString();

    populateDaughterPlates(dilutionSteps, compounds, testType);
}

/* ========================================================================== */
/*                                plate logic                                 */
/* ========================================================================== */
void TecanWindow::populateDaughterPlates(int dilutionSteps,
                                         const QStringList &compoundList,
                                         const QString &testType)
{
    /* ---- clear previous widgets ---- */
    while (auto *item = daughterPlatesLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    /* ---- determine special (INV‑T‑031) layout? ---- */
    const bool isINV_T_031  = testType.contains(QLatin1String("INV-T-031"));
    const int  standardCol  = isINV_T_031 ? 11 : -1;
    const int  dmsoCol      = isINV_T_031 ? 12 : -1;

    /* ---- build list of fixed wells ---- */
    QStringList standardWells, dmsoWells;
    if (isINV_T_031) {
        for (const QString &row : kPlateRows) {
            standardWells << row + QString::number(standardCol);
            dmsoWells     << row + QString::number(dmsoCol);
        }
    } else {
        const int stdDilutions = qMax(dilutionSteps, 6);
        for (int c = 1; c <= stdDilutions; ++c) standardWells << "A" + QString::number(c);
        for (int c = 1; c <= kMaxColumns; ++c)  dmsoWells     << "H" + QString::number(c);
    }

    /* ---------------------------------------------------------------------- */
    QList<QMap<QString,QStringList>> plates(1);
    plates[0]["Standard"] = standardWells;
    plates[0]["DMSO"]     = dmsoWells;

    int plateIdx   = 0;
    int curRowIdx  = isINV_T_031 ? 0 : 1;     // skip row A if not special
    int curColIdx  = 1;

    auto startNewPlate = [&]{
        plates.append({{"Standard",standardWells}, {"DMSO",dmsoWells}});
        ++plateIdx;
        curRowIdx = isINV_T_031 ? 0 : 1;
        curColIdx = 1;
    };

    /* ---- place every compound, honouring dilution steps & reserved cols ---- */
    for (const QString &cmpd : compoundList)
    {
        while (true)                              // find first valid region
        {
            const int lastCol = curColIdx + dilutionSteps - 1;
            const bool overlapSpecial = isINV_T_031 && lastCol >= standardCol;
            const bool exceedCols     = lastCol > (isINV_T_031 ? standardCol-1
                                                                : kMaxColumns);
            if (!overlapSpecial && !exceedCols) break;

            if (++curRowIdx >= kPlateRows.size() - (isINV_T_031 ? 0 : 1))
                startNewPlate();
        }

        QStringList wells;
        for (int d = 0; d < dilutionSteps; ++d)
            wells << kPlateRows[curRowIdx] + QString::number(curColIdx + d);

        plates[plateIdx][cmpd] = wells;

        if (++curRowIdx >= kPlateRows.size() - (isINV_T_031 ? 0 : 1))
        {
            curRowIdx = isINV_T_031 ? 0 : 1;
            curColIdx += dilutionSteps;

            if (isINV_T_031 && curColIdx >= standardCol) curColIdx = 1;
            if (curColIdx > (isINV_T_031 ? standardCol-1 : kMaxColumns))
                startNewPlate();
        }
    }

    /* ---- render daughter plates ---- */
    for (int i = 0; i < plates.size(); ++i)
    {
        auto *plateWidget = new DaughterPlateWidget(i+1, this);

        /* assign colours */
        QMap<QString,QColor> colours;
        int hue = 0, hueStep = 360 / (plates[i].size() + 1);
        for (auto it = plates[i].cbegin(); it != plates[i].cend(); ++it) {
            if (it.key() == "DMSO")      colours[it.key()] = Qt::gray;
            else if (it.key() == "Standard")
                                          colours[it.key()] = QColor(0,122,204);
            else {                       colours[it.key()] = QColor::fromHsv(hue,200,220);
                                          hue += hueStep; }
        }

        plateWidget->populatePlate(plates[i], colours, dilutionSteps);
        plateWidget->enableCompoundDragDrop(dilutionSteps);
        daughterPlatesLayout->addWidget(plateWidget);
    }
}

/* ========================================================================== */
/*                             UI slot handlers                               */
/* ========================================================================== */
void TecanWindow::on_clearPlatesButton_clicked()
{
    if (QMessageBox::question(this, tr("Clear Plates"),
        tr("Are you sure you want to clear all compound placements?\n"
           "(Standard and DMSO will be preserved)"),
        QMessageBox::Yes|QMessageBox::No) != QMessageBox::Yes)
        return;

    for (int i = 0; i < daughterPlatesLayout->count(); ++i)
        if (auto *plate = qobject_cast<DaughterPlateWidget*>(
                daughterPlatesLayout->itemAt(i)->widget()))
        {
            plate->clearCompounds();
            plate->setAcceptDrops(true);
        }
}

/* =========================================================================
 *  Persistence, load/save & GWL helpers – refactored April 2025
 *  (drop‑in replacement for the second half of tecanwindow.cpp)
 * ========================================================================= */

/* ============================================================== */
/*                      SAVE EXPERIMENT                           */
/* ============================================================== */
void TecanWindow::on_actionSave_triggered()
{
    /* --- ask experiment code ----------------------------------- */
    bool ok = false;
    const QString expCode = QInputDialog::getText(
        this, tr("Save Experiment"), tr("Enter experiment code:"),
        QLineEdit::Normal, "", &ok);
    if (!ok || expCode.trimmed().isEmpty()) return;

    /* --- ask user name ----------------------------------------- */
    const QString username = QInputDialog::getText(
        this, tr("User"), tr("Enter your name:"),
        QLineEdit::Normal, "", &ok);
    if (!ok || username.trimmed().isEmpty()) return;

    /* --- ask for STANDARD compound ----------------------------- */
    StandardSelectionDialog stdDlg(this);
    if (stdDlg.exec() != QDialog::Accepted) {
        showInfo(this, tr("Cancelled"), tr("Save aborted."));
        return;
    }
    const QJsonObject stdObj = stdDlg.selectedStandardJson();
    if (stdObj.isEmpty()) {
        showWarning(this, tr("Invalid Standard"),
                    tr("No valid standard selected."));
        return;
    }

    /* --- build CURRENT JSON snapshot --------------------------- */
    QJsonObject expJson = buildCurrentExperimentJson(expCode, username);
    if (expJson.isEmpty()) return;          // build routine already showed msg
    expJson["standard"] = stdObj;

    qDebug() << QJsonDocument(expJson).toJson(QJsonDocument::Indented);

    /* --- write to DB ------------------------------------------- */
    QSqlQuery q;
    q.prepare(R"(
        INSERT INTO experiments (experiment_code, project_code, date_created, data)
        VALUES (:code, :project, NOW(), :data)
        ON CONFLICT (experiment_code) DO UPDATE
          SET date_created = NOW(), data = EXCLUDED.data
        RETURNING experiment_id)");
    q.bindValue(":code",    expCode);
    q.bindValue(":project", expJson["project_code"].toString());
    q.bindValue(":data",    QString(QJsonDocument(expJson)
                                     .toJson(QJsonDocument::Compact)));

    if (!q.exec() || !q.next()) {
        showError(this, tr("Database Error"),
                  tr("Failed to insert/update experiment:\n%1")
                      .arg(q.lastError().text()));
        return;
    }
    const int expId = q.value(0).toInt();

    /* --- link experiment to each test request ------------------ */
    const QJsonArray trArr = expJson["test_requests"].toArray();
    for (const QJsonValue &v : trArr) {
        const QString reqId = v.toObject().value("request_id").toString();
        QSqlQuery link;
        link.prepare(R"(INSERT INTO experiment_requests (experiment_id, request_id)
                        VALUES (:eid, :rid)
                        ON CONFLICT DO NOTHING)");
        link.bindValue(":eid", expId);
        link.bindValue(":rid", reqId);
        if (!link.exec())
            qWarning() << "Failed to link request" << reqId << ":" << link.lastError();
    }

    showInfo(this, tr("Success"), tr("Experiment saved successfully!"));
    lastSavedExperimentJson = expJson;
}


/* ========================================================================= */
void TecanWindow::on_actionLoad_triggered()
{
    LoadExperimentDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    const int  expId    = dlg.selectedExperimentId();
    const bool readOnly = dlg.isReadOnly();

    if (expId == -1) {
        showWarning(this, tr("No Selection"),
                    tr("Please select an experiment to load."));
        return;
    }

    QSqlQuery q;
    q.prepare("SELECT experiment_code, project_code, data "
              "FROM   experiments WHERE experiment_id = :id");
    q.bindValue(":id", expId);

    if (!q.exec() || !q.next()) {
        showError(this, tr("Error"),
                  tr("Failed to load experiment:\n%1").arg(q.lastError().text()));
        return;
    }

    const QString expCode  = q.value("experiment_code").toString();
    const QString jsonData = q.value("data").toString();

    const QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        showError(this, tr("Error"),
                  tr("Invalid JSON format in experiment."));
        return;
    }

    lastSavedExperimentJson = doc.object();

    /* ---- restore UI state ---- */
    loadTestRequestsFromJson(lastSavedExperimentJson["test_requests"].toArray());
    loadCompoundsFromJson   (lastSavedExperimentJson["compounds"].toArray());
    loadMatrixPlatesFromJson(lastSavedExperimentJson["matrix_plates"].toObject());
    loadDaughterPlatesFromJson(lastSavedExperimentJson["daughter_plates"].toArray(),
                               readOnly);

    showInfo(this, tr("Experiment Loaded"),
             tr("Experiment '%1' loaded successfully.").arg(expCode));
}

/* =========================================================================
 *  JSON → model helpers
 * ========================================================================= */
void TecanWindow::loadTestRequestsFromJson(const QJsonArray &array)
{
    if (array.isEmpty()) return;

    // Build a model that preserves the JSON fields *including* number_of_dilutions
    auto *model = new QStandardItemModel(this);
    const QStringList headers = {
        "request_id",
        "project_code",
        "requested_tests",
        "compound_name",
        "starting_concentration",
        "starting_concentration_unit",
        "dilution_steps",
        "dilution_steps_unit",
        "number_of_dilutions",      // <-- newly added
        "number_of_replicate",
        "stock_concentration",
        "stock_concentration_unit",
        "concentration_to_be_tested",
        "additional_notes"
    };
    model->setHorizontalHeaderLabels(headers);

    // Populate rows
    for (const QJsonValue &val : array) {
        const QJsonObject obj = val.toObject();
        QList<QStandardItem*> row;
        for (const QString &key : headers) {
            row << new QStandardItem(obj.value(key).toVariant().toString());
        }
        model->appendRow(row);
    }

    // Swap into the view
    ui->testRequestTableView->setModel(model);
}

void TecanWindow::loadCompoundsFromJson(const QJsonArray &array)
{
    if (array.isEmpty()) return;

    auto *model = new QStandardItemModel(this);
    const QStringList headers = {
        "product_name","invenesis_solution_id","weight","weight_unit",
        "concentration","concentration_unit","container_id","well_id",
        "matrix_tube_id"};
    model->setHorizontalHeaderLabels(headers);

    for (const QJsonValue &val : array) {
        const QJsonObject obj = val.toObject();
        QList<QStandardItem*> row;
        for (const QString &key : headers)
            row << new QStandardItem(obj.value(key).toVariant().toString());
        model->appendRow(row);
    }

    compoundQueryModel->setQuery(QSqlQuery());
    ui->compoundQueryTableView->setModel(model);
}

void TecanWindow::loadMatrixPlatesFromJson(const QJsonObject &obj)
{
    QMap<QString,QSet<QString>> map;
    foreach(const QString &cid, obj.keys()) {
        QSet<QString> wells;
        foreach(const QJsonValue &w, obj[cid].toArray()) wells.insert(w.toString());
        map[cid] = wells;
    }
    matrixPlateContainer->populatePlates(map);
}

void TecanWindow::loadDaughterPlatesFromJson(const QJsonArray &array,
                                             bool readOnly)
{
    while (auto *item = daughterPlatesLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    /* extract standard info if available */
    QString stdLabel, stdNotes;
    const QJsonObject stdObj = lastSavedExperimentJson["standard"].toObject();
    const bool hasStd = !stdObj.isEmpty();
    if (hasStd) {
        stdLabel = QString("%1 – Well: %2 – %3 %4 – Barcode: %5")
            .arg(stdObj["Samplealias"].toString(),
                 stdObj["Containerposition"].toString(),
                 stdObj["Concentration"].toString(),
                 stdObj["ConcentrationUnit"].toString(),
                 stdObj["Containerbarcode"].toString());
        stdNotes = QJsonDocument(stdObj).toJson(QJsonDocument::Indented);
    }

    for (int i = 0; i < array.size(); ++i) {
        const QJsonObject plateObj = array[i].toObject();
        const int dilSteps = plateObj.value("dilution_steps").toInt(3);

        auto *plate = new DaughterPlateWidget(i+1, this);
        plate->fromJson(plateObj["wells"].toObject(), dilSteps);

        if (!readOnly) plate->enableCompoundDragDrop(dilSteps);
        if (hasStd)    plate->setStandardInfo(stdLabel, stdNotes);

        daughterPlatesLayout->addWidget(plate);
    }
}

/* =======================================================================
 *  Safe buildCurrentExperimentJson
 * ======================================================================= */
QJsonObject TecanWindow::buildCurrentExperimentJson(const QString &experimentCode,
                                                    const QString &username)
{
    /* -------------------------------------------------------------------
     * 1) get the models that are ACTUALLY shown in the views
     * ------------------------------------------------------------------- */
    const QAbstractItemModel *trModel  = ui->testRequestTableView->model();
    const QAbstractItemModel *cmpModel = ui->compoundQueryTableView->model();

    if (!trModel  || trModel->rowCount()  == 0 ||
        !cmpModel || cmpModel->rowCount() == 0)
    {
        showError(this, tr("GWL Generation"),
                  tr("Test‑request or compound table is empty."));
        return {};                                     // return an EMPTY object
    }

    /* helper ‑ locate a column by its header text */
    auto columnOf = [](const QAbstractItemModel *m, const QString &hdr) -> int
    {
        for (int c = 0; c < m->columnCount(); ++c)
            if (m->headerData(c, Qt::Horizontal).toString() == hdr)
                return c;
        return -1;
    };

    /* -------------------------------------------------------------------
     * 2) root object with simple fields
     * ------------------------------------------------------------------- */
    QJsonObject root;
    root["experiment_code"] = experimentCode;
    root["user"]            = username;

    const int projCol = columnOf(trModel, "project_code");
    root["project_code"]    = (projCol >= 0)
                               ? trModel->index(0, projCol).data().toString()
                               : QString();

    /* -------------------------------------------------------------------
     * 3) serialise TEST REQUESTS
     * ------------------------------------------------------------------- */
    QJsonArray trArray;
    for (int r = 0; r < trModel->rowCount(); ++r) {
        QJsonObject rowObj;
        for (int c = 0; c < trModel->columnCount(); ++c) {
            const QString key = trModel->headerData(c, Qt::Horizontal).toString();
            rowObj[key] = QJsonValue::fromVariant(trModel->index(r,c).data());
        }
        trArray.append(rowObj);
    }
    root["test_requests"] = trArray;

    /* -------------------------------------------------------------------
     * 4) serialise COMPOUNDS
     * ------------------------------------------------------------------- */
    QJsonArray cmpArray;
    for (int r = 0; r < cmpModel->rowCount(); ++r) {
        QJsonObject rowObj;
        for (int c = 0; c < cmpModel->columnCount(); ++c) {
            const QString key = cmpModel->headerData(c, Qt::Horizontal).toString();
            rowObj[key] = QJsonValue::fromVariant(cmpModel->index(r,c).data());
        }
        cmpArray.append(rowObj);
    }
    root["compounds"] = cmpArray;

    /* -------------------------------------------------------------------
     * 5) MATRIX plate map
     * ------------------------------------------------------------------- */
    QMap<QString,QSet<QString>> plateMap = matrixPlateContainer->getPlateMap();
    QJsonObject matrixObj;
    for (auto it = plateMap.cbegin(); it != plateMap.cend(); ++it) {
        QJsonArray wells;
        for (const QString &w : it.value()) wells.append(w);
        matrixObj[it.key()] = wells;
    }
    root["matrix_plates"] = matrixObj;

    /* -------------------------------------------------------------------
     * 6) DAUGHTER plate data
     * ------------------------------------------------------------------- */
    const int dilCol = columnOf(trModel, "number_of_dilutions");
    const int dilutionSteps =
        (dilCol >= 0) ? trModel->index(0, dilCol).data().toInt() : 3;

    QJsonArray dghtArray;
    for (int i = 0; i < daughterPlatesLayout->count(); ++i) {
        auto *plate = qobject_cast<DaughterPlateWidget *>(
            daughterPlatesLayout->itemAt(i)->widget());
        if (!plate) continue;

        QJsonObject plateObj;
        plateObj["plate_number"]   = i + 1;
        plateObj["dilution_steps"] = dilutionSteps;
        plateObj["wells"]          = plate->toJson();
        dghtArray.append(plateObj);
    }
    root["daughter_plates"] = dghtArray;

    return root;
}



/* =======================================================================
 * 1) on_actionGenerate_GWL_triggered()
 * ======================================================================= */
void TecanWindow::on_actionGenerate_GWL_triggered()
{
    qDebug() << "[TRACE] GWL button pressed";

    /* 0 – must have at least one saved snapshot ------------------------ */
       if (lastSavedExperimentJson.isEmpty()) {
    showWarning(this, tr("Not Saved"),
                tr("You must save the experiment before generating a GWL file."));
    on_actionSave_triggered();
    return;
}

/* 1 – build JSON from CURRENT UI ---------------------------------- */
   const QString code = lastSavedExperimentJson["experiment_code"].toString();
const QString user = lastSavedExperimentJson["user"].toString();
QJsonObject   currentJson = buildCurrentExperimentJson(code, user);

/* always copy the STANDARD from the last saved version ------------- */
currentJson["standard"] = lastSavedExperimentJson["standard"];

/* 2 – compare order‑insensitively ---------------------------------- */
   if (!jsonEqual(lastSavedExperimentJson, currentJson)) {
const auto choice = QMessageBox::question(
    this, tr("Experiment Modified"),
    tr("Changes have been made since last save.\n"
       "Do you want to overwrite the saved experiment?"),
    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

if (choice == QMessageBox::Cancel) return;

if (choice == QMessageBox::Yes) { on_actionSave_triggered(); return; }

/* choice == No: generate with *currentJson* (unsaved edits) */
generateGWLFromJson(currentJson);
return;
}

generateGWLFromJson(lastSavedExperimentJson);
}

/* =======================================================================
 * 2) generateGWLFromJson()
 * ======================================================================= */
void TecanWindow::generateGWLFromJson(const QJsonObject &experimentJson)
{
    qDebug() << "[TRACE] generateGWLFromJson()";

    // 1) Determine dilution factor (dilution_steps_unit) from first test request
    double dilutionFactor = 3.16;  // default
    const QJsonArray trArr = experimentJson["test_requests"].toArray();
    if (!trArr.isEmpty()) {
        const auto tr0 = trArr.at(0).toObject();
        bool ok       = false;
        double df     = tr0.value("dilution_steps_unit")
                        .toString().toDouble(&ok);
        if (ok && df > 0.0) dilutionFactor = df;
    }

    // 2) Extract test ID for volumeMap lookup
    QString testId;
    if (!trArr.isEmpty()) {
        testId = trArr.at(0).toObject()
        .value("requested_tests").toString();
    }

    // 3) Extract the stock concentration (µM) of the first compound
    double stockConcMicroM = 0.0;
    const QJsonArray cmpArr = experimentJson["compounds"].toArray();
    if (!cmpArr.isEmpty()) {
        const auto cmp0    = cmpArr.at(0).toObject();
        const double sc    = cmp0.value("concentration").toDouble();
        const QString cu   = cmp0.value("concentration_unit").toString();
        // convert mM → µM
        if (cu.compare("mM", Qt::CaseInsensitive) == 0)
            stockConcMicroM = sc * 1000.0;
        else
            stockConcMicroM = sc;
    }

    // 4) Instantiate GWLGenerator with the new signature
    QStringList gwlLines;
    try {
        GWLGenerator generator(dilutionFactor, testId, stockConcMicroM);
        gwlLines = generator.generateTransferCommands(experimentJson);
        qDebug() << "[TRACE] generator produced" << gwlLines.size() << "lines";
    } catch (const std::exception &ex) {
        showError(this, tr("GWL Generation"),
                  tr("Generator error: %1")
                      .arg(QString::fromLocal8Bit(ex.what())));
        qCritical() << "[FATAL] exception from GWLGenerator:" << ex.what();
        return;
    }

    // 5) Bail out if nothing was produced
    if (gwlLines.isEmpty()) {
        showWarning(this, tr("GWL Generation"),
                    tr("No GWL lines generated. Check mapping or input data."));
        return;
    }

    // 6) Ask user where to save
    QString fileStem = experimentJson.value("experiment_code")
                           .toString().trimmed();
    if (fileStem.isEmpty()) fileStem = "experiment";
    const QString gwlPath = QFileDialog::getSaveFileName(
        this, tr("Save GWL File"), fileStem + ".gwl",
        tr("GWL files (*.gwl)"));
    if (gwlPath.isEmpty()) return;  // user cancelled

    // 7) Write to disk
    QFile f(gwlPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(this, tr("File Error"),
                  tr("Failed to write GWL file:\n%1")
                      .arg(f.errorString()));
        return;
    }
    QTextStream(&f) << gwlLines.join('\n');
    f.close();
    qDebug() << "[TRACE] GWL written to" << gwlPath;

    // 8) Generate the auxiliary files alongside
    generateExperimentAuxiliaryFiles(experimentJson,
                                     QFileInfo(gwlPath).absolutePath());

    // 9) Final success message
    showInfo(this, tr("Success"),
             tr("GWL and metadata files saved in:\n%1")
                 .arg(QFileInfo(gwlPath).absolutePath()));
}


/* =======================================================================
 * 3) generateExperimentAuxiliaryFiles()
 *      (unchanged except stdDilution now = dilutionFactor)
 * ======================================================================= */

void TecanWindow::generateExperimentAuxiliaryFiles(const QJsonObject &exp,
                                                   const QString     &outputFolder)
{
    qDebug() << "[TRACE] generateExperimentAuxiliaryFiles() to" << outputFolder;

    // --- 0) Extract and validate top‐level JSON data ---
    const QString experimentCode = exp.value("experiment_code").toString().trimmed();
    const QString projectCode    = exp.value("project_code").toString().trimmed();
    const QJsonObject standard   = exp.value("standard").toObject();
    const QJsonArray  plates     = exp.value("daughter_plates").toArray();
    const QJsonArray  testReqArr = exp.value("test_requests").toArray();
    const QJsonArray  compounds  = exp.value("compounds").toArray();

    if (experimentCode.isEmpty() || standard.isEmpty() || plates.isEmpty()) {
        showError(this, tr("GWL Generation"),
                  tr("Missing experiment code, standard, or daughter‐plate data."));
        return;
    }

    // --- 1) Read dilution_steps_unit → dilutionFactor (for GWLGenerator) ---
    double dilutionFactor = 3.16;
    if (!testReqArr.isEmpty()) {
        bool ok3 = false;
        double df = testReqArr.at(0).toObject()
                        .value("dilution_steps_unit").toString()
                        .toDouble(&ok3);
        if (ok3 && df > 0.0) dilutionFactor = df;
    }

    // --- 2) Extract testId (requested_tests) ---
    QString testId;
    if (!testReqArr.isEmpty()) {
        testId = testReqArr.at(0).toObject()
        .value("requested_tests").toString();
    }

    // --- 3) Extract stock concentration (µM) from first compound ---
    double stockConcMicroM = 0.0;
    if (!compounds.isEmpty()) {
        const auto cmp0 = compounds.at(0).toObject();
        const double sc = cmp0.value("concentration").toDouble();
        const QString cu = cmp0.value("concentration_unit").toString();
        stockConcMicroM = cu.compare("mM", Qt::CaseInsensitive) == 0
                              ? sc * 1000.0 : sc;
    }

    // --- 4) Gather all non‐standard matrix barcodes ---
    const QString stdMatrix = standard.value("Containerbarcode").toString();
    QSet<QString> allMatrices;
    for (const auto &vv : compounds) {
        const auto o = vv.toObject();
        const QString bc = o.value("container_id").toString();
        if (!bc.isEmpty() && bc != stdMatrix)
            allMatrices.insert(bc);
    }

    // --- 5) Create root experiment folder ---
    QDir rootDir(outputFolder);
    if (!rootDir.mkpath(experimentCode)) {
        showError(this, tr("File Error"),
                  tr("Could not create folder:\n%1")
                      .arg(rootDir.filePath(experimentCode)));
        return;
    }
    rootDir.cd(experimentCode);

    // --- 6) Build compound index (by product_name) ---
    QMap<QString,QJsonObject> cmpIdx;
    for (const auto &vv : compounds)
        cmpIdx[vv.toObject().value("product_name").toString()] = vv.toObject();

    QSet<QString> prevSet;

    // --- 7) Per‐daughter‐plate folders (dght_0, dght_1, …) ---
    for (int i = 0; i < plates.size(); ++i) {
        const QJsonObject plate = plates.at(i).toObject();
        const QJsonObject wells = plate.value("wells").toObject();

        // 7.a) Determine which matrices this plate uses (exclude standard)
        QSet<QString> thisSet;
        for (const QString &key : wells.keys()) {
            const QString cmpName = wells.value(key).toString();
            if (cmpIdx.contains(cmpName)) {
                const QString bc = cmpIdx[cmpName]
                                       .value("container_id").toString();
                if (bc != stdMatrix) thisSet.insert(bc);
            }
        }

        // 7.b) Make the dght_i folder and cd into it
        const QString dghtName = QString("dght_%1").arg(i);
        rootDir.mkdir(dghtName);
        rootDir.cd(dghtName);

        // 7.c) Generate one GWL per matrix in this plate
        for (const QString &matrixBC : thisSet) {
            // Build sub‐experiment JSON for this single‐matrix
            QJsonObject subExp = exp;
            subExp["daughter_plates"] = QJsonArray{ plate };

            // Filter compounds down to just this matrix
            QJsonArray subCmps;
            for (const auto &vv : compounds) {
                const auto o = vv.toObject();
                if (o.value("container_id").toString() == matrixBC)
                    subCmps.append(o);
            }
            subExp["compounds"] = subCmps;

            // Instantiate the 3-arg generator
            GWLGenerator generator(dilutionFactor,
                                   testId,
                                   stockConcMicroM);

            // Generate
            QStringList lines;
            try {
                lines = generator.generateTransferCommands(subExp);
            } catch (const std::exception &ex) {
                showError(this, tr("GWL Generation"),
                          tr("Matrix %1 error:\n%2")
                              .arg(matrixBC,
                                   QString::fromLocal8Bit(ex.what())));
                continue;
            }

            // Write <matrixBC>.gwl
            QFile gwlF(rootDir.filePath(matrixBC + ".gwl"));
            if (gwlF.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream(&gwlF) << lines.join('\n');
                gwlF.close();
            }
        }

        // 7.d) MemeMotherdghtPrecedente.txt
        {
            QFile flagF(rootDir.filePath("MemeMotherdghtPrecedente.txt"));
            if (flagF.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&flagF);
                const bool sameAsPrev = (i > 0 && thisSet == prevSet);
                out << (i == 0 || !sameAsPrev
                            ? "ChangeMatrix "
                            : "MemeMatrix ");
                flagF.close();
            }
        }
        prevSet = thisSet;
        rootDir.cdUp();
    }

    // --- 8) FichiersTXT folder ---
    rootDir.mkdir("FichiersTXT");
    rootDir.cd("FichiersTXT");
    auto writeTxt = [&](const QString &name, const QString &txt){
        QFile f(rootDir.filePath(name));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream(&f) << txt;
            f.close();
        }
    };

    writeTxt("controlListeMatrixLueTecan.txt", "");
    writeTxt("DaughterIDAamener.txt",    QString::number(plates.size()));
    writeTxt("DilutionsStep.txt",       QString::number(
                                      testReqArr.at(0)
                                          .toObject()
                                          .value("dilution_steps")
                                          .toInt()));
    writeTxt("NbDilutionsDemandee.txt", QString::number(
                                            testReqArr.at(0)
                                                .toObject()
                                                .value("number_of_dilutions")
                                                .toInt()));
    writeTxt("ExpSTD.txt",              "\"WithSTD\"");
    writeTxt("layout.txt",              "\"LAY26\"");
    writeTxt("MatrixIDAamener.txt",     QString::number(allMatrices.size()));

    // NbMatrixParDght_i.txt
    for (int i = 0; i < plates.size(); ++i) {
        const QJsonObject plate = plates.at(i).toObject();
        const QJsonObject wells = plate.value("wells").toObject();
        QSet<QString> thisSet;
        for (const QString &key : wells.keys()) {
            const QString cmpName = wells.value(key).toString();
            if (cmpIdx.contains(cmpName)) {
                const QString bc = cmpIdx[cmpName]
                                       .value("container_id").toString();
                if (bc != stdMatrix) thisSet.insert(bc);
            }
        }
        writeTxt(QString("NbMatrixParDght_%1.txt").arg(i),
                 QString::number(thisSet.size()));
    }

    writeTxt("NombreDghtAamener.txt",    QString::number(plates.size()));
    writeTxt("NombreMatrixAamener.txt",  QString::number(allMatrices.size()));
    writeTxt("projets.txt",              "\"" + projectCode + "\"");
    writeTxt("replicate.txt",            "\"NA\"");
    writeTxt("sourceQuadrant.txt",       "\"NA\"");
    writeTxt("stdDilutionStep.txt",      QString::number(
                                        testReqArr.at(0)
                                            .toObject()
                                            .value("dilution_steps")
                                            .toInt()));
    writeTxt("stdSolID.txt",             "\"" +
                                 standard.value(
                                             "invenesis_solution_ID")
                                     .toString() + "\"");
    writeTxt("stdStartConc.txt",         "\"NA\"");
    writeTxt("stdUsed.txt",              "\"" +
                                standard.value("Samplealias")
                                    .toString() + "\"");
    writeTxt("targetQuadrant.txt",       "\"NA\"");
    writeTxt("testProtocol.txt",         "\"daughterHitPick\"");
    writeTxt("volume.txt",               "50");
    writeTxt("volumeUnite.txt",          "\"ul\"");
    rootDir.cdUp();

    // --- 9) PlateMapHitLW folder & per‐matrix CSVs ---
    rootDir.mkdir("PlateMapHitLW");
    rootDir.cd("PlateMapHitLW");

    QMap<QString, QList<QJsonObject>> byMatrix;
    for (const auto &vv : compounds) {
        const auto o = vv.toObject();
        const QString bc = o.value("container_id").toString();
        if (!bc.isEmpty() && bc != stdMatrix)
            byMatrix[bc].append(o);
    }

    const QString csvHdr =
        "Containerbarcode;Samplealias;Containerposition;"
        "Volume;VolumeUnit;Concentration;ConcentrationUnit;"
        "UserdefValue1;UserdefValue2;UserdefValue3;UserdefValue4;UserdefValue5";

    for (auto it = byMatrix.constBegin(); it != byMatrix.constEnd(); ++it) {
        QFile f(rootDir.filePath(it.key() + ".csv"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) continue;
        QTextStream out(&f);
        out << csvHdr << "\n";
        for (const auto &cmp : it.value()) {
            const QString bc    = cmp.value("container_id").toString();
            const QString samp  = cmp.value("product_name").toString();
            const QString pos   = cmp.value("well_id").toString();
            const QString vol   = cmp.value("weight").toString();
            const QString vUnit = cmp.value("weight_unit").toString();
            const QString conc  = cmp.value("concentration").toString();
            const QString cUnit = cmp.value("concentration_unit").toString();
            const QString u1    = bc + "_" + pos;
            const QString u2    = cmp.value("invenesis_solution_ID")
                                   .toString();
            const QString u3    = conc;
            const QString u4    = QString::number(
                testReqArr.at(0)
                    .toObject()
                    .value("dilution_steps")
                    .toInt());
            const QString u5    = projectCode;
            out << QStringList{bc, samp, pos,
                               vol, vUnit,
                               conc, cUnit,
                               u1, u2, u3, u4, u5}
                       .join(';') << "\n";
        }
        f.close();
    }
    rootDir.cdUp();

    // --- 10) hitlist.txt (merge PlateMapHitLW CSVs, single header) ---
    {
        QFile outF(rootDir.filePath("hitlist.txt"));
        if (outF.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&outF);
            bool wroteHeader = false;
            QDir pm(rootDir.filePath("PlateMapHitLW"));
            const QStringList files = pm.entryList(
                QStringList{"*.csv"}, QDir::Files, QDir::Name);
            for (const QString &fn : files) {
                QFile inF(pm.filePath(fn));
                if (!inF.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;
                QTextStream in(&inF);
                bool firstLine = true;
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.isEmpty()) continue;
                    if (firstLine) {
                        firstLine = false;
                        if (!wroteHeader) {
                            out << line << "\n";
                            wroteHeader = true;
                        }
                    } else {
                        out << line << "\n";
                    }
                }
                inF.close();
            }
            outF.close();
        }
    }

    // --- 11) ListeMatrix.txt + empty ListeMatrixLueTecan.txt ---
    {
        QFile mF(rootDir.filePath("ListeMatrix.txt"));
        if (mF.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&mF);
            out << stdMatrix << "\n";
            for (const auto &bc : allMatrices) out << bc << "\n";
            mF.close();
        }
    }
    {
        QFile eF(rootDir.filePath("ListeMatrixLueTecan.txt"));
        eF.open(QIODevice::WriteOnly | QIODevice::Text);
        eF.close();
    }

    showInfo(this, tr("Success"),
             tr("Auxiliary files for “%1” written to:\n%2")
                 .arg(experimentCode, rootDir.absolutePath()));
}



void TecanWindow::on_actionCreate_Plate_Map_triggered()
{
    PlateMapDialog dlg(this);
    dlg.exec();
}

