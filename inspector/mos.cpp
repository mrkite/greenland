#include "mos.h"
#include "mainwindow.h"
#include <QLabel>
#include <QDebug>
#include <zlib.h>

MOS::MOS(QSharedPointer<Handle> handle, QWidget *parent) : QScrollArea(parent) {
  QString key = handle->r4();
  if (key == "MOSC") {
    ulong len = handle->r32();
    handle->skip(4); //unknown
    Bytef *in = (Bytef *)handle->read(handle->length() - 12);
    Bytef *out = new Bytef[len];
    uncompress(out, &len, in, handle->length() - 12);
    QByteArray arr((const char *)out, len);
    handle = QSharedPointer<Handle>(new Handle(arr));
    key = handle->r4();
  }

  if (key != "MOS ")
    throw FileError("Not a MOS file");

  handle->skip(4); //unknown
  quint16 w = handle->r16();
  quint16 h = handle->r16();
  quint16 cols = handle->r16();
  quint16 rows = handle->r16();
  handle->skip(4); //bsize
  quint32 offset = handle->r32();

  QImage image(w, h, QImage::Format_RGBA8888);
  uchar *bits = image.bits();
  int stride = image.bytesPerLine();

  handle->seek(offset);
  quint8 *palette = (quint8 *)handle->read(256 * 4 * cols * rows);
  quint32 *offsets = new quint32[cols * rows];
  for (int y = 0, off = 0; y < rows; y++)
    for (int x = 0; x < cols; x++, off++)
      offsets[off] = handle->r32();
  qint64 tiledata = handle->tell();
  int poff = 0;
  quint8 c;
  for (int ty = 0, off = 0; ty < rows; ty++) {
    for (int tx = 0; tx < cols; tx++, off++) {
      quint16 tw = 64, th = 64;
      if (tx == cols - 1) tw = w & 63;
      if (ty == rows - 1) th = h & 63;
      if (tw == 0) tw = 64;
      if (th == 0) th = 64;
      handle->seek(tiledata + offsets[off]);

      for (int y = 0; y < th; y++) {
        int out = (y + ty * 64) * stride + tx * 64 * 4;
        for (int x = 0; x < tw; x++) {
          c = handle->r8();
          bits[out++] = palette[poff + c * 4 + 2];
          bits[out++] = palette[poff + c * 4 + 1];
          bits[out++] = palette[poff + c * 4 + 0];
          if (palette[poff + c * 4 + 2] == 0 &&
              palette[poff + c * 4 + 1] == 0xff &&
              palette[poff + c * 4 + 0] == 0)
            bits[out++] = 0;
          else
            bits[out++] = 255;
        }
      }
      poff += 256 * 4;
    }
  }
  delete[] offsets;

  QLabel *label = new QLabel(this);
  label->setPixmap(QPixmap::fromImage(image));
  setWidget(label);
}
