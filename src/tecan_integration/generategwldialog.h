#ifndef GENERATEGWLDIALOG_H
#define GENERATEGWLDIALOG_H
#pragma once
#include <QDialog>

class QCheckBox;
class QPushButton;

class GenerateGwlDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GenerateGwlDialog(QWidget *parent = nullptr);
    bool useFluent() const;

private:
    QCheckBox *fluentCheck_{nullptr};
    QPushButton *ok_{nullptr};
    QPushButton *cancel_{nullptr};
};


#endif // GENERATEGWLDIALOG_H
