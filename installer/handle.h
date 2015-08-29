#ifndef HANDLE_H
#define HANDLE_H

#include <QByteArray>


class Handle
{
public:
  explicit Handle(QString filename);
  explicit Handle(QByteArray array);

  bool exists() const;
  bool eof() const;
  quint8 r8();
  quint16 r16();
  quint32 r32();
  quint64 r64();
  const char *read(int len);
  const char *rs();
  const ushort *rs16();
  void skip(int len);
  void seek(int offset);
  qint64 length() const;
  qint64 tell() const;

protected:
  const quint8 *data;

private:
  QByteArray bytearray;
  qint64 pos, len;
};

#endif // HANDLE_H
