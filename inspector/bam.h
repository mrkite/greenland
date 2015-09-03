#ifndef BAM_H
#define BAM_H

#include <QWidget>
#include <QList>
#include <QImage>
#include <QLabel>
#include <QTimer>
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

private slots:
  void changeCycle(int cycle);
  void goPrev();
  void goNext();
  void play();
  void tick();

private:
  void update();
  quint8 curCycle;
  QLabel *frameNo;
  QLabel *view;
  QList<Cycle> cycles;
  QTimer timer;
};

#endif // BAM_H
