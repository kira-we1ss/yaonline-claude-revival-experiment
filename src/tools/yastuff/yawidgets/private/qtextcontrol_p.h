/****************************************************************************
**
** Qt4→Qt5 compatibility shim.
** QTextControl was renamed to QWidgetTextControl in Qt5 (QtWidgets module).
** This file replaces the vendored Qt4 copy and forwards to the Qt5 header,
** then provides a backward-compatible typedef so existing code keeps
** compiling without further changes.
**
****************************************************************************/

#ifndef QTEXTCONTROL_P_H
#define QTEXTCONTROL_P_H

#include <private/qwidgettextcontrol_p.h>

QT_BEGIN_NAMESPACE

// Backward-compat alias: Qt4 called it QTextControl; Qt5 calls it QWidgetTextControl.
typedef QWidgetTextControl QTextControl;

QT_END_NAMESPACE

#endif // QTEXTCONTROL_P_H
