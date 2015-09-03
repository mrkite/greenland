#include "bif.h"
#include "mainwindow.h"
#include <zlib.h>

BIF::BIF(QString path) {
  handle = QSharedPointer<Handle>(new Handle(path));
  if (!handle->exists())
    throw FileError(QString("Can't open %1").arg(path));
}


QSharedPointer<Handle> BIF::get(quint16 tileset, quint16 index) {
  handle->seek(0);
  QString key = handle->r4();
  if (key != "BIFF" && key != "BIFC")
    throw FileError("Not a BIF file");
  bool compressed = key == "BIFC";

  QString ver = handle->r4();
  if (ver != "V1  " && ver != "V1.0")
    throw FileError("Invalid BIF version");

  if (!tileset) {
    if (compressed) {
      auto h = getBlock(16, 4);
      quint32 fileOffs = h->r32() + index * 16;
      h = getBlock(fileOffs, 16);
      h->skip(4); //locator
      quint32 offset = h->r32();
      quint32 size = h->r32();
      return getBlock(offset, size);
    } else {
      handle->skip(8); //num files and num tiles
      quint32 fileOffs = handle->r32() + index * 16;
      handle->seek(fileOffs);
      handle->skip(4); //locator
      quint32 offset = handle->r32();
      quint32 size = handle->r32();
      return QSharedPointer<Handle>(new Handle(handle, offset, size));
    }
  } else {
    if (compressed) {
      auto h = getBlock(8, 4);
      quint32 numFiles = h->r32();
      h = getBlock(16, 4);
      quint32 fileOffs = h->r32() + numFiles * 16 + tileset * 20;
      h = getBlock(fileOffs, 20);
      h->skip(4); //locator
      quint32 offset = h->r32();
      quint32 numTiles = h->r32();
      quint32 size = h->r32();
      return getBlock(offset, size * numTiles);
    } else {
      quint32 numFiles = handle->r32();
      handle->skip(4); //num tiles
      quint32 fileOffs = handle->r32() + numFiles * 16 + tileset * 20;
      handle->seek(fileOffs);
      handle->skip(4); //locator
      quint32 offset = handle->r32();
      quint32 numTiles = handle->r32();
      quint32 size = handle->r32();
      return QSharedPointer<Handle>(new Handle(handle, offset, size * numTiles));
    }
  }
}

QSharedPointer<Handle> BIF::getBlock(qint64 offset, quint64 length) {
  handle->seek(12);
  quint32 cur = 0;
  quint32 cLen = 0;
  ulong uLen = 0;
  while (cur <= offset) {
    handle->skip(cLen);
    uLen = handle->r32();
    cLen = handle->r32();
    cur += uLen;
  }

  QByteArray arr;
  quint32 delta = offset - (cur - uLen);
  quint32 dLen = 0;
  while (dLen < length) {
    Bytef *in = (Bytef *)handle->read(cLen);
    Bytef *out = new Bytef[uLen];
    uncompress(out, &uLen, in, cLen);
    uLen -= delta;
    if (uLen + dLen > length)
      uLen = length - dLen;
    arr.append((const char *)out + delta, uLen);
    dLen += uLen;
    delta = 0;
    delete [] out;
    uLen = handle->r32();
    cLen = handle->r32();
  }
  return QSharedPointer<Handle>(new Handle(arr));
}
