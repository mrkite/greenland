#ifndef BIF_H
#define BIF_H

#include "handle.h"

class BIF {
public:
  BIF(QString path);
  QSharedPointer<Handle> get(quint16 tileset, quint16 index);
private:
  QSharedPointer<Handle> getBlock(qint64 offset, quint64 length);
  QSharedPointer<Handle> handle;
};

#endif // BIF_H
