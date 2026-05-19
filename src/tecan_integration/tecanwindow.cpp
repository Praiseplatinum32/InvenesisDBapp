#include "tecanwindow.h"
#include "generategwldialog.h"
#include "plate_management/PlateMapDialog.h"
#include "ui_tecanwindow.h"

// Qt
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardItemModel>
#include <QTextStream>
#include <QVariant>
#include <algorithm> // std::sort

// Project
#include "gwlgenerator.h"
#include "services/ExperimentManager.h"
#include "services/DilutionEngine.h"
#include "plate_management/daughterplatewidget.h"
#include "standardselectiondialog.h"
#include "ui/loadexperimentdialog.h"
#include "services/ExperimentJsonSerializer.h"

#include "services/JsonHelpers.h"

using SqlModelUPtr = std::unique_ptr<QSqlQueryModel>;

namespace /* ====== file-local helpers / constants ====== */
{
static constexpr int kMaxColumns = 12;
static const QStringList kPlateRows = {"A", "B", "C", "D", "E", "F", "G", "H"};
} // unnamed namespace

/* ======= static QMessageBox wrappers (header declared) ======= */
void TecanWindow::showInfo(QWidget *parent, const QString &title,
                           const QString &msg) {
  QMessageBox::information(parent, title, msg);
}

void TecanWindow::showWarning(QWidget *parent, const QString &title,
                              const QString &msg) {
  QMessageBox::warning(parent, title, msg);
}

void TecanWindow::showError(QWidget *parent, const QString &title,
                            const QString &msg) {
  QMessageBox::critical(parent, title, msg);
}

/* ========================================================================== */
/*                              ctor / dtor                                   */
/* ========================================================================== */
TecanWindow::TecanWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::TecanWindow),
      m_viewModel(std::make_unique<TecanViewModel>(this)) {
  ui->setupUi(this);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  
  QAction *backAction = new QAction(QIcon(":/icons/resources/icons/cacher.png"), tr("Back to Database"), this);
  ui->toolBar->insertAction(ui->actionSave, backAction);
  connect(backAction, &QAction::triggered, this, &TecanWindow::backRequested);
  
  mainLayout->addWidget(ui->toolBar);
  mainLayout->addWidget(ui->splitter_3);

  
  

  ui->compoundQueryTableView->setDragEnabled(true);
  ui->compoundQueryTableView->setSelectionBehavior(
      QAbstractItemView::SelectRows);
  ui->compoundQueryTableView->setDragDropMode(QAbstractItemView::DragOnly);

  /* --- plate visual containers --- */
  matrixPlateContainer = new MatrixPlateContainer(this);
  ui->plateDisplayScrollArea->setWidget(matrixPlateContainer);

  // Daughter Plates Tab
  daughterPlatesContainerWidget = new QWidget(this);
  daughterPlatesLayout = new QVBoxLayout(daughterPlatesContainerWidget);
  daughterPlatesLayout->setAlignment(Qt::AlignTop);
  QScrollArea *saDaughter = new QScrollArea(this);
  saDaughter->setWidgetResizable(true);
  saDaughter->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  saDaughter->setWidget(daughterPlatesContainerWidget);
  ui->platesTabWidget->addTab(saDaughter, tr("Daughter Plates"));

  // Test Plates Tab
  testPlatesContainerWidget = new QWidget(this);
  testPlatesLayout = new QVBoxLayout(testPlatesContainerWidget);
  testPlatesLayout->setAlignment(Qt::AlignTop);
  QScrollArea *saTest = new QScrollArea(this);
  saTest->setWidgetResizable(true);
  saTest->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  saTest->setWidget(testPlatesContainerWidget);
  ui->platesTabWidget->addTab(saTest, tr("Test Plates"));

  // QC Plates Tab
  QWidget *qcTabWidget = new QWidget(this);
  QVBoxLayout *qcTabLayout = new QVBoxLayout(qcTabWidget);
  qcSelectionCombo = new QComboBox(this);
  qcTabLayout->addWidget(qcSelectionCombo);
  connect(qcSelectionCombo, &QComboBox::currentTextChanged, this,
          &TecanWindow::onQcSelectionChanged);

  qcPlatesContainerWidget = new QWidget(this);
  qcPlatesLayout = new QVBoxLayout(qcPlatesContainerWidget);
  qcPlatesLayout->setAlignment(Qt::AlignTop);
  QScrollArea *saQC = new QScrollArea(this);
  saQC->setWidgetResizable(true);
  saQC->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  saQC->setWidget(qcPlatesContainerWidget);
  qcTabLayout->addWidget(saQC);
  ui->platesTabWidget->addTab(qcTabWidget, tr("QC Plates"));

  loadQcPlatesConfig();

  // Ensure default plate type and button sync
  m_viewModel->setDaughterPlateType(ExperimentJsonSerializer::DaughterPlateType::Plate96);
  if (ui->switchPlate)
    ui->switchPlate->setChecked(false);

  // Balance the 4 panels in the UI to be centered
  ui->splitter_3->setSizes({1000, 1000});
  ui->splitter->setSizes({1000, 1000});
  ui->splitter_1->setSizes({1000, 1000});
}

TecanWindow::~TecanWindow() = default;

/* ========================================================================== */
/*                         test-request / solution logic                      */
/* ========================================================================== */
void TecanWindow::loadTestRequests(const QStringList &requestIDs) {
  if (requestIDs.isEmpty()) {
    showInfo(this, tr("No Selection"), tr("No test requests selected."));
    return;
  }

  QString err;
  QList<QVariantMap> data = m_viewModel->getExperimentManager()->fetchTestRequests(requestIDs, &err);
  if (!err.isEmpty()) {
    showError(this, tr("Query Error"), err);
    return;
  }

  if (testRequestModel) delete testRequestModel;
  testRequestModel = m_viewModel->getExperimentManager()->createTestRequestModel(data, this);
  ui->testRequestTableView->setModel(testRequestModel);
  ui->testRequestTableView->resizeColumnsToContents();
  
  querySolutionsFromTestRequests();
}

void TecanWindow::querySolutionsFromTestRequests() {
  QSet<QString> compoundNames;
  int compoundCol = -1;
  for (int c = 0; c < testRequestModel->columnCount(); ++c) {
      if (testRequestModel->headerData(c, Qt::Horizontal).toString() == "compound_name") {
          compoundCol = c;
          break;
      }
  }
  if (compoundCol != -1) {
      for (int row = 0, rows = testRequestModel->rowCount(); row < rows; ++row)
        compoundNames.insert(testRequestModel->index(row, compoundCol).data().toString());
  }
  querySolutions(compoundNames);
}

void TecanWindow::querySolutions(const QSet<QString> &compoundNames) {
  QList<int> selectedSolutionIds;
  QString err;
  QList<QVariantMap> solutionsFound = m_viewModel->getExperimentManager()->fetchSolutionsForCompounds(compoundNames, &err);
  if (!err.isEmpty()) {
      showError(this, tr("Query Error"), err);
  }

  QMap<QString, QList<QVariantMap>> solByCmp;
  for (const auto& sol : solutionsFound) {
      solByCmp[sol["product_name"].toString()].append(sol);
  }

  for (const QString &compound : compoundNames) {
      QList<QVariantMap> sols = solByCmp[compound];
      if (sols.size() == 1) {
          selectedSolutionIds << sols.first()["solution_id"].toInt();
      } else if (sols.size() > 1) {
          const int id = resolveCompoundDuplicates(compound, sols);
          if (id != -1) selectedSolutionIds << id;
      } else {
          showWarning(this, tr("No Solution Found"), tr("No solution found for compound '%1'.").arg(compound));
      }
  }

  populateCompoundTable(selectedSolutionIds);
}

int TecanWindow::resolveCompoundDuplicates(
    const QString &compoundName, const QList<QVariantMap> &duplicates) {
  QStringList items;
  QMap<QString, int> itemToId;

  for (const auto &sol : duplicates) {
    const QString desc =
        QStringLiteral(
            "Solution ID: %1 | Container: %2 | Well: %3 | Conc.: %4 %5")
            .arg(sol["invenesis_solution_id"].toString(),
                 sol["container_id"].toString(), sol["well_id"].toString(),
                 sol["concentration"].toString(),
                 sol["concentration_unit"].toString());

    items << desc;
    itemToId[desc] = sol["solution_id"].toInt();
  }

  bool ok = false;
  const QString choice = QInputDialog::getItem(
      this, tr("Select Solution for %1").arg(compoundName),
      tr("Multiple solutions found. Please select one:"), items, 0, false, &ok);

  return ok && itemToId.contains(choice) ? itemToId.value(choice) : -1;
}

void TecanWindow::populateCompoundTable(const QList<int> &solutionIds) {
  if (solutionIds.isEmpty()) {
    showInfo(this, tr("No Solutions"), tr("No solutions selected to display."));
    return;
  }

  QString err;
  QList<QVariantMap> data = m_viewModel->getExperimentManager()->fetchSolutionsByIds(solutionIds, &err);
  if (!err.isEmpty()) {
    showError(this, tr("Query Error"), err);
    return;
  }
  
  if (compoundQueryModel) delete compoundQueryModel;
  compoundQueryModel = m_viewModel->getExperimentManager()->createCompoundModel(data, this);
  ui->compoundQueryTableView->setModel(compoundQueryModel);
  ui->compoundQueryTableView->resizeColumnsToContents();

  /* ---------- update visual matrix plate ---------- */
  QMap<QString, QSet<QString>> plateData;
  for (int i = 0, rows = compoundQueryModel->rowCount(); i < rows; ++i) {
      int c_id_col = -1, w_id_col = -1;
      for (int c = 0; c < compoundQueryModel->columnCount(); ++c) {
          if (compoundQueryModel->headerData(c, Qt::Horizontal).toString() == "container_id") c_id_col = c;
          if (compoundQueryModel->headerData(c, Qt::Horizontal).toString() == "well_id") w_id_col = c;
      }
      if (c_id_col != -1 && w_id_col != -1) {
        plateData[compoundQueryModel->index(i, c_id_col).data().toString()].insert(compoundQueryModel->index(i, w_id_col).data().toString());
      }
  }
  matrixPlateContainer->populatePlates(plateData);

  /* ---------- prepare daughter plates ---------- */
  m_viewModel->setDaughterPlateType(ExperimentJsonSerializer::DaughterPlateType::Plate96);
  if (ui->switchPlate) ui->switchPlate->setChecked(false);

  rebuildDaughterPlatesFromModels();
}

/* ========================================================================== */
/*                                plate logic                                 */
/* ========================================================================== */

void TecanWindow::rebuildDaughterPlatesFromModels() {
  const QAbstractItemModel *trModel = ui->testRequestTableView->model();
  const QAbstractItemModel *cmpModel = ui->compoundQueryTableView->model();

  if (!trModel || trModel->rowCount() == 0 || !cmpModel ||
      cmpModel->rowCount() == 0) {
    return; // nothing loaded
  }

  auto columnOf = [](const QAbstractItemModel *m, const QString &hdr) -> int {
    if (!m)
      return -1;
    for (int c = 0; c < m->columnCount(); ++c)
      if (m->headerData(c, Qt::Horizontal).toString() == hdr)
        return c;
    return -1;
  };

  const int productCol = columnOf(cmpModel, "product_name");
  const int nDilCol = columnOf(trModel, "number_of_dilutions");
  const int testTypeCol = columnOf(trModel, "requested_tests");

  if (productCol < 0)
    return;

  QStringList compounds;
  for (int r = 0; r < cmpModel->rowCount(); ++r) {
    const QString name = cmpModel->index(r, productCol).data().toString();
    if (!name.isEmpty() && !compounds.contains(name))
      compounds << name;
  }

  const int dilutionSteps =
      (nDilCol >= 0) ? trModel->index(0, nDilCol).data().toInt() : 3;
  const QString testType =
      (testTypeCol >= 0) ? trModel->index(0, testTypeCol).data().toString()
                         : QString();

  populateDaughterPlates(dilutionSteps, compounds, testType);
}

void TecanWindow::populateDaughterPlates(int dilutionSteps, const QStringList &compoundList, const QString &testType) {
  while (auto *item = daughterPlatesLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  const bool is384 = (m_viewModel->getDaughterPlateType() == ExperimentJsonSerializer::DaughterPlateType::Plate384);
  QList<QMap<QString, QStringList>> plates = m_viewModel->getExperimentManager()->calculateInitialDaughterPlates(dilutionSteps, compoundList, testType, is384);

  for (int i = 0; i < plates.size(); ++i) {
    auto *plateWidget = new DaughterPlateWidget(i + 1, this);
    plateWidget->setPlateFormat(is384 ? DaughterPlateWidget::Plate384 : DaughterPlateWidget::Plate96);

    QMap<QString, QColor> colours;
    int hue = 0;
    const int hueStep = 360 / (plates[i].size() + 1);

    for (auto it = plates[i].cbegin(); it != plates[i].cend(); ++it) {
      if (it.key() == "DMSO")
        colours[it.key()] = Qt::gray;
      else if (it.key() == "Standard")
        colours[it.key()] = QColor(0, 122, 204);
      else {
        colours[it.key()] = QColor::fromHsv(hue, 200, 220);
        hue += hueStep;
      }
    }

    plateWidget->populatePlate(plates[i], colours, dilutionSteps);
    plateWidget->enableCompoundDragDrop(dilutionSteps);
    daughterPlatesLayout->addWidget(plateWidget);
  }

  populateTestPlates(dilutionSteps, testType, plates);
}

/* ========================================================================== */
/*                             UI slot handlers                               */
/* ========================================================================== */
void TecanWindow::on_clearPlatesButton_clicked() {
  if (QMessageBox::question(
          this, tr("Clear Plates"),
          tr("Are you sure you want to clear all compound placements?\n"
             "(Standard and DMSO will be preserved)"),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;

  for (int i = 0; i < daughterPlatesLayout->count(); ++i)
    if (auto *plate = qobject_cast<DaughterPlateWidget *>(
            daughterPlatesLayout->itemAt(i)->widget())) {
      plate->clearCompounds();
      plate->setAcceptDrops(true);
    }

  // Clear test and QC plates as well
  while (auto *item = testPlatesLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  while (auto *item = qcPlatesLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }
}

void TecanWindow::on_switchPlate_toggled(bool checked) {
  m_viewModel->setDaughterPlateType(checked ? ExperimentJsonSerializer::DaughterPlateType::Plate384 : ExperimentJsonSerializer::DaughterPlateType::Plate96);

  rebuildDaughterPlatesFromModels();
}

/* =========================================================================
 *  Persistence, load/save & GWL helpers – refactored April 2025
 *  (drop-in replacement for the second half of tecanwindow.cpp)
 * ========================================================================= */

/* ============================================================== */
/*                      SAVE EXPERIMENT                           */
/* ============================================================== */
void TecanWindow::on_actionSave_triggered() {
  /* --- ask experiment code ----------------------------------- */
  bool ok = false;
  const QString expCode = QInputDialog::getText(this, tr("Save Experiment"),
                                                tr("Enter experiment code:"),
                                                QLineEdit::Normal, "", &ok);
  if (!ok || expCode.trimmed().isEmpty())
    return;

  /* --- ask user name ----------------------------------------- */
  const QString username = QInputDialog::getText(
      this, tr("User"), tr("Enter your name:"), QLineEdit::Normal, "", &ok);
  if (!ok || username.trimmed().isEmpty())
    return;

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
  QList<DaughterPlateWidget*> daughterPlatesList;
  for (int i = 0; i < daughterPlatesLayout->count(); ++i) {
      daughterPlatesList.append(qobject_cast<DaughterPlateWidget*>(daughterPlatesLayout->itemAt(i)->widget()));
  }
  QList<DaughterPlateWidget*> testPlatesList;
  for (int i = 0; i < testPlatesLayout->count(); ++i) {
      testPlatesList.append(qobject_cast<DaughterPlateWidget*>(testPlatesLayout->itemAt(i)->widget()));
  }
  QList<DaughterPlateWidget*> qcPlatesList;
  for (int i = 0; i < qcPlatesLayout->count(); ++i) {
      qcPlatesList.append(qobject_cast<DaughterPlateWidget*>(qcPlatesLayout->itemAt(i)->widget()));
  }

  QJsonObject expJson = ExperimentJsonSerializer::buildExperimentJson(
      expCode, username,
      ui->testRequestTableView->model(),
      ui->compoundQueryTableView->model(),
      matrixPlateContainer->getPlateMap(),
      daughterPlatesList, testPlatesList, qcPlatesList,
      (m_viewModel->getDaughterPlateType() == ExperimentJsonSerializer::DaughterPlateType::Plate384) ? ExperimentJsonSerializer::DaughterPlateType::Plate384 : ExperimentJsonSerializer::DaughterPlateType::Plate96,
      qcSelectionCombo ? qcSelectionCombo->currentText() : QString()
  );
  if (expJson.isEmpty())
    return; // build routine already showed msg
  expJson["standard"] = stdObj;

  ExperimentJsonSerializer::addConcentrationsToExperimentJson(expJson, m_viewModel->getQcPlatesJson());

  qDebug() << QJsonDocument(expJson).toJson(QJsonDocument::Indented);

  /* --- write to DB ------------------------------------------- */
  QString err;
  if (!m_viewModel->getExperimentManager()->saveExperiment(expCode, username, stdObj, expJson, &err)) {
      showError(this, tr("Database Error"), tr("Failed to insert/update experiment:\n%1").arg(err));
      return;
  }
  showInfo(this, tr("Success"), tr("Experiment saved successfully!"));
  m_viewModel->setLastSavedExperimentJson(expJson);
}

/* ========================================================================= */
void TecanWindow::on_actionLoad_triggered() {
  LoadExperimentDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted)
    return;

  const int expId = dlg.selectedExperimentId();
  const bool readOnly = dlg.isReadOnly();

  if (expId == -1) {
    showWarning(this, tr("No Selection"),
                tr("Please select an experiment to load."));
    return;
  }

  QString err, expCode;
  QJsonObject loadedJson = m_viewModel->getExperimentManager()->loadExperiment(expId, &expCode, &err);
  if (!err.isEmpty()) {
      showError(this, tr("Error"), tr("Failed to load experiment:\n%1").arg(err));
      return;
  }
  
  m_viewModel->setLastSavedExperimentJson(loadedJson);

  /* ---- restore UI state ---- */
  loadTestRequestsFromJson(m_viewModel->getLastSavedExperimentJson()["test_requests"].toArray());
  loadCompoundsFromJson(m_viewModel->getLastSavedExperimentJson()["compounds"].toArray());
  loadMatrixPlatesFromJson(m_viewModel->getLastSavedExperimentJson()["matrix_plates"].toObject());

  QString qcType = m_viewModel->getLastSavedExperimentJson().value("qc_plate_type").toString();
  if (!qcType.isEmpty()) {
    qcSelectionCombo->blockSignals(true);
    qcSelectionCombo->setCurrentText(qcType);
    qcSelectionCombo->blockSignals(false);
  }

  loadDaughterPlatesFromJson(m_viewModel->getLastSavedExperimentJson()["daughter_plates"].toArray(), readOnly);

  showInfo(this, tr("Experiment Loaded"), tr("Experiment '%1' loaded successfully.").arg(expCode));
}

/* =========================================================================
 *  JSON → model helpers
 * ========================================================================= */
void TecanWindow::loadTestRequestsFromJson(const QJsonArray &array) {
  if (array.isEmpty()) return;
  auto* model = ExperimentJsonSerializer::parseTestRequests(array, this);
  if (model) ui->testRequestTableView->setModel(model);
}

void TecanWindow::loadCompoundsFromJson(const QJsonArray &array) {
  if (array.isEmpty()) return;
  auto* model = ExperimentJsonSerializer::parseCompounds(array, this);
  if (model) ui->compoundQueryTableView->setModel(model);
}

void TecanWindow::loadMatrixPlatesFromJson(const QJsonObject &obj) {
  QMap<QString, QSet<QString>> map;
  foreach (const QString &cid, obj.keys()) {
    QSet<QString> wells;
    foreach (const QJsonValue &w, obj[cid].toArray())
      wells.insert(w.toString());
    map[cid] = wells;
  }
  matrixPlateContainer->populatePlates(map);
}

void TecanWindow::loadDaughterPlatesFromJson(const QJsonArray &array,
                                             bool readOnly) {
  while (auto *item = daughterPlatesLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  // restore plate type (root has priority, per-plate as fallback)
  QString plateTypeStr = m_viewModel->getLastSavedExperimentJson().value("plate_type").toString();
  if (plateTypeStr.isEmpty() && !array.isEmpty())
    plateTypeStr = array.at(0).toObject().value("plate_type").toString();

  if (plateTypeStr == "384")
    m_viewModel->setDaughterPlateType(ExperimentJsonSerializer::DaughterPlateType::Plate384);
  else
    m_viewModel->setDaughterPlateType(ExperimentJsonSerializer::DaughterPlateType::Plate96);

  if (ui->switchPlate)
    ui->switchPlate->setChecked(m_viewModel->getDaughterPlateType() ==
                                ExperimentJsonSerializer::DaughterPlateType::Plate384);

  /* extract standard info if available */
  QString stdLabel, stdNotes;
  const QJsonObject stdObj = m_viewModel->getLastSavedExperimentJson()["standard"].toObject();
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

  const bool is384 = (m_viewModel->getDaughterPlateType() == ExperimentJsonSerializer::DaughterPlateType::Plate384);
  QList<QMap<QString, QStringList>> loadedPlates;
  int firstDilSteps = 3;

  for (int i = 0; i < array.size(); ++i) {
    const QJsonObject plateObj = array[i].toObject();
    const int dilSteps = plateObj.value("dilution_steps").toInt(3);
    if (i == 0)
      firstDilSteps = dilSteps;

    auto *plate = new DaughterPlateWidget(i + 1, this);
    plate->setPlateFormat(is384 ? DaughterPlateWidget::Plate384
                                : DaughterPlateWidget::Plate96);

    plate->fromJson(plateObj["wells"].toObject(), dilSteps);

    if (!readOnly)
      plate->enableCompoundDragDrop(dilSteps);
    if (hasStd)
      plate->setStandardInfo(stdLabel, stdNotes);

    daughterPlatesLayout->addWidget(plate);

    QMap<QString, QStringList> pMap;
    QJsonObject wellsObj = plateObj["wells"].toObject();
    for (auto it = wellsObj.constBegin(); it != wellsObj.constEnd(); ++it) {
      pMap[it.value().toString()].append(it.key());
    }
    loadedPlates.append(pMap);
  }

  QString testType;
  const QJsonArray trArr =
      m_viewModel->getLastSavedExperimentJson().value("test_requests").toArray();
  if (!trArr.isEmpty()) {
    testType = trArr.at(0).toObject().value("requested_tests").toString();
  }
  populateTestPlates(firstDilSteps, testType, loadedPlates);
}


// ==== small helpers (file-scope) =========================================
static inline QString instrumentToString(GWLGenerator::Instrument ins) {
  return ins == GWLGenerator::Instrument::FLUENT1080 ? "FLUENT1080" : "EVO150";
}
static inline GWLGenerator::Instrument instrumentFromString(const QString &s) {
  return s.compare("FLUENT1080", Qt::CaseInsensitive) == 0
             ? GWLGenerator::Instrument::FLUENT1080
             : GWLGenerator::Instrument::EVO150;
}

/* =======================================================================
 * 1) on_actionGenerate_GWL_triggered()  — with instrument dialog
 * ======================================================================= */
void TecanWindow::on_actionGenerate_GWL_triggered() {
  qDebug() << "[TRACE] GWL button pressed";

  /* 0 – must have at least one saved snapshot ------------------------ */
  if (m_viewModel->getLastSavedExperimentJson().isEmpty()) {
    showWarning(
        this, tr("Not Saved"),
        tr("You must save the experiment before generating a GWL file."));
    on_actionSave_triggered();
    return;
  }

  /* 1 – build JSON from CURRENT UI ---------------------------------- */
  const QString code = m_viewModel->getLastSavedExperimentJson()["experiment_code"].toString();
  const QString user = m_viewModel->getLastSavedExperimentJson()["user"].toString();
  QList<DaughterPlateWidget*> daughterPlatesList;
  for (int i = 0; i < daughterPlatesLayout->count(); ++i) {
      daughterPlatesList.append(qobject_cast<DaughterPlateWidget*>(daughterPlatesLayout->itemAt(i)->widget()));
  }
  QList<DaughterPlateWidget*> testPlatesList;
  for (int i = 0; i < testPlatesLayout->count(); ++i) {
      testPlatesList.append(qobject_cast<DaughterPlateWidget*>(testPlatesLayout->itemAt(i)->widget()));
  }
  QList<DaughterPlateWidget*> qcPlatesList;
  for (int i = 0; i < qcPlatesLayout->count(); ++i) {
      qcPlatesList.append(qobject_cast<DaughterPlateWidget*>(qcPlatesLayout->itemAt(i)->widget()));
  }

  QJsonObject currentJson = ExperimentJsonSerializer::buildExperimentJson(
      code, user,
      ui->testRequestTableView->model(),
      ui->compoundQueryTableView->model(),
      matrixPlateContainer->getPlateMap(),
      daughterPlatesList, testPlatesList, qcPlatesList,
      (m_viewModel->getDaughterPlateType() == ExperimentJsonSerializer::DaughterPlateType::Plate384) ? ExperimentJsonSerializer::DaughterPlateType::Plate384 : ExperimentJsonSerializer::DaughterPlateType::Plate96,
      qcSelectionCombo ? qcSelectionCombo->currentText() : QString()
  );

  /* always copy the STANDARD from the last saved version ------------- */
  currentJson["standard"] = m_viewModel->getLastSavedExperimentJson()["standard"];

  // Helper that asks instrument and forwards to generator
  auto chooseInstrumentAndGenerate = [&](const QJsonObject &jsonBase) {
    GenerateGwlDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
      qDebug() << "[TRACE] GWL dialog cancelled by user";
      return;
    }
    const auto instrument = dlg.useFluent()
                                ? GWLGenerator::Instrument::FLUENT1080
                                : GWLGenerator::Instrument::EVO150;

    QJsonObject json = jsonBase;
    json["_instrument"] =
        instrumentToString(instrument); // carry instrument through the pipeline
    json["_tip_size"] = dlg.selectedTipSize();
    json["_optimize_gwl"] = dlg.optimizeGwl();
    json["_override_min_vol"] = dlg.overrideMinVolume();
    ExperimentJsonSerializer::addConcentrationsToExperimentJson(json, m_viewModel->getQcPlatesJson());
    generateGWLFromJson(json);
  };

  /* 2 – compare order-insensitively ---------------------------------- */
  if (!JsonHelpers::jsonEqual(m_viewModel->getLastSavedExperimentJson(), currentJson)) {
    const auto choice = QMessageBox::question(
        this, tr("Experiment Modified"),
        tr("Changes have been made since last save.\n"
           "Do you want to overwrite the saved experiment?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (choice == QMessageBox::Cancel)
      return;

    if (choice == QMessageBox::Yes) {
      on_actionSave_triggered();
      return;
    }

    /* choice == No: generate with *currentJson* (unsaved edits) */
    chooseInstrumentAndGenerate(currentJson);
    return;
  }

  /* No differences: use last saved JSON */
  chooseInstrumentAndGenerate(m_viewModel->getLastSavedExperimentJson());
}

/* =======================================================================
 * 2) generateGWLFromJson() — uses new GWLGenerator and saveMany()
 * ======================================================================= */
void TecanWindow::generateGWLFromJson(const QJsonObject &experimentJson) {
  qDebug() << "[TRACE] generateGWLFromJson()";

  // 0) Instrument selection (defaults to EVO150 if unknown)
  const auto instrStr = experimentJson.value("_instrument").toString();
  const auto instrument = instrumentFromString(instrStr);

  // 1) Determine dilution factor (keep old fallback logic)
  double dilutionFactor = 3.16; // default
  const QJsonArray trArr = experimentJson.value("test_requests").toArray();
  if (!trArr.isEmpty()) {
    const auto tr0 = trArr.at(0).toObject();
    bool ok = false;
    // NOTE: some JSONs store a number as text here; if it's not numeric we
    // keep 3.16
    const double df = tr0.value("dilution_steps").toString().toDouble(&ok);
    if (ok && df > 0.0)
      dilutionFactor = df;
  }

  // 2) Extract test ID for volumeMap lookup
  QString testId;
  if (!trArr.isEmpty()) {
    testId = trArr.at(0).toObject().value("requested_tests").toString();
  }

  // 3) Extract the stock concentration (µM) of the first compound
  double stockConcMicroM = 0.0;
  const QJsonArray cmpArr = experimentJson.value("compounds").toArray();
  if (!cmpArr.isEmpty()) {
    const auto cmp0 = cmpArr.at(0).toObject();
    const double sc = cmp0.value("concentration").toDouble();
    const QString cu = cmp0.value("concentration_unit").toString();
    // convert mM → µM
    stockConcMicroM =
        (cu.compare("mM", Qt::CaseInsensitive) == 0) ? sc * 1000.0 : sc;
  }

  // 4) Instantiate the new generator (EVO150/Fluent handled inside)
  GWLGenerator generator(dilutionFactor, testId, stockConcMicroM, instrument);

  // 5) Generate all files (per-daughter/per-matrix); no master experiment .gwl
  QVector<GWLGenerator::FileOut> outs;
  QString err;
  if (!generator.generate(experimentJson, outs, &err)) {
    showError(this, tr("GWL Generation"), tr("Generator error: %1").arg(err));
    qCritical() << "[FATAL] generator.generate:" << err;
    return;
  }

  // 6) Also ask the backend for auxiliary files (plate maps, etc.)
  if (!generator.generateAuxiliary(experimentJson, outs, &err)) {
    // Non-fatal; just warn
    qWarning() << "[WARN] generator.generateAuxiliary:" << err;
  }

  // 7) Choose an output folder (robot expects `dght_0/…`, `dght_1/…` inside)
  const QString defaultDir =
      QStringLiteral("//Inv_syno_srv/INVENesis/Evo_pc/Fluent/Experiments");
  const QString outDir = QFileDialog::getExistingDirectory(
      this, tr("Select Output Folder"), defaultDir,
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (outDir.isEmpty())
    return; // user cancelled

  // 8) Write everything (saveMany creates subfolders as needed)
  if (!GWLGenerator::saveMany(outDir, outs, &err)) {
    showError(this, tr("File Error"),
              tr("Failed to write files:\n%1").arg(err));
    return;
  }

  // 9) Mark related test requests as done
  QString dbErr;
  if (!markTestRequestsDoneFromJson(experimentJson, &dbErr)) {
    // You can choose warning vs error. Warning is safer (GWL already
    // generated).
    showWarning(
        this, tr("Database Update"),
        tr("GWL was generated, but failed to mark test requests as done:\n%1")
            .arg(dbErr));
  }

  // 11) Done
  showInfo(this, tr("Success"), tr("Files written to:\n%1").arg(outDir));
}

/* =======================================================================
 * 3) generateExperimentAuxiliaryFiles() — thin wrapper to backend
 * ======================================================================= */
void TecanWindow::generateExperimentAuxiliaryFiles(
    const QJsonObject &exp, const QString &outputFolder) {
  qDebug() << "[TRACE] generateExperimentAuxiliaryFiles() to" << outputFolder;

  // Instrument (defaults to EVO150 if missing)
  const auto instrStr = exp.value("_instrument").toString();
  const auto instrument = instrumentFromString(instrStr);

  // Dilution factor (same fallback as above)
  double dilutionFactor = 3.16;
  const QJsonArray trArr = exp.value("test_requests").toArray();
  if (!trArr.isEmpty()) {
    const auto tr0 = trArr.at(0).toObject();
    bool ok = false;
    const double df = tr0.value("dilution_steps").toString().toDouble(&ok);
    if (ok && df > 0.0)
      dilutionFactor = df;
  }

  // Test ID
  QString testId;
  if (!trArr.isEmpty()) {
    testId = trArr.at(0).toObject().value("requested_tests").toString();
  }

  // Stock concentration (µM)
  double stockConcMicroM = 0.0;
  const QJsonArray cmpArr = exp.value("compounds").toArray();
  if (!cmpArr.isEmpty()) {
    const auto cmp0 = cmpArr.at(0).toObject();
    const double sc = cmp0.value("concentration").toDouble();
    const QString cu = cmp0.value("concentration_unit").toString();
    stockConcMicroM =
        (cu.compare("mM", Qt::CaseInsensitive) == 0) ? sc * 1000.0 : sc;
  }

  // Delegate to backend
  GWLGenerator generator(dilutionFactor, testId, stockConcMicroM, instrument);
  QVector<GWLGenerator::FileOut> aux;
  QString err;
  if (!generator.generateAuxiliary(exp, aux, &err)) {
    qWarning() << "[WARN] generateAuxiliary:" << err;
    return;
  }
  if (!GWLGenerator::saveMany(outputFolder, aux, &err)) {
    qWarning() << "[WARN] saveMany(aux):" << err;
    return;
  }

  qDebug() << "[TRACE] Auxiliary files saved into" << outputFolder;
}

void TecanWindow::on_actionCreate_Plate_Map_triggered() {
  PlateMapDialog dlg(this);
  dlg.exec();
}

bool TecanWindow::markTestRequestsDoneFromJson(const QJsonObject &experimentJson, QString *errOut) {
  const QJsonArray trArr = experimentJson.value("test_requests").toArray();
  if (trArr.isEmpty()) return true;

  QSet<QString> idsSet;
  for (const QJsonValue &v : trArr) {
    const QString id = v.toObject().value("request_id").toString().trimmed();
    if (!id.isEmpty()) idsSet.insert(id);
  }
  if (idsSet.isEmpty()) return true;

  const QStringList ids = QStringList(idsSet.begin(), idsSet.end());
  return m_viewModel->getExperimentManager()->markTestRequestsDone(ids, errOut);
}

void TecanWindow::loadQcPlatesConfig() {
  QFile qcFile(":/data/resources/data/qc_plates.json");
  if (qcFile.open(QIODevice::ReadOnly)) {
    m_viewModel->setQcPlatesJson(QJsonDocument::fromJson(qcFile.readAll()).object());
    qcFile.close();

    qcSelectionCombo->blockSignals(true);
    qcSelectionCombo->clear();
    qcSelectionCombo->addItems(m_viewModel->getQcPlatesJson().keys());
    qcSelectionCombo->blockSignals(false);
  }
}

void TecanWindow::onQcSelectionChanged(const QString & /*qcName*/) {
  rebuildDaughterPlatesFromModels();
}

void TecanWindow::populateTestPlates(int dilutionSteps, const QString &testType, const QList<QMap<QString, QStringList>> &daughterPlates) {
  while (auto *item = testPlatesLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  while (auto *item = qcPlatesLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  QString qcType = qcSelectionCombo->currentText();
  ExperimentManager::TestPlatesResult res = m_viewModel->getExperimentManager()->calculateTestPlates(dilutionSteps, testType, daughterPlates, m_viewModel->getQcPlatesJson(), qcType);

  auto *qcPlateWidget = new DaughterPlateWidget(1, this);
  qcPlateWidget->setPlateFormat(DaughterPlateWidget::Plate96);
  QMap<QString, QColor> qcColours;
  qcColours["DMSO"] = Qt::gray;
  qcColours["Standard Compound A"] = QColor(0, 122, 204);
  qcColours["Standard Compound B"] = QColor(0, 204, 122);
  qcPlateWidget->populatePlate(res.qcPlate, qcColours, 12);
  qcPlatesLayout->addWidget(qcPlateWidget);

  bool is384Test = false;
  QFile vocabFile(":/data/resources/data/tests_vocabulary.json");
  if (vocabFile.open(QIODevice::ReadOnly)) {
      QJsonObject vocab = QJsonDocument::fromJson(vocabFile.readAll()).object();
      if (vocab.value(testType).toString() == "384") is384Test = true;
      vocabFile.close();
  }

  for (int i = 0; i < res.testPlates.size(); ++i) {
      auto *testWidget = new DaughterPlateWidget(i + 1, this);
      testWidget->setPlateFormat(is384Test ? DaughterPlateWidget::Plate384 : DaughterPlateWidget::Plate96);
      
      QMap<QString, QColor> testColours;
      int hue = 0, step = 360 / (res.testPlates[i].size() + 1);
      for (const QString &cmp : res.testPlates[i].keys()) {
          QColor color;
          if (cmp.contains("Standard")) color = QColor(0, 204, 122);
          else if (cmp.contains("DMSO")) color = Qt::gray;
          else {
              color = QColor::fromHsv(hue, 200, 220);
              hue += step;
          }
          if (cmp == "Standard") color = QColor(0, 122, 204);
          testColours[cmp] = color;
      }
      testWidget->populatePlate(res.testPlates[i], testColours, dilutionSteps);
      testPlatesLayout->addWidget(testWidget);
  }
}


