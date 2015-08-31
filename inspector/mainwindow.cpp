#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "handle.h"
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>

struct {
  quint16 type;
  const char *extension;
  const char *name;
} categories[] = {
{0x001, "bmp", "Bitmaps"},
{0x002, "mve", "Movies"},
{0x004, "wav", "Audio"},
{0x005, "wfx", "Wave FX"},
{0x006, "plt", "Paper Dolls"},
{0x3e8, "bam", "Animations"},
{0x3e9, "wed", "Maps"},
{0x3ea, "chu", "Controls"},
{0x3eb, "tis", "Tiles"},
{0x3ec, "mos", "GUIs"},
{0x3ed, "itm", "Items"},
{0x3ee, "spl", "Spells"},
{0x3ef, "bcs", "Compiled Scripts"},
{0x3f0, "ids", "IDs"},
{0x3f1, "cre", "Creatures"},
{0x3f2, "are", "Areas"},
{0x3f3, "dlg", "Dialogs"},
{0x3f4, "2da", "Rulesets"},
{0x3f5, "gam", "Save games"},
{0x3f6, "sto", "Stores"},
{0x3f7, "wmp", "World maps"},
{0x3f8, "eff", "Effects"},
{0x3fb, "vvc", "Spell Effects"},
{0x3fd, "pro", "Projectiles"},
{0x000, NULL}
};

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::open() {
  QSettings info;
  QString last = info.value("last", "").toString();
  QString filename = QFileDialog::getOpenFileName(this,
                                                  tr("Open chitin.key"),
                                                  last,
                                                  tr("chitin.key"));
  if (!filename.isEmpty()) {
    QFileInfo fi(filename);
    root = fi.absolutePath();
    info.setValue("last", root);

    try {
      loadKey(filename);
    } catch (FileError e) {
      QMessageBox::warning(this, tr("Failed to load chitin.key"), e.reason);
    }
  }
}

void MainWindow::loadKey(QString filename) {
  QSharedPointer<Handle> handle(new Handle(filename));

  if (!handle->exists())
    throw FileError(tr("%1 doesn't exist").arg(filename));
  if (handle->r4() != "KEY ")
    throw FileError(tr("Not a key file"));
  if (handle->r4() != "V1  ")
    throw FileError(tr("Invalid version"));

  qint32 numBifs = handle->r32();
  qint32 numResources = handle->r32();
  quint32 bifOfs = handle->r32();
  quint32 resOfs = handle->r32();

  handle->seek(bifOfs);
  for (int i = 0; i < numBifs; i++) {
    handle->seek(bifOfs + i * 12 + 4); //skip length
    quint32 nameOfs = handle->r32();
    quint32 nameLen = handle->r16();
    quint16 location = handle->r16();
    handle->seek(nameOfs);
    QString path = "";
    if (location & 0x20) path = "cd4/";
    if (location & 0x10) path = "cd3/";
    if (location & 0x08) path = "cd2/";

    path.append(handle->rs(nameLen));
    path.replace(QRegExp("\\\\"), "/");
    bifs.append(path.toLower());
  }

  handle->seek(resOfs);
  for (int i = 0; i < numResources; i++) {
    Resource res;
    res.name = handle->rs(8);
    res.type = handle->r16();
    quint32 locator = handle->r32();
    res.bif = locator >> 20;
    res.tileset = (locator >> 14) & 0x3f;
    res.index = locator & 0x3fff;
    resources.append(res);
    QString type;
    for (int c = 0; categories[c].name != NULL; c++) {
      if (categories[c].type == res.type) {
        type = categories[c].name;
        break;
      }
    }
    if (type.isEmpty())
      qDebug() << res.name << "." << res.type;
  }
}
