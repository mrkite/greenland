#ifndef MVE_H
#define MVE_H

#include "handle.h"
#include <QScrollArea>
#include <QImage>
#include <QLabel>
#include <QTimer>

class MVE : public QScrollArea {
  Q_OBJECT
public:
  MVE(QSharedPointer<Handle> handle, QWidget *parent = 0);
  virtual ~MVE();

signals:

public slots:

private slots:
  void readChunk();

private:
  void handleChunk(quint32 size);
  void initVideo();
  void saveFrame(quint16 wc);
  void decodeBlocks(quint8 *control, quint16 rx, quint16 ry,
                    quint16 w, quint16 h);
  void sameBlock(quint8 *dest);
  void copyPreBlock(quint8 *dest);
  void copyBlock(quint8 *dest);
  void copyBackBlock(quint8 *dest);
  void shiftBackBlock(quint8 *dest);
  void draw2ColBitBlock(quint8 *dest);
  void draw4ColBitBlock(quint8 *dest);
  void draw8ColBitBlock(quint8 *dest);
  void draw16ColBitBlock(quint8 *dest);
  void drawFullBlock(quint8 *dest);
  void fill1x1CheckedBlock(quint8 *dest);
  void fill2x2CheckedBlock(quint8 *dest);
  void fill4x4CheckedBlock(quint8 *dest);
  void fillSolidBlock(quint8 *dest);


  QSharedPointer<Handle> handle;
  quint8 palette[256 * 3];
  quint8 *surface, *surface2;
  QImage *image;
  bool updateScreen;
  quint32 framewidth, frameheight;
  quint32 realX, realY, realW, realH;
  quint16 width, height;
  QLabel *view;
  QTimer timer;
};

#endif // MVE_H
