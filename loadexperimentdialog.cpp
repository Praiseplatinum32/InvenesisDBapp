#include "loadexperimentdialog.h"
#include "ui_loadexperimentdialog.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>

LoadExperimentDialog::LoadExperimentDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoadExperimentDialog),
    experimentModel(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    setWindowTitle("Load Experiment");

    // Load experiment list
    experimentModel->setQuery(R"(
        SELECT experiment_id, experiment_code, project_code, date_created, user
        FROM experiments
        ORDER BY date_created DESC
    )");

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
