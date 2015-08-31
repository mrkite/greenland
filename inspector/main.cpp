#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  a.setApplicationName("Inspector");
  a.setApplicationVersion("1.0");
  a.setOrganizationName("seancode");

  MainWindow w;
  w.show();

  return a.exec();
}
