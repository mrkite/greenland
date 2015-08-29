#ifndef CAB_H
#define CAB_H

#include <QString>
#include <QObject>
#include <QRunnable>
#include <QList>
#include <QMap>


class CabException {
public:
  explicit CabException(QString reason) : reason(reason) {}
  QString reason;
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
    quint8 md5[0x10];
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
    QString name;
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
  void progress(int percent);
  void error(QString err);
  void finished();

protected:
  void run() override;

private:
  void extractFile(int index, QString filename);
  QString source, dest;
  Header header;
};

#endif // CAB_H
