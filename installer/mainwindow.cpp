#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "cab.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QThreadPool>
#include <QProgressDialog>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
  ui(new Ui::MainWindow) {
  ui->setupUi(this);

  QSettings info;
  ui->sourcePath->setText(info.value("source", "").toString());
  ui->destPath->setText(info.value("dest", "").toString());
}

MainWindow::~MainWindow() {
  delete ui;
}

void MainWindow::chooseSource() {
  QString fn = QFileDialog::getExistingDirectory(this,
                                                 tr("Select source data"));
  if (!fn.isEmpty())
    ui->sourcePath->setText(fn);
}

void MainWindow::chooseDest() {
  QString fn = QFileDialog::getExistingDirectory(this,
                                                 tr("Select install folder"));
  if (!fn.isEmpty())
    ui->destPath->setText(fn);
}

void MainWindow::install() {
  QSettings info;
  info.setValue("source", ui->sourcePath->text());
  info.setValue("dest", ui->destPath->text());
  info.sync();

  progress = new QProgressDialog("Installing...", "Abort", 0, 100, this);
  progress->setWindowModality(Qt::WindowModal);

  progress->show();

  try {
    Cab *cab = new Cab(ui->sourcePath->text(), ui->destPath->text());
    connect(cab, SIGNAL(error(QString)),
            this, SLOT(showError(QString)));
    connect(cab, SIGNAL(finished()),
            this, SLOT(done()));
    connect(cab, SIGNAL(progress(QString,int)),
            this, SLOT(update(QString,int)));
    QThreadPool::globalInstance()->start(cab);
  } catch (CabException e) {
    showError(e.reason);
  }
}

void MainWindow::update(QString msg, int percent) {
  progress->setValue(percent);
  progress->setLabelText(msg);
}

void MainWindow::showError(QString err) {
  QMessageBox::warning(this,
                       "Failed to install game",
                       err,
                       QMessageBox::Cancel);
  progress->hide();
}

void MainWindow::done() {
  progress->hide();
  QMessageBox::information(this,
                           "Finished installing",
                           "Installation successful",
                           QMessageBox::Ok);
  close();
}
