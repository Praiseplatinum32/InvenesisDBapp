#include "standardselectiondialog.h"
#include "ui_standardselectiondialog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>

StandardSelectionDialog::StandardSelectionDialog(QWidget *parent)
    : QDialog(parent),
    ui(new Ui::StandardSelectionDialog)
{
    ui->setupUi(this);
    ui->comboBox->clear();
    loadStandardJson();

    // Show detail on selection
    connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StandardSelectionDialog::displayStandardDetails);
}

StandardSelectionDialog::~StandardSelectionDialog()
{
    delete ui;
}

void StandardSelectionDialog::loadStandardJson()
{
    QFile file(":/standardjson/jsonfile/standards_matrix.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to load standard JSON resource.";
        ui->comboBox->addItem("Error loading standards");
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (doc.isNull()) {
        qWarning() << "Failed to parse JSON:" << error.errorString();
        ui->comboBox->addItem("Parsing error");
        return;
    }

    if (!doc.isArray()) {
        qWarning() << "Standard JSON is not a JSON array.";
        ui->comboBox->addItem("Invalid format");
        return;
    }

    QJsonArray standards = doc.array();
    for (const QJsonValue &val : standards) {
        QJsonObject obj = val.toObject();
        QString name = obj["Samplealias"].toString().trimmed();
        QString well = obj["Containerposition"].toString();
        QString conc = obj["Concentration"].toString();
        QString unit = obj["ConcentrationUnit"].toString();
        QString barcode = obj["Containerbarcode"].toString();

        if (name.isEmpty() || well.isEmpty() || conc.isEmpty())
            continue;

        QString label = QString("%1 – Well: %2 – %3 %4 – Barcode: %5")
                            .arg(name, well, conc, unit, barcode);

        ui->comboBox->addItem(label);
        standardObjects.append(obj);
    }

    if (!standardObjects.isEmpty())
        displayStandardDetails(0);
}

void StandardSelectionDialog::displayStandardDetails(int index)
{
    if (index >= 0 && index < standardObjects.size()) {
        const QJsonObject &selected = standardObjects.at(index);
        ui->textEdit->setPlainText(QJsonDocument(selected).toJson(QJsonDocument::Indented));
    }
}

QString StandardSelectionDialog::selectedStandard() const
{
    return ui->comboBox->currentText();
}

QString StandardSelectionDialog::notes() const
{
    return ui->textEdit->toPlainText();
}

QJsonObject StandardSelectionDialog::selectedStandardJson() const
{
    int idx = ui->comboBox->currentIndex();
    return (idx >= 0 && idx < standardObjects.size()) ? standardObjects[idx] : QJsonObject();
}

void StandardSelectionDialog::setAvailableStandards(const QStringList &standards)
{
    ui->comboBox->clear();
    ui->comboBox->addItems(standards);
}
