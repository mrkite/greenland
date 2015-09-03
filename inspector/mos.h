#ifndef MOS_H
#define MOS_H

#include <QScrollArea>
#include "handle.h"

class MOS : public QScrollArea {
  Q_OBJECT
public:
  MOS(QSharedPointer<Handle> handle, QWidget *parent = 0);

signals:

public slots:

};

#endif // MOS_H
