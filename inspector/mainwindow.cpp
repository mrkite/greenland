#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "handle.h"
#include "bif.h"
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
#include "unk.h"
#include "bmp.h"
#include "bam.h"
#include "mos.h"

template <typename T>
static QWidget *createView(QSharedPointer<Handle> handle, QWidget *parent = 0) {
  return new T(handle, parent);
}

typedef QWidget *(catFunc)(QSharedPointer<Handle> handle, QWidget *parent);

struct {
  quint16 type;
  const char *extension;
  const char *name;
  catFunc *func;
} categories[] = {
{0x001, "bmp", "Bitmaps", createView<BMP>},
{0x002, "mve", "Movies", createView<UNK>},
{0x004, "wav", "Audio", createView<UNK>},
{0x005, "wfx", "Wave FX", createView<UNK>},
{0x006, "plt", "Paper Dolls", createView<UNK>},
{0x3e8, "bam", "Animations", createView<BAM>},
{0x3e9, "wed", "Maps", createView<UNK>},
{0x3ea, "chu", "Controls", createView<UNK>},
{0x3eb, "tis", "Tiles", createView<UNK>},
{0x3ec, "mos", "GUIs", createView<MOS>},
{0x3ed, "itm", "Items", createView<UNK>},
{0x3ee, "spl", "Spells", createView<UNK>},
{0x3ef, "bcs", "Compiled Scripts", createView<UNK>},
{0x3f0, "ids", "IDs", createView<UNK>},
{0x3f1, "cre", "Creatures", createView<UNK>},
{0x3f2, "are", "Areas",createView<UNK>},
{0x3f3, "dlg", "Dialogs", createView<UNK>},
{0x3f4, "2da", "Rulesets", createView<UNK>},
{0x3f5, "gam", "Save games", createView<UNK>},
{0x3f6, "sto", "Stores", createView<UNK>},
{0x3f7, "wmp", "World maps", createView<UNK>},
{0x3f8, "eff", "Effects", createView<UNK>},
{0x3fb, "vvc", "Spell Effects", createView<UNK>},
{0x3fd, "pro", "Projectiles", createView<UNK>},
{0x000, NULL, NULL, NULL}
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

void MainWindow::keySelected() {
  auto item = ui->treeWidget->selectedItems().first();
  int id = item->data(0, Qt::UserRole).toInt();

  if (id >= 0) {
    try {
      QWidget *w = NULL;
      Resource &res = resources[id];

      QString path = bifs[res.bif];
      QDir dir(root);
      BIF bif(dir.absoluteFilePath(path));

      auto handle = bif.get(res.tileset, res.index);

      for (int c = 0; categories[c].name != NULL; c++) {
        if (categories[c].type == res.type) {
          w = categories[c].func(handle, ui->view);
          break;
        }
      }
      if (w != NULL)
        ui->view->setWidget(w);
      else
        ui->view->setWidget(createView<UNK>(handle, ui->view));
    } catch (FileError e) {
      QMessageBox::warning(this, tr("Failed to load BIF"), e.reason);
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

  QHash<QString, QTreeWidgetItem *> parents;

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
    QString type = "unknown";
    for (int c = 0; categories[c].name != NULL; c++) {
      if (categories[c].type == res.type) {
        type = categories[c].name;
        break;
      }
    }

    if (!parents.contains(type)) {
      QStringList row;
      row << type;
      QTreeWidgetItem *p = new QTreeWidgetItem(row);
      p->setData(0, Qt::UserRole, -1);
      ui->treeWidget->addTopLevelItem(p);
      parents[type] = p;
    }

    QStringList row;
    row << res.name;
    QTreeWidgetItem *c = new QTreeWidgetItem(parents[type], row);
    c->setData(0, Qt::UserRole, i);
    parents[type]->addChild(c);
  }

  ui->treeWidget->sortItems(1, Qt::AscendingOrder);
  ui->treeWidget->sortItems(0, Qt::AscendingOrder);
}
