#include "bmp.h"
#include "mainwindow.h"
#include <QImage>
#include <QLabel>

BMP::BMP(QSharedPointer<Handle> handle, QWidget *parent)
  : QScrollArea(parent) {
  QString key = handle->rs(2);
  if (key != "BM")
    throw FileError("Not a valid bitmap");

  handle->seek(10);
  quint32 offs = handle->r32();
  handle->skip(4); //unknown
  quint32 w = handle->r32();
  quint32 h = handle->r32();
  handle->skip(2); //planes
  quint16 bpp = handle->r16();
  handle->skip(4); // compression

  quint8 *colors = 0;

  if (bpp == 4 || bpp == 8) {
    colors = new quint8[3 * (1 << bpp)];
    handle->seek(54);
    for (int i = 0; i < (1 << bpp); i++) {
      colors[i * 3 + 2] = handle->r8(); //b
      colors[i * 3 + 1] = handle->r8(); //g
      colors[i * 3 + 0] = handle->r8(); //r
      handle->skip(1); //a
    }
  }

  QImage image(w, h, QImage::Format_RGB888);
  uchar *bits = image.bits();
  int stride = image.bytesPerLine();

  quint8 c;
  handle->seek(offs);
  for (int y = h - 1; y >= 0; y--) {
    quint32 out = y * stride;
    bool pix = false;
    int px = 0;
    for (uint x = 0; x < w; x++) {
      if (bpp == 4) {
        if (!pix) {
          px++;
          c = handle->r8();
          bits[out++] = colors[(c >> 4) * 3];
          bits[out++] = colors[(c >> 4) * 3 + 1];
          bits[out++] = colors[(c >> 4) * 3 + 2];
          pix = true;
        } else {
          bits[out++] = colors[(c & 0xf) * 3];
          bits[out++] = colors[(c & 0xf) * 3 + 1];
          bits[out++] = colors[(c & 0xf) * 3 + 2];
          pix = false;
        }
      } else if (bpp == 8) {
        px++;
        c = handle->r8();
        bits[out++] = colors[c * 3];
        bits[out++] = colors[c * 3 + 1];
        bits[out++] = colors[c * 3 + 2];
      } else {
        px += 3;
        bits[out + 2] = handle->r8();
        bits[out + 1] = handle->r8();
        bits[out + 0] = handle->r8();
        out += 3;
      }
    }
    while (px & 3) {
      handle->skip(1);
      px++;
    }
  }

  if (colors)
    delete [] colors;

  QLabel *label = new QLabel(this);
  label->setPixmap(QPixmap::fromImage(image));
  setWidget(label);
}
