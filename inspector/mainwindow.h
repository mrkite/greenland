#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>

namespace Ui {
class MainWindow;
}

class FileError {
public:
  FileError(QString reason) : reason(reason) {}
  QString reason;
};

struct Resource {
  QString name;
  quint16 type;
  quint16 bif;
  quint16 tileset;
  quint16 index;
};

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private slots:
  void open();

private:
  void loadKey(QString filename);
  QString root;
  Ui::MainWindow *ui;

  QList<QString> bifs;
  QList<Resource> resources;
};

#endif // MAINWINDOW_H
