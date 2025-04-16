#ifndef CUSTOMPROXYMODEL_H
#define CUSTOMPROXYMODEL_H

#include <QSortFilterProxyModel>

class CustomProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit CustomProxyModel(QObject *parent = nullptr);

    void setFilter1(const QString &text, int column);
    void setFilter2(const QString &text, int column);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString filterText1;
    QString filterText2;
    int filterColumn1 = -1;
    int filterColumn2;
};

#endif // CUSTOMPROXYMODEL_H
