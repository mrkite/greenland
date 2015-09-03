#include "unk.h"
#include <QLabel>
#include <QHBoxLayout>

UNK::UNK(QSharedPointer<Handle>, QWidget *parent) : QWidget(parent) {
  QHBoxLayout *layout = new QHBoxLayout(this);
  setLayout(layout);
  this->layout()->addWidget(new QLabel(tr("Unknown type"), this));
}
