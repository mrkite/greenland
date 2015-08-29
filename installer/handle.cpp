#include "handle.h"
#include <QFile>

Handle::Handle(QString filename) {
  QFile f(filename);
  if (f.exists() && f.open(QIODevice::ReadOnly)) {
    pos = 0;
    len = f.size();
    bytearray = f.readAll();
    data = (const quint8 *)bytearray.constData();
  } else {
    data = NULL;
  }
}

Handle::Handle(QByteArray array) {
  bytearray = array;
  data = (const quint8 *)bytearray.constData();
  pos = 0;
  len = bytearray.length();
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

quint64 Handle::r64() {
  quint64 r = r32();
  r |= ((quint64)r32()) << 32;
  return r;
}

const char *Handle::read(int len) {
  int oldpos = pos;
  pos += len;
  return (const char *)data + oldpos;
}

const char *Handle::rs() {
  return (const char *)data + pos;
}

const ushort *Handle::rs16() {
  return (const ushort *)data + pos;
}

void Handle::skip(int len) {
  pos += len;
}

void Handle::seek(int offset) {
  pos = offset;
}

qint64 Handle::tell() const {
  return pos;
}

qint64 Handle::length() const {
  return len;
}

