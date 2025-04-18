#include "tecanwindow.h"
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

    auto *model = new QStandardItemModel(this);
    const QStringList headers = {
        "request_id", "project_code", "requested_tests", "compound_name",
        "starting_concentration", "starting_concentration_unit",
        "dilution_steps", "dilution_steps_unit", "number_of_replicate",
        "stock_concentration", "stock_concentration_unit",
        "concentration_to_be_tested", "additional_notes"};
    model->setHorizontalHeaderLabels(headers);

    for (const QJsonValue &val : array) {
        const QJsonObject obj = val.toObject();
        QList<QStandardItem*> row;
        for (const QString &key : headers)
            row << new QStandardItem(obj.value(key).toVariant().toString());
        model->appendRow(row);
    }

    testRequestModel->setQuery(QSqlQuery());
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
 * 1)  on_actionGenerate_GWL_triggered()
 * ======================================================================= */
void TecanWindow::on_actionGenerate_GWL_triggered()
{
    qDebug() << "[TRACE] GWL button pressed";

    /* ---------------------------------------------------------- */
    /* 0) Make sure we have at least one saved snapshot           */
    /* ---------------------------------------------------------- */
    if (lastSavedExperimentJson.isEmpty()) {
        showWarning(this, tr("Not Saved"),
                    tr("You must save the experiment before generating a GWL file."));
        on_actionSave_triggered();            // user does an explicit save
        return;
    }

    /* ---------------------------------------------------------- */
    /* 1) Build JSON from the *current* UI state                  */
    /* ---------------------------------------------------------- */
    const QString code = lastSavedExperimentJson["experiment_code"].toString();
    const QString user = lastSavedExperimentJson["user"].toString();
    const QJsonObject currentJson = buildCurrentExperimentJson(code, user);

    /* ---------------------------------------------------------- */
    /* 2) Compare with last snapshot using order‑insensitive eq.  */
    /* ---------------------------------------------------------- */
    if (!jsonEqual(lastSavedExperimentJson, currentJson)) {
        const QMessageBox::StandardButton choice = QMessageBox::question(
            this, tr("Experiment Modified"),
            tr("Changes have been made since last save.\n"
               "Do you want to overwrite the saved experiment?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (choice == QMessageBox::Cancel)
            return;

        if (choice == QMessageBox::Yes) {         // overwrite snapshot, then stop
            on_actionSave_triggered();
            return;                               // user will press Generate again
        }

        /* choice == No → continue with unsaved edits that are on screen */
        generateGWLFromJson(currentJson);
        return;
    }

    generateGWLFromJson(lastSavedExperimentJson);
}

/* =======================================================================
 * 2)  generateGWLFromJson()
 * ======================================================================= */
void TecanWindow::generateGWLFromJson(const QJsonObject &experimentJson)
{
    qDebug() << "[TRACE] generateGWLFromJson()";

    QStringList gwlLines;
    try {
        GWLGenerator generator;
        gwlLines = generator.generateTransferCommands(experimentJson);
        qDebug() << "[TRACE] generator produced" << gwlLines.size() << "lines";
    } catch (const std::exception &ex) {
        showError(this, tr("GWL Generation"),
                  tr("Generator error: %1").arg(QString::fromLocal8Bit(ex.what())));
        qCritical() << "[FATAL] exception from GWLGenerator:" << ex.what();
        return;
    }

    if (gwlLines.isEmpty()) {
        showWarning(this, tr("GWL Generation"),
                    tr("No GWL lines generated. Check mapping or input data."));
        qWarning() << "[WARN] generator returned empty list";
        return;
    }

    QString filename = experimentJson.value("experiment_code").toString().trimmed();
    if (filename.isEmpty()) filename = QStringLiteral("experiment");

    const QString gwlPath = QFileDialog::getSaveFileName(
        this, tr("Save GWL File"), filename + ".gwl",
        tr("GWL files (*.gwl)"));
    if (gwlPath.isEmpty()) { qDebug() << "[TRACE] user canceled file dialog"; return; }

    QFile gwlFile(gwlPath);
    if (!gwlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(this, tr("File Error"),
                  tr("Failed to write GWL file:\n%1").arg(gwlFile.errorString()));
        qCritical() << "[FATAL] cannot open" << gwlPath << ":" << gwlFile.errorString();
        return;
    }
    QTextStream(&gwlFile) << gwlLines.join(QLatin1Char('\n'));
    gwlFile.close();
    qDebug() << "[TRACE] GWL written to" << gwlPath;

    const QString outFolder = QFileInfo(gwlPath).absolutePath();
    generateExperimentAuxiliaryFiles(experimentJson, outFolder);

    showInfo(this, tr("Success"),
             tr("GWL and metadata files saved in:\n%1").arg(outFolder));
}

/* =======================================================================
 * 3)  generateExperimentAuxiliaryFiles()
 * ======================================================================= */
void TecanWindow::generateExperimentAuxiliaryFiles(const QJsonObject &experimentJson,
                                                   const QString     &outputFolder)
{
    qDebug() << "[TRACE] generateExperimentAuxiliaryFiles() to" << outputFolder;

    const QJsonObject standard   = experimentJson.value("standard").toObject();
    const QJsonArray  testReqArr = experimentJson.value("test_requests").toArray();

    if (standard.isEmpty()) {
        showError(this, tr("GWL Generation"),
                  tr("Missing standard information; cannot write metadata files."));
        qCritical() << "[FATAL] standard object missing";
        return;
    }
    if (testReqArr.isEmpty()) {
        showError(this, tr("GWL Generation"),
                  tr("No test‑request data present; cannot write metadata files."));
        qCritical() << "[FATAL] test_requests array empty";
        return;
    }

    QDir dir(outputFolder);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        showError(this, tr("File Error"),
                  tr("Unable to create output folder:\n%1").arg(dir.absolutePath()));
        qCritical() << "[FATAL] cannot create directory" << dir.absolutePath();
        return;
    }

    auto writeFile = [&](const QString &name, const QString &content) {
        QFile f(dir.filePath(name));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream(&f) << content.trimmed();
            qDebug() << "[TRACE] wrote" << name;
        } else {
            qWarning().noquote() << "Failed to write" << name << ':' << f.errorString();
        }
    };

    const QString testProtocol =
        testReqArr.at(0).toObject().value("requested_tests").toString();
    const QString stdSolId     = standard.value("invenesis_solution_ID").toString();
    const QString stdUsed      = standard.value("Samplealias").toString();
    const QString stdDilution  = QStringLiteral("3.16");   // TODO configurable

    /* text files */
    writeFile("volume.txt",               "50");
    writeFile("testProtocol.txt",         testProtocol);
    writeFile("stdDilutionStep.txt",      stdDilution);
    writeFile("stdSolID.txt",             stdSolId);
    writeFile("stdUsed.txt",              stdUsed);
    writeFile("sourceQuadrant.txt",       "NA");
    writeFile("targetQuadrant.txt",       "NA");
    writeFile("layout.txt",               "LAY18");
    writeFile("replicate.txt",            "1");
    writeFile("projets.txt",              experimentJson.value("project_code").toString());
    writeFile("NombreMatrixAamener.txt",  "01");
    writeFile("NbMatrixParDght_0.txt",    "0");
    writeFile("NombreDghtAamener.txt",    "1");
    writeFile("MatrixBarcodeIDActuel.txt",
              standard.value("Containerbarcode").toString());

    /* ExpEnCours.txt */
    const QString expEnCoursPath =
        R"(C:\ProgramData\Tecan\EVOware\database\scripts\INVUseByTecan\txtFiles\ExpEnCours.txt)";
    QFileInfo expInfo(expEnCoursPath);
    QDir expDir = expInfo.dir();
    if (!expDir.exists() && !expDir.mkpath(QStringLiteral("."))) {
        qWarning() << "Cannot create directory for ExpEnCours.txt:" << expDir.path();
        return;
    }
    QFile expFile(expEnCoursPath);
    if (expFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&expFile) << experimentJson.value("experiment_code").toString() << '\n';
        qDebug() << "[TRACE] wrote ExpEnCours.txt";
    } else {
        qWarning() << "Failed to write ExpEnCours.txt:" << expFile.errorString();
    }
}

