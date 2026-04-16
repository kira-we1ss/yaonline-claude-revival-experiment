#ifndef QWEXTEND_H
#define QWEXTEND_H

#include <QtGlobal>
#include <Qt>

#ifdef Q_OS_LINUX
class QWidget;
void reparent_good(QWidget *that, Qt::WFlags f, bool showIt);
#endif

#endif
