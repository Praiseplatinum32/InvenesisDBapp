#include "tecanwindow.h"
#include "ui_tecanwindow.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QInputDialog>
#include <QColor>
#include <QRandomGenerator>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

#include "daughterplatewidget.h"
#include "standardselectiondialog.h"
#include "loadexperimentdialog.h"
#include "gwlgenerator.h"


// Constructor
TecanWindow::TecanWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TecanWindow),
    testRequestModel(new QSqlQueryModel(this)),
    compoundQueryModel(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    showMaximized();
    setCentralWidget(ui->splitter_3);
    setWindowTitle("Tecan Automation Interface");

    // Set models to views
    ui->testRequestTableView->setModel(testRequestModel);
    ui->compoundQueryTableView->setModel(compoundQueryModel);

    // Enable drag from compoundQueryTableView
    ui->compoundQueryTableView->setDragEnabled(true);
    ui->compoundQueryTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->compoundQueryTableView->setDragDropMode(QAbstractItemView::DragOnly);

    // Initialize MatrixPlateContainer
    matrixPlateContainer = new MatrixPlateContainer(this);
    ui->plateDisplayScrollArea->setWidget(matrixPlateContainer);

    // Initialize daughter plates container correctly
    daughterPlatesContainerWidget = new QWidget(this);
    daughterPlatesLayout = new QVBoxLayout(daughterPlatesContainerWidget);
    daughterPlatesLayout->setAlignment(Qt::AlignTop);
    daughterPlatesContainerWidget->setLayout(daughterPlatesLayout);

    ui->daughterPlateScrollArea->setWidgetResizable(true);
    ui->daughterPlateScrollArea->setWidget(daughterPlatesContainerWidget);

}


TecanWindow::~TecanWindow()
{
    delete ui;
}

void TecanWindow::loadTestRequests(const QStringList &requestIDs)
{
    if (requestIDs.isEmpty()) {
        QMessageBox::information(this, "No Selection", "No test requests selected.");
        return;
    }

    // Format query string with selected request IDs
    QString placeholders = QString("'%1'").arg(requestIDs.join("','"));
    QString queryStr = QString("SELECT * FROM test_requests WHERE request_id IN (%1)").arg(placeholders);

    // Load test requests into model
    testRequestModel->setQuery(queryStr);

    if (testRequestModel->lastError().isValid()) {
        QMessageBox::critical(this, "Query Error", testRequestModel->lastError().text());
        return;
    }

    ui->testRequestTableView->resizeColumnsToContents();

    // Initiate solutions query based on loaded requests
    querySolutionsFromTestRequests();
}

void TecanWindow::querySolutionsFromTestRequests()
{
    QSet<QString> compoundNames;

    // Extract unique compound names from testRequestModel
    for (int row = 0; row < testRequestModel->rowCount(); ++row) {
        QString compound = testRequestModel->record(row).value("compound_name").toString();
        compoundNames.insert(compound);
    }

    // Query solutions for each unique compound
    querySolutions(compoundNames);
}

void TecanWindow::querySolutions(const QSet<QString> &compoundNames)
{
    QList<int> selectedSolutionIds;
    QSqlQuery query;

    // Iterate over compounds to query solutions table
    for (const QString &compound : compoundNames) {
        query.prepare(R"(
            SELECT solution_id, product_name, invenesis_solution_id, weight, weight_unit,
                   concentration, concentration_unit, container_id, well_id, matrix_tube_id
            FROM solutions
            WHERE product_name = :compound
        )");
        query.bindValue(":compound", compound);

        if (!query.exec()) {
            QMessageBox::critical(this, "Query Error", query.lastError().text());
            continue;
        }

        QList<QVariantMap> solutionsFound;

        while (query.next()) {
            QVariantMap solution;
            QSqlRecord record = query.record();
            for (int i = 0; i < record.count(); ++i) {
                solution[record.fieldName(i)] = query.value(i);
            }
            solutionsFound.append(solution);
        }

        // Handle cases of 1 or multiple solutions
        if (solutionsFound.size() == 1) {
            selectedSolutionIds.append(solutionsFound.first()["solution_id"].toInt());
        } else if (solutionsFound.size() > 1) {
            int selectedId = resolveCompoundDuplicates(compound, solutionsFound);
            if (selectedId != -1)
                selectedSolutionIds.append(selectedId);
        } else {
            QMessageBox::warning(this, "No Solution Found",
                                 QString("No solution found for compound '%1'.").arg(compound));
        }
    }

    populateCompoundTable(selectedSolutionIds);
}

int TecanWindow::resolveCompoundDuplicates(const QString &compoundName, const QList<QVariantMap> &duplicateSolutions)
{
    QStringList items;
    QMap<QString, int> itemToIdMap;

    for(const auto &solution : duplicateSolutions) {
        QString description = QString("Solution ID: %1 | Container: %2 | Well: %3 | Conc.: %4 %5")
        .arg(solution["invenesis_solution_id"].toString())
            .arg(solution["container_id"].toString())
            .arg(solution["well_id"].toString())
            .arg(solution["concentration"].toString())
            .arg(solution["concentration_unit"].toString());

        items << description;
        itemToIdMap[description] = solution["solution_id"].toInt();
    }

    bool ok;
    QString item = QInputDialog::getItem(this,
                                         "Select Solution for " + compoundName,
                                         "Multiple solutions found. Please select one:",
                                         items, 0, false, &ok);
    if (ok && !item.isEmpty()) {
        return itemToIdMap[item];
    }

    return -1; // If the user cancels selection
}

void TecanWindow::populateCompoundTable(const QList<int> &solutionIds)
{
    if (solutionIds.isEmpty()) {
        QMessageBox::information(this, "No Solutions", "No solutions selected to display.");
        return;
    }

    QStringList idPlaceholders;
    for (int id : solutionIds)
        idPlaceholders << QString::number(id);

    QString queryStr = QString(R"(
        SELECT product_name, invenesis_solution_id, weight, weight_unit,
               concentration, concentration_unit, container_id, well_id, matrix_tube_id
        FROM solutions
        WHERE solution_id IN (%1)
    )").arg(idPlaceholders.join(","));

    compoundQueryModel->setQuery(queryStr);

    if (compoundQueryModel->lastError().isValid()) {
        QMessageBox::critical(this, "Query Error", compoundQueryModel->lastError().text());
    } else {
        ui->compoundQueryTableView->resizeColumnsToContents();
    }

    // After compoundQueryModel is filled
    QMap<QString, QSet<QString>> plateData;

    // Gather data from query results
    for(int i = 0; i < compoundQueryModel->rowCount(); ++i) {
        QString container = compoundQueryModel->record(i).value("container_id").toString();
        QString well = compoundQueryModel->record(i).value("well_id").toString();

        plateData[container].insert(well);
    }

    // Populate matrix plate visuals
    matrixPlateContainer->populatePlates(plateData);

    QStringList compounds;
    for (int i = 0; i < compoundQueryModel->rowCount(); ++i) {
        QString compoundName = compoundQueryModel->record(i).value("product_name").toString();
        if (!compounds.contains(compoundName))
            compounds << compoundName;
    }

    // Retrieve dilution steps (assuming uniform dilution for simplicity)
    int dilutionSteps = testRequestModel->record(0).value("number_of_dilutions").toInt();

    // ✅ Call populateDaughterPlates
    QString testType = testRequestModel->record(0).value("requested_tests").toString();
    populateDaughterPlates(dilutionSteps, compounds, testType);


}


void TecanWindow::populateDaughterPlates(int dilutionSteps, const QStringList& compoundList, const QString& testType)
{
    // Clear existing plates
    QLayoutItem* child;
    while ((child = daughterPlatesLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    const QStringList rows = {"A","B","C","D","E","F","G","H"};
    const int maxCols = 12;
    QList<QMap<QString, QStringList>> plates;
    plates.append(QMap<QString, QStringList>());
    int currentPlate = 0;

    bool specialTest = testType.contains("INV-T-031");

    int standardColumn = specialTest ? 11 : -1;
    int dmsoColumn = specialTest ? 12 : -1;

    QStringList standardWells, dmsoWells;

    // === 1. Define STANDARD and DMSO wells ===
    if (specialTest) {
        // Special layout: Standard in col 11, DMSO in col 12 (vertical)
        for (const QString& row : rows) {
            standardWells << row + QString::number(standardColumn);
            dmsoWells << row + QString::number(dmsoColumn);
        }
    } else {
        // Regular layout: Standard horizontal on Row A, DMSO horizontal on Row H
        int standardDilution = qMax(dilutionSteps, 6);  // Standard is minimum 6 dilutions
        for (int col = 1; col <= standardDilution; ++col)
            standardWells << "A" + QString::number(col);

        for (int col = 1; col <= maxCols; ++col)
            dmsoWells << "H" + QString::number(col);  // DMSO now fills entire H row
    }

    plates[currentPlate]["Standard"] = standardWells;
    plates[currentPlate]["DMSO"] = dmsoWells;

    // === 2. Place Compounds ===
    int currentCol = 1;
    int currentRow = (specialTest ? 0 : 1); // skip A for regular tests

    for (const QString& compound : compoundList) {
        // Check plate limits
        while (true) {
            int lastCol = currentCol + dilutionSteps - 1;

            bool overlapSpecialCols = specialTest && lastCol >= standardColumn;
            bool exceedPlateCols = lastCol > (specialTest ? standardColumn - 1 : maxCols);

            if (overlapSpecialCols || exceedPlateCols) {
                currentRow++;
                currentCol = 1;
            } else {
                break;
            }

            if (currentRow >= rows.size() - (specialTest ? 0 : 1)) {
                // New plate
                plates.append(QMap<QString, QStringList>());
                currentPlate++;
                plates[currentPlate]["Standard"] = standardWells;
                plates[currentPlate]["DMSO"] = dmsoWells;
                currentRow = (specialTest ? 0 : 1);
                currentCol = 1;
            }
        }

        QStringList assignedWells;
        for (int d = 0; d < dilutionSteps; ++d)
            assignedWells << rows[currentRow] + QString::number(currentCol + d);

        plates[currentPlate][compound] = assignedWells;

        currentRow++;
        if (currentRow >= rows.size() - (specialTest ? 0 : 1)) {
            currentRow = (specialTest ? 0 : 1);
            currentCol += dilutionSteps;

            if (specialTest && currentCol >= standardColumn)
                currentCol = 1;

            if (currentCol > (specialTest ? standardColumn - 1 : maxCols)) {
                plates.append(QMap<QString, QStringList>());
                currentPlate++;
                plates[currentPlate]["Standard"] = standardWells;
                plates[currentPlate]["DMSO"] = dmsoWells;
                currentRow = (specialTest ? 0 : 1);
                currentCol = 1;
            }
        }
    }

    // === 3. Render Plates ===
    for (int i = 0; i <= currentPlate; ++i) {
        auto plateWidget = new DaughterPlateWidget(i + 1, this);

        QMap<QString, QColor> compoundColors;
        int hueStep = 360 / (plates[i].keys().size() + 1);
        int hue = 0;
        const QMap<QString, QStringList>& plateMap = plates[i];
        for (auto it = plateMap.constBegin(); it != plateMap.constEnd(); ++it) {
            const QString& cmpd = it.key();
            if (cmpd == "DMSO")
                compoundColors[cmpd] = Qt::gray;
            else if (cmpd == "Standard")
                compoundColors[cmpd] = QColor(0, 122, 204);
            else {
                compoundColors[cmpd] = QColor::fromHsv(hue % 360, 200, 220);
                hue += hueStep;
            }
        }

        plateWidget->populatePlate(plates[i], compoundColors, dilutionSteps);
        plateWidget->enableCompoundDragDrop(dilutionSteps);
        daughterPlatesLayout->addWidget(plateWidget);
    }
}


void TecanWindow::on_clearPlatesButton_clicked()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Clear Plates",
                                  "Are you sure you want to clear all compound placements?\n"
                                  "(Standard and DMSO will be preserved)",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        for (int i = 0; i < daughterPlatesLayout->count(); ++i) {
            auto plateWidget = qobject_cast<DaughterPlateWidget *>(daughterPlatesLayout->itemAt(i)->widget());
            if (plateWidget) {
                plateWidget->clearCompounds();
                plateWidget->setAcceptDrops(true);  // ⬅️ make extra sure
            }
        }
    }
}


void TecanWindow::on_actionSave_triggered()
{
    // Prompt user for experiment code
    bool ok;
    QString experimentCode = QInputDialog::getText(this, "Save Experiment", "Enter experiment code:", QLineEdit::Normal, "", &ok);
    if (!ok || experimentCode.isEmpty()) return;

    // Prompt for username
    QString username = QInputDialog::getText(this, "User", "Enter your name:", QLineEdit::Normal, "", &ok);
    if (!ok || username.isEmpty()) return;

    // Prompt for standard
    // 🧪 Prompt for standard compound
    StandardSelectionDialog standardDialog(this);
    if (standardDialog.exec() != QDialog::Accepted) {
        QMessageBox::information(this, "Cancelled", "Save aborted.");
        return;
    }

    QJsonObject selectedStandardJson = standardDialog.selectedStandardJson();
    if (selectedStandardJson.isEmpty()) {
        QMessageBox::warning(this, "Invalid Standard", "No valid standard selected.");
        return;
    }

    // ===== Collect Test Requests =====
    QJsonArray testRequestsArray;
    for (int i = 0; i < testRequestModel->rowCount(); ++i) {
        QJsonObject requestObj;
        QSqlRecord record = testRequestModel->record(i);
        for (int j = 0; j < record.count(); ++j) {
            requestObj[record.fieldName(j)] = QJsonValue::fromVariant(record.value(j));
        }
        testRequestsArray.append(requestObj);
    }

    // ===== Collect Compounds from Solutions =====
    QJsonArray compoundsArray;
    for (int i = 0; i < compoundQueryModel->rowCount(); ++i) {
        QJsonObject compoundObj;
        QSqlRecord record = compoundQueryModel->record(i);
        for (int j = 0; j < record.count(); ++j) {
            compoundObj[record.fieldName(j)] = QJsonValue::fromVariant(record.value(j));
        }
        compoundsArray.append(compoundObj);
    }

    // ===== Collect Matrix Plate Data =====
    QMap<QString, QSet<QString>> plateMap = matrixPlateContainer->getPlateMap();
    QJsonObject matrixPlatesJson;
    for (auto it = plateMap.begin(); it != plateMap.end(); ++it) {
        QJsonArray wells;
        foreach(const QString& well, it.value()) {
            wells.append(well);
        }
        matrixPlatesJson[it.key()] = wells;
    }

    // ===== Collect Daughter Plate Data =====
    QJsonArray daughterPlatesJson;
    int dilutionSteps = testRequestModel->record(0).value("number_of_dilutions").toInt();  // Extract once

    for (int i = 0; i < daughterPlatesLayout->count(); ++i) {
        auto plateWidget = qobject_cast<DaughterPlateWidget *>(daughterPlatesLayout->itemAt(i)->widget());
        if (!plateWidget) continue;

        QJsonObject plateObj;
        plateObj["plate_number"] = i + 1;
        plateObj["dilution_steps"] = dilutionSteps;               // ✅ add this
        plateObj["wells"] = plateWidget->toJson();
        daughterPlatesJson.append(plateObj);
    }


    // ===== Assemble Master JSON =====
    QJsonObject experimentJson;
    experimentJson["experiment_code"] = experimentCode;
    experimentJson["project_code"] = testRequestModel->record(0).value("project_code").toString();
    experimentJson["user"] = username;
    experimentJson["standard"] = selectedStandardJson;
    experimentJson["test_requests"] = testRequestsArray;
    experimentJson["compounds"] = compoundsArray;
    experimentJson["matrix_plates"] = matrixPlatesJson;
    experimentJson["daughter_plates"] = daughterPlatesJson;

    qDebug() << QJsonDocument(experimentJson).toJson(QJsonDocument::Indented);

    // ===== Insert into experiments table =====
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO experiments (experiment_code, project_code, date_created, data)
        VALUES (:code, :project, NOW(), :data)
        RETURNING experiment_id
    )");
    query.bindValue(":code", experimentCode);
    query.bindValue(":project", testRequestModel->record(0).value("project_code").toString());
    query.bindValue(":data", QString(QJsonDocument(experimentJson).toJson(QJsonDocument::Compact)));

    if (!query.exec() || !query.next()) {
        QMessageBox::critical(this, "Error", "Failed to insert experiment:\n" + query.lastError().text());
        return;
    }

    int experimentId = query.value(0).toInt();

    // ===== Link experiment to test requests =====
    for (int row = 0; row < testRequestModel->rowCount(); ++row) {
        QString requestId = testRequestModel->record(row).value("request_id").toString();
        QSqlQuery linkQuery;
        linkQuery.prepare("INSERT INTO experiment_requests (experiment_id, request_id) VALUES (:eid, :rid)");
        linkQuery.bindValue(":eid", experimentId);
        linkQuery.bindValue(":rid", requestId);
        if (!linkQuery.exec()) {
            QMessageBox::warning(this, "Warning", "Failed to link request ID: " + requestId);
        }
    }

    QMessageBox::information(this, "Success", "Experiment saved successfully!");

    lastSavedExperimentJson = experimentJson;

}


void TecanWindow::on_actionLoad_triggered()
{
    LoadExperimentDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    int selectedExperimentId = dialog.selectedExperimentId();
    bool readOnly = dialog.isReadOnly();

    if (selectedExperimentId == -1) {
        QMessageBox::warning(this, "No Selection", "Please select an experiment to load.");
        return;
    }

    QSqlQuery query;
    query.prepare("SELECT experiment_code, project_code, data FROM experiments WHERE experiment_id = :id");
    query.bindValue(":id", selectedExperimentId);

    if (!query.exec() || !query.next()) {
        QMessageBox::critical(this, "Error", "Failed to load experiment:\n" + query.lastError().text());
        return;
    }

    QString experimentCode = query.value("experiment_code").toString();
    QString projectCode = query.value("project_code").toString();
    QString jsonData = query.value("data").toString();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::critical(this, "Error", "Invalid JSON format in experiment.");
        return;
    }

    QJsonObject root = doc.object();
    lastSavedExperimentJson = root;

    QJsonObject standardObj = root["standard"].toObject();  // ⬅️ New full standard object
    QString standardLabel = QString("%1 – Well: %2 – %3 %4 – Barcode: %5")
                                .arg(standardObj["Samplealias"].toString())
                                .arg(standardObj["Containerposition"].toString())
                                .arg(standardObj["Concentration"].toString())
                                .arg(standardObj["ConcentrationUnit"].toString())
                                .arg(standardObj["Containerbarcode"].toString());

    QString standardNotes = QJsonDocument(standardObj).toJson(QJsonDocument::Indented);

    qDebug() << "Loaded Standard:" << root["standard_name"].toString();

    // restoration steps
    loadTestRequestsFromJson(root["test_requests"].toArray());
    loadCompoundsFromJson(root["compounds"].toArray());
    loadMatrixPlatesFromJson(root["matrix_plates"].toObject());
    loadDaughterPlatesFromJson(root["daughter_plates"].toArray(), readOnly);

    QMessageBox::information(this, "Experiment Loaded", QString("Experiment '%1' loaded successfully.").arg(experimentCode));
}


//Load test request
void TecanWindow::loadTestRequestsFromJson(const QJsonArray& array)
{
    if (array.isEmpty()) return;

    auto model = new QStandardItemModel(this);

    QStringList headers = {
        "request_id",
        "project_code",
        "requested_tests",
        "compound_name",
        "starting_concentration",
        "starting_concentration_unit",
        "dilution_steps",
        "dilution_steps_unit",
        "number_of_replicate",
        "stock_concentration",
        "stock_concentration_unit",
        "concentration_to_be_tested",
        "additional_notes"
    };

    model->setHorizontalHeaderLabels(headers);

    foreach(const QJsonValue& value, array) {
        QJsonObject obj = value.toObject();
        QList<QStandardItem*> rowItems;
        foreach(const QString& key, headers) {
            rowItems << new QStandardItem(obj.value(key).toVariant().toString());
        }
        model->appendRow(rowItems);
    }

    testRequestModel->setQuery(QSqlQuery());  // Clear QSqlQueryModel
    ui->testRequestTableView->setModel(model);
}


//Load compounds
void TecanWindow::loadCompoundsFromJson(const QJsonArray& array)
{
    if (array.isEmpty()) return;

    auto model = new QStandardItemModel(this);

    // 🔁 Define the correct column order explicitly
    QStringList headers = {
        "product_name",
        "invenesis_solution_id",
        "weight",
        "weight_unit",
        "concentration",
        "concentration_unit",
        "container_id",
        "well_id",
        "matrix_tube_id"
    };

    model->setHorizontalHeaderLabels(headers);

    foreach(const QJsonValue& value, array) {
        QJsonObject obj = value.toObject();
        QList<QStandardItem*> rowItems;
        foreach(const QString& key, headers) {
            rowItems << new QStandardItem(obj.value(key).toVariant().toString());
        }
        model->appendRow(rowItems);
    }

    compoundQueryModel->setQuery(QSqlQuery());  // Clear QSqlQueryModel
    ui->compoundQueryTableView->setModel(model);
}


//Load matrix plates
void TecanWindow::loadMatrixPlatesFromJson(const QJsonObject& obj)
{
    QMap<QString, QSet<QString>> plateMap;
    foreach(const QString& containerId, obj.keys()) {
        QJsonArray wells = obj[containerId].toArray();
        QSet<QString> wellSet;
        foreach(const QJsonValue& w, wells)
            wellSet.insert(w.toString());
        plateMap[containerId] = wellSet;
    }

    matrixPlateContainer->populatePlates(plateMap);
}

void TecanWindow::loadDaughterPlatesFromJson(const QJsonArray& array, bool readOnly)
{
    // Clear existing layout
    QLayoutItem* child;
    while ((child = daughterPlatesLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // Attempt to extract standard info from saved JSON
    QString standardLabel;
    QString standardNotes;
    bool hasValidStandard = false;

    if (lastSavedExperimentJson.contains("standard") && lastSavedExperimentJson["standard"].isObject()) {
        QJsonObject standardObj = lastSavedExperimentJson["standard"].toObject();

        standardLabel = QString("%1 – Well: %2 – %3 %4 – Barcode: %5")
                            .arg(standardObj["Samplealias"].toString())
                            .arg(standardObj["Containerposition"].toString())
                            .arg(standardObj["Concentration"].toString())
                            .arg(standardObj["ConcentrationUnit"].toString())
                            .arg(standardObj["Containerbarcode"].toString());

        standardNotes = QJsonDocument(standardObj).toJson(QJsonDocument::Indented);
        hasValidStandard = true;
    } else {
        qWarning() << "Missing or invalid 'standard' object in experiment JSON.";
    }

    // Recreate each daughter plate
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject plateObj = array[i].toObject();
        int dilutionSteps = plateObj.contains("dilution_steps") ? plateObj["dilution_steps"].toInt() : 3;

        auto* plate = new DaughterPlateWidget(i + 1, this);
        plate->fromJson(plateObj["wells"].toObject(), dilutionSteps);

        if (!readOnly) {
            plate->enableCompoundDragDrop(dilutionSteps);
        }

        if (hasValidStandard) {
            plate->setStandardInfo(standardLabel, standardNotes);
        }

        daughterPlatesLayout->addWidget(plate);
    }
}


QJsonObject TecanWindow::buildCurrentExperimentJson(const QString& experimentCode, const QString& username)
{
    // Step 1: Gather all data like in the save function
    QJsonObject experimentJson;

    experimentJson["experiment_code"] = experimentCode;
    experimentJson["project_code"] = testRequestModel->record(0).value("project_code").toString();
    experimentJson["user"] = username;

    QJsonArray testRequestsArray;
    for (int i = 0; i < testRequestModel->rowCount(); ++i) {
        QJsonObject obj;
        QSqlRecord record = testRequestModel->record(i);
        for (int j = 0; j < record.count(); ++j) {
            obj[record.fieldName(j)] = QJsonValue::fromVariant(record.value(j));
        }
        testRequestsArray.append(obj);
    }
    experimentJson["test_requests"] = testRequestsArray;

    QJsonArray compoundsArray;
    for (int i = 0; i < compoundQueryModel->rowCount(); ++i) {
        QJsonObject obj;
        QSqlRecord record = compoundQueryModel->record(i);
        for (int j = 0; j < record.count(); ++j) {
            obj[record.fieldName(j)] = QJsonValue::fromVariant(record.value(j));
        }
        compoundsArray.append(obj);
    }
    experimentJson["compounds"] = compoundsArray;

    QMap<QString, QSet<QString>> plateMap = matrixPlateContainer->getPlateMap();
    QJsonObject matrixJson;
    for (auto it = plateMap.begin(); it != plateMap.end(); ++it) {
        QJsonArray wells;
        for (const QString& well : it.value()) {
            wells.append(well);
        }
        matrixJson[it.key()] = wells;
    }
    experimentJson["matrix_plates"] = matrixJson;

    QJsonArray daughterPlatesArray;
    int dilutionSteps = testRequestModel->record(0).value("number_of_dilutions").toInt();
    for (int i = 0; i < daughterPlatesLayout->count(); ++i) {
        auto plateWidget = qobject_cast<DaughterPlateWidget *>(daughterPlatesLayout->itemAt(i)->widget());
        if (!plateWidget) continue;
        QJsonObject plate;
        plate["plate_number"] = i + 1;
        plate["dilution_steps"] = dilutionSteps;
        plate["wells"] = plateWidget->toJson();
        daughterPlatesArray.append(plate);
    }
    experimentJson["daughter_plates"] = daughterPlatesArray;

    return experimentJson;
}

void TecanWindow::on_actionGenerate_GWL_triggered()
{
    // Check if we have a saved version
    if (lastSavedExperimentJson.isEmpty()) {
        QMessageBox::warning(this, "Not Saved", "You must save the experiment before generating a GWL file.");
        on_actionSave_triggered();  // Trigger save first
        return;
    }

    // Rebuild current state
    QString code = lastSavedExperimentJson.value("experiment_code").toString();
    QString user = lastSavedExperimentJson.value("user").toString();  // fallback
    QJsonObject current = buildCurrentExperimentJson(code, user);

    // Compare compact JSON strings
    QString saved = QJsonDocument(lastSavedExperimentJson).toJson(QJsonDocument::Compact);
    QString now = QJsonDocument(current).toJson(QJsonDocument::Compact);

    if (saved != now) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Experiment Modified",
            "Changes have been made since last save. Do you want to overwrite the saved experiment?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel)
            return;

        if (reply == QMessageBox::Yes) {
            on_actionSave_triggered();  // Save the latest version
            return;  // Wait for next GWL trigger to avoid duplication
        }

        // If No: Continue using the saved version (not recommended, but allowed)
    }

    // At this point: safe to generate GWL
    generateGWLFromJson(lastSavedExperimentJson);
}

void TecanWindow::generateGWLFromJson(const QJsonObject& experimentJson)
{
    GWLGenerator generator;
    QStringList lines = generator.generateTransferCommands(experimentJson);

    if (lines.isEmpty()) {
        QMessageBox::warning(this, "GWL Generation", "No GWL lines generated. Check mapping or input data.");
        return;
    }

    QString filename = experimentJson["experiment_code"].toString().trimmed();
    if (filename.isEmpty()) filename = "experiment";

    QString gwlPath = QFileDialog::getSaveFileName(this, "Save GWL File", filename + ".gwl", "GWL Files (*.gwl)");
    if (gwlPath.isEmpty())
        return;

    QFile file(gwlPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "File Error", "Failed to write GWL file:\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
    out << lines.join('\n');
    file.close();

    // ✅ Generate metadata files in the same directory as the GWL
    QFileInfo info(gwlPath);
    QString experimentFolder = info.absolutePath();
    generateExperimentAuxiliaryFiles(experimentJson, experimentFolder);

    QMessageBox::information(this, "Success", QString("GWL and metadata files saved in:\n%1").arg(experimentFolder));
}




void TecanWindow::generateExperimentAuxiliaryFiles(const QJsonObject& experimentJson, const QString& outputFolder) {
    QDir dir(outputFolder);
    if (!dir.exists()) {
        QDir().mkpath(outputFolder);
    }

    auto writeFile = [&](const QString& filename, const QString& content) {
        QFile file(dir.filePath(filename));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << content.trimmed();
            file.close();
        } else {
            qWarning() << "Failed to write file:" << filename << file.errorString();
        }
    };

    const QJsonObject standard = experimentJson["standard"].toObject();
    const QJsonArray testRequests = experimentJson["test_requests"].toArray();

    QString testProtocol = testRequests.first().toObject()["requested_tests"].toString();
    QString stdSolId = standard["invenesis_solution_ID"].toString();
    QString stdUsed = standard["Samplealias"].toString();
    QString stdDilution = "3.16";  // You may make this configurable
    QString experimentCode = experimentJson["experiment_code"].toString();

    writeFile("volume.txt", "50");
    writeFile("testProtocol.txt", testProtocol);
    writeFile("stdDilutionStep.txt", stdDilution);
    writeFile("stdSolID.txt", stdSolId);
    writeFile("stdUsed.txt", stdUsed);
    writeFile("sourceQuadrant.txt", "NA");
    writeFile("targetQuadrant.txt", "NA");
    writeFile("layout.txt", "LAY18");
    writeFile("replicate.txt", "1");
    writeFile("projets.txt", experimentJson["project_code"].toString());
    writeFile("NombreMatrixAamener.txt", "01");
    writeFile("NbMatrixParDght_0.txt", "0");
    writeFile("NombreDghtAamener.txt", "1");
    writeFile("MatrixBarcodeIDActuel.txt", standard["Containerbarcode"].toString());

    // ✅ Create ExpEnCours.txt (outside the experiment folder, in fixed location)
    QString expEnCoursPath = R"(C:\ProgramData\Tecan\EVOware\database\scripts\INVUseByTecan\txtFiles\ExpEnCours.txt)";
    QFile expFile(expEnCoursPath);
    if (expFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&expFile);
        out << experimentCode << "\n";
        expFile.close();
    } else {
        qWarning() << "Failed to write ExpEnCours.txt:" << expFile.errorString();
    }
}



