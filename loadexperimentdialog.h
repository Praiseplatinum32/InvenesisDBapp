#ifndef LOADEXPERIMENTDIALOG_H
#define LOADEXPERIMENTDIALOG_H

#include <QDialog>
#include <QSqlQueryModel>

namespace Ui {
class LoadExperimentDialog;
}

class LoadExperimentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoadExperimentDialog(QWidget *parent = nullptr);
    ~LoadExperimentDialog();

    int selectedExperimentId() const;
    bool isReadOnly() const;

private slots:
    void onSelectionChanged();

private:
    Ui::LoadExperimentDialog *ui;
    QSqlQueryModel *experimentModel;
    int selectedId = -1;
};

#endif // LOADEXPERIMENTDIALOG_H
