#ifndef DRAGGABLETABLEVIEW_H
#define DRAGGABLETABLEVIEW_H

#include <QTableView>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>

class DraggableTableView : public QTableView
{
    Q_OBJECT

public:
    explicit DraggableTableView(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QPoint dragStartPos;
};

#endif // DRAGGABLETABLEVIEW_H