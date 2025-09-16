#ifndef STANDARDSELECTIONDIALOG_H
#define STANDARDSELECTIONDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace Ui {
class StandardSelectionDialog;
}

class StandardSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StandardSelectionDialog(QWidget *parent = nullptr);
    ~StandardSelectionDialog();

    // Access to selected information
    QString selectedStandard() const;
    QString notes() const;
    QJsonObject selectedStandardJson() const;

    // Optional: preload alternative list externally (not used with JSON resource)
    void setAvailableStandards(const QStringList &standards);

private:
    void loadStandardJson();               // Handles JSON resource loading
    void populateComboBox();              // Populates combo with readable labels
    void displayStandardDetails(int index); // Shows JSON preview in the textEdit

    Ui::StandardSelectionDialog *ui;
    QList<QJsonObject> standardObjects;   // Raw data for each standard row
};

#endif // STANDARDSELECTIONDIALOG_H
