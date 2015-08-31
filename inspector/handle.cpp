#include "handle.h"
#include <QFile>
#include <QDataStream>

Handle::Handle(QString filename) {
  QFile f(filename);
  if (f.exists() && f.open(QIODevice::ReadOnly)) {
    QDataStream stream(&f);
    pos = 0;
    len = f.size();
    bytearray = f.read(len);
    data = (const quint8 *)bytearray.constData();
  } else {
    data = NULL;
  }
}

Handle::Handle(QSharedPointer<Handle> parent, qint64 start, qint64 length) {
  this->parent = parent;
  data = parent->data + start;
  pos = 0;
  len = length;
}

Handle::Handle(QByteArray array) {
  bytearray = array;
  data = (const quint8 *)bytearray.constData();
  pos = 0;
  len = bytearray.size();
}

bool Handle::exists() const {
  return data != NULL;
}

bool Handle::eof() const {
  return pos >= len;
}

quint8 Handle::r8() {
  return data[pos++];
}

quint16 Handle::r16() {
  quint16 r = data[pos++];
  r |= data[pos++] << 8;
  return r;
}

quint32 Handle::r32() {
  quint32 r = data[pos++];
  r |= data[pos++] << 8;
  r |= data[pos++] << 16;
  r |= data[pos++] << 24;
  return r;
}

QString Handle::r4() {
  QString r;
  for (int i = 0; i < 4; i++)
    r += static_cast<char>(data[pos++]);
  return r;
}

QString Handle::rs(qint64 length) {
  QString r;
  for (int i = 0; i < length; i++) {
    quint8 c = data[pos++];
    if (c)
      r += static_cast<char>(c);
  }
  return r;
}

qint64 Handle::tell() const {
  return pos;
}

qint64 Handle::length() const {
  return len;
}

void Handle::seek(qint64 offset) {
  pos = offset;
}

void Handle::skip(qint64 length) {
  pos += length;
}

const char *Handle::read(qint64 length) {
  qint64 oldPos = pos;
  pos += length;
  return (const char *)data + oldPos;
}
