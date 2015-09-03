#ifndef BAM_H
#define BAM_H

#include <QWidget>
#include <QList>
#include <QImage>
#include <QLabel>
#include "handle.h"

struct Frame {
  QImage *image;
  int width, height;
  int x, y;
};

struct Cycle {
  int width;
  int height;
  int frame;
  QList<Frame> frames;
};

class BAM : public QWidget {
  Q_OBJECT
public:
  BAM(QSharedPointer<Handle> handle, QWidget *parent = 0);

private:
  quint8 curCycle;
  QLabel *view;
  QList<Cycle> cycles;
};

#endif // BAM_H
