#ifndef BMP_H
#define BMP_H

#include <QScrollArea>
#include "handle.h"

class BMP : public QScrollArea {
  Q_OBJECT
public:
  BMP(QSharedPointer<Handle> handle, QWidget *parent = 0);

signals:

public slots:

};

#endif // BMP_H
