#ifndef CAB_H
#define CAB_H

#include <QString>
#include <QObject>
#include <QRunnable>
#include <QList>
#include <QMap>
#include <QSharedPointer>
#include "handle.h"


class CabException {
public:
  explicit CabException(QString reason) : reason(reason) {}
  QString reason;
};

class Cabinet {
public:
  Cabinet(QString base);
  void seek(int volume, int index, int offset, bool obfuscated);
  QByteArray read(int len);
private:
  void open();
  qint32 firstIndex, lastIndex;
  qint64 firstOffset, lastOffset;
  qint64 firstSize, lastSize;
  qint64 firstCompressed, lastCompressed;
  QString base;
  int curIndex;
  int curVolume;
  bool unobfuscate;
  qint64 end;
  quint8 obOffs;
  QSharedPointer<Handle> handle;
};

class Cab : public QObject, public QRunnable {
  Q_OBJECT

  class File {
  public:
    QString name;
    QString directory;
    quint16 flags;
    qint64 size;
    qint64 compressed;
    qint64 offset;
    qint8 md5[0x10];
    quint32 previous, next;
    quint8 link;
    quint16 volume;
  };

  class Group {
  public:
    QString name, source, dest;
    qint32 first, last;
  };

  class Component {
  public:
    QString name, dest;
    QList<QString> groups;
  };

  class Header {
  public:
    Header(QString path);
    quint32 version;
    QList<Component> components;
    QMap<QString, Group> groups;
    QList<File> files;
  };

public:
  Cab(QString source, QString dest);
  virtual ~Cab();

signals:
  void progress(QString message, int percent);
  void error(QString err);
  void finished();

protected:
  void run() override;

private:
  void extractFile(int index, QString filename);
  QString source, dest;
  Header header;
  Cabinet cab;
};

#endif // CAB_H
