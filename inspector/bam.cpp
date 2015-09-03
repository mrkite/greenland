#include "bam.h"
#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSpinBox>
#include <QPushButton>
#include <QStyle>
#include <zlib.h>
#include <QDebug>

BAM::BAM(QSharedPointer<Handle> handle, QWidget *parent) : QWidget(parent) {
  QString key = handle->r4();
  if (key == "BAMC") {
    ulong len = handle->r32();
    handle->skip(4); //unknown
    Bytef *in = (Bytef *)handle->read(handle->length() - 12);
    Bytef *out = new Bytef[len];
    uncompress(out, &len, in, handle->length() - 12);
    QByteArray arr((const char *)out, len);
    handle = QSharedPointer<Handle>(new Handle(arr));
    key = handle->r4();
  }

  if (key != "BAM ")
    throw FileError("Not a BAM file");

  handle->skip(4); // 4 unknown
  quint16 numFrames = handle->r16(); //8
  quint8 numCycles = handle->r8();  //10
  quint8 clone = handle->r8(); //11

  quint32 frameOfs = handle->r32(); //12
  quint32 palOfs = handle->r32(); //16
  quint32 idxOfs = handle->r32(); //20

  handle->seek(palOfs);
  quint8 *palette = (quint8*)handle->read(256 * 4);
  int trans = 0;
  for (int f = 0; f < 256; f++) {
    if (palette[f * 4] == 0 && palette[f * 4 + 1] == 0xff &&
        palette[f * 4 + 2] == 0) {
      trans = f;
      break;
    }
  }

  handle->seek(frameOfs + numFrames * 12);

  struct FramePtr {
    quint16 numFrames;
    quint16 frameID;
  };
  QList<FramePtr> frameptrs;

  for (int i = 0; i < numCycles; i++) {
    FramePtr frameptr;
    frameptr.numFrames = handle->r16();
    if (frameptr.numFrames == 0)
      frameptr.numFrames = 1;
    frameptr.frameID = handle->r16();
    frameptrs.append(frameptr);
  }

  for (auto ptr : frameptrs) {
    Cycle cycle;
    int minX, minY, maxX, maxY, cenX, cenY;

    for (int f = 0; f < ptr.numFrames; f++) {
      Frame frame;
      handle->seek(idxOfs + ptr.frameID * 2 + f * 2);
      quint16 num = handle->r16();
      if (num == 0xffff) {
        frame.width = 0;
        frame.height = 0;
        frame.x = 0;
        frame.y = 0;
        frame.image = NULL;
      } else {
        handle->seek(frameOfs + num * 12);
        frame.width = handle->r16();
        frame.height = handle->r16();
        frame.x = (qint16)handle->r16();
        frame.y = (qint16)handle->r16();
        quint32 foffs = handle->r32();
        if (f == 0) {
          minX = minY = 0;
          cenX = frame.x;
          cenY = frame.y;
          maxX = frame.width;
          maxY = frame.height;
        }
        frame.x = cenX - frame.x;
        frame.y = cenY - frame.y;
        minX = qMin(frame.x, minX);
        minY = qMin(frame.y, minY);
        maxX = qMax(frame.x + frame.width, maxX);
        maxY = qMax(frame.y + frame.height, maxY);
        frame.image = new QImage(frame.width, frame.height,
                                 QImage::Format_RGBA8888);
        uchar *bits = frame.image->bits();
        int stride = frame.image->bytesPerLine();

        if (foffs & 0x80000000) {
          foffs &= 0x7fffffff;
          handle->seek(foffs);
          for (int y = 0; y < frame.height; y++) {
            int out = y * stride;
            for (int x = 0; x < frame.width; x++) {
              quint8 c = handle->r8();
              bits[out++] = palette[c * 4 + 2];
              bits[out++] = palette[c * 4 + 1];
              bits[out++] = palette[c * 4 + 0];
              bits[out++] = 0xff;
            }
          }
        } else {
          handle->seek(foffs);
          for (int y = 0; y < frame.height; y++) {
            int out = y * stride;
            for (int x = 0; x < frame.width && y < frame.height;) {
              quint8 c = handle->r8();
              if (c == clone) {
                int n = handle->r8() + 1;
                while (n-- && y < frame.height) {
                  if (c == trans) {
                    bits[out++] = 0;
                    bits[out++] = 0;
                    bits[out++] = 0;
                    bits[out++] = 0;
                  } else {
                    bits[out++] = palette[c * 4 + 2];
                    bits[out++] = palette[c * 4 + 1];
                    bits[out++] = palette[c * 4 + 0];
                    bits[out++] = 0xff;
                  }
                  x++;
                  if (x == frame.width) {
                    x = 0;
                    y++;
                    out = y * stride;
                  }
                }
              } else {
                if (c == trans) {
                  bits[out++] = 0;
                  bits[out++] = 0;
                  bits[out++] = 0;
                  bits[out++] = 0;
                } else {
                  bits[out++] = palette[c * 4 + 2];
                  bits[out++] = palette[c * 4 + 1];
                  bits[out++] = palette[c * 4 + 0];
                  bits[out++] = 0xff;
                }
                x++;
              }
            }
          }
        }
      }
      cycle.frames.append(frame);
    }
    for (auto &f : cycle.frames) {
      f.x -= minX;
      f.y -= minY;
    }
    cycle.width = maxX - minX;
    cycle.height = maxY - minY;
    cycle.frame = 0;
    cycles.append(cycle);
  }

  QVBoxLayout *layout = new QVBoxLayout(this);
  setLayout(layout);

  QWidget *top = new QWidget(this);
  QHBoxLayout *buttons = new QHBoxLayout(top);
  layout->addWidget(top);

  buttons->addWidget(new QLabel(tr("Cycle:")));

  curCycle = 0;
  QSpinBox *spinner = new QSpinBox(this);
  spinner->setMinimum(0);
  spinner->setMaximum(cycles.count() - 1);
  connect(spinner, SIGNAL(valueChanged(int)),
          this, SLOT(changeCycle(int)));
  buttons->addWidget(spinner);

  buttons->addWidget(new QLabel(tr("Frame:")));
  frameNo = new QLabel("0");
  buttons->addWidget(frameNo);

  QPushButton *prev = new QPushButton(this);
  prev->setIcon(style()->standardIcon(QStyle::SP_MediaSeekBackward));
  connect(prev, SIGNAL(clicked()),
          this, SLOT(goPrev()));
  buttons->addWidget(prev);
  QPushButton *play = new QPushButton(this);
  play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  connect(play, SIGNAL(clicked()),
          this, SLOT(play()));
  buttons->addWidget(play);
  QPushButton *next = new QPushButton(this);
  next->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
  connect(next, SIGNAL(clicked()),
          this, SLOT(goNext()));
  buttons->addWidget(next);

  view = new QLabel(this);
  Cycle const &c = cycles[curCycle];
  view->resize(c.width, c.height);
  QScrollArea *sa = new QScrollArea(this);
  sa->setWidget(view);
  layout->addWidget(sa, 1);
  update();

  connect(&timer, SIGNAL(timeout()),
          this, SLOT(tick()));
}

void BAM::changeCycle(int cycle) {
  curCycle = cycle;
  Cycle const &c = cycles[cycle];
  view->resize(c.width, c.height);
  update();
}

void BAM::goPrev() {
  timer.stop();
  Cycle &c = cycles[curCycle];
  c.frame--;
  if (c.frame < 0)
    c.frame = c.frames.count() - 1;

  update();
}

void BAM::goNext() {
  timer.stop();
  Cycle &c = cycles[curCycle];
  c.frame++;
  if (c.frame >= c.frames.count())
    c.frame = 0;

  update();
}

void BAM::tick() {
  Cycle &c = cycles[curCycle];
  c.frame++;
  if (c.frame >= c.frames.count())
    c.frame = 0;

  update();
}

void BAM::play() {
  if (timer.isActive())
    timer.stop();
  else
    timer.start(150);
}

void BAM::update() {
  Cycle const &c = cycles[curCycle];
  frameNo->setText(QString("%1").arg(c.frame));
  Frame const &f = c.frames[c.frame];
  view->setPixmap(QPixmap::fromImage(*f.image));
  view->setContentsMargins(f.x, f.y, 0, 0);
}
