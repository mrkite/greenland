#ifndef VIEW_H
#define VIEW_H

#include <QWidget>

class View : public QWidget {
  Q_OBJECT
public:
  explicit View(QWidget *parent = 0);
  void setWidget(QWidget *widget);

private:
  QWidget *widget;
};

#endif // VIEW_H
