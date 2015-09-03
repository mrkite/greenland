#ifndef UNK_H
#define UNK_H

#include <QWidget>
#include "handle.h"

class UNK : public QWidget {
  Q_OBJECT
public:
  UNK(QSharedPointer<Handle> handle, QWidget *parent = 0);

signals:

public slots:

};

#endif // UNK_H
