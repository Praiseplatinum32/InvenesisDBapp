#include "loadexperimentdialog.h"
#include "ui_loadexperimentdialog.h"
#include <QSqlError>
#include <QMessageBox>
#include "../services/ExperimentManager.h"

LoadExperimentDialog::LoadExperimentDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoadExperimentDialog)
{
    ui->setupUi(this);
    setWindowTitle("Load Experiment");

    // Load experiment list
    ExperimentManager manager;
    experimentModel = manager.fetchExperimentListModel(this);

    if (experimentModel->lastError().isValid()) {
        QMessageBox::critical(this, "Error", "Failed to load experiments:\n" + experimentModel->lastError().text());
        return;
    }

    ui->experimentTableView->setModel(experimentModel);
    ui->experimentTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->experimentTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->experimentTableView->resizeColumnsToContents();

    connect(ui->experimentTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LoadExperimentDialog::onSelectionChanged);
}

LoadExperimentDialog::~LoadExperimentDialog()
{
    delete ui;
}

void LoadExperimentDialog::onSelectionChanged()
{
    QModelIndex index = ui->experimentTableView->currentIndex();
    if (index.isValid()) {
        selectedId = experimentModel->data(experimentModel->index(index.row(), 0)).toInt(); // column 0 = experiment_id
    }
}

int LoadExperimentDialog::selectedExperimentId() const
{
    return selectedId;
}

bool LoadExperimentDialog::isReadOnly() const
{
    return ui->readOnlyCheckBox->isChecked();
}