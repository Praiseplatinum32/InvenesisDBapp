#include "draggabletableview.h"


#include <QApplication>

DraggableTableView::DraggableTableView(QWidget *parent)
    : QTableView(parent)
{
    setDragEnabled(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setDragDropMode(QAbstractItemView::DragOnly);
}

void DraggableTableView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        dragStartPos = event->pos();

    QTableView::mousePressEvent(event);
}

void DraggableTableView::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;

    if ((event->pos() - dragStartPos).manhattanLength() < QApplication::startDragDistance())
        return;

    QModelIndex index = indexAt(dragStartPos);
    if (!index.isValid())
        return;

    QString compound = model()->data(model()->index(index.row(), 0)).toString();  // column 0 = compound name

    QMimeData *mimeData = new QMimeData;
    mimeData->setText(compound);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
}
