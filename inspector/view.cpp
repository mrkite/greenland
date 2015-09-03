#include "view.h"
#include <QHBoxLayout>

View::View(QWidget *parent) : QWidget(parent) {
  widget = 0;
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
}

void View::setWidget(QWidget *widget) {
  if (this->widget) {
    delete layout()->takeAt(0);
    this->widget->deleteLater();
  }
  this->widget = widget;
  layout()->addWidget(this->widget);
}
