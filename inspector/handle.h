#ifndef HANDLE_H
#define HANDLE_H

#include <QSharedPointer>
#include <QByteArray>

class Handle {
public:
  explicit Handle(QString filename);
  Handle(QSharedPointer<Handle> parent, qint64 start, qint64 length);
  explicit Handle(QByteArray array);

  bool exists() const;
  bool eof() const;
  quint8 r8();
  quint16 r16();
  quint32 r32();
  QString r4();
  const char *read(qint64 length);
  QString rs(qint64 length);
  void skip(qint64 length);
  void seek(qint64 offset);
  qint64 length() const;
  qint64 tell() const;

protected:
  const quint8 *data;

private:
  QByteArray bytearray;
  QSharedPointer<Handle> parent;
  qint64 pos, len;
};

#endif // HANDLE_H
