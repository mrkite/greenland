#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressDialog>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

private slots:
  void chooseSource();
  void chooseDest();
  void install();
  void done();
  void update(QString msg, int percent);
  void showError(QString err);

private:
  Ui::MainWindow *ui;
  QProgressDialog *progress;
};

#endif // MAINWINDOW_H
